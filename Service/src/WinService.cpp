// WinService.cpp
#include "WinService.h"
#include <winsvc.h>
#include <aclapi.h>
#include <cstdlib>  

void* __RPC_USER MIDL_user_allocate(size_t size)
{
    return malloc(size);
}

void __RPC_USER MIDL_user_free(void* ptr)
{
    free(ptr);
}

static WinService* g_pWinService = nullptr;

// Установка службы 
bool WinService::Install(const std::wstring& serviceName, const std::wstring& displayName, const std::wstring& appPath) {
    wchar_t szPath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, szPath, MAX_PATH))
        return false;

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) return false;

    std::wstring commandLine = L"\"" + std::wstring(szPath) + L"\" \"" + appPath + L"\"";
    SC_HANDLE hService = CreateServiceW(
        hSCM, serviceName.c_str(), displayName.c_str(),
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        commandLine.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr);
    bool success = (hService != nullptr);
    if (hService) CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return success;
}

// Удаление 
bool WinService::Uninstall(const std::wstring& serviceName)
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return false;
    SC_HANDLE hService = OpenServiceW(hSCM, serviceName.c_str(), DELETE);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return false;
    }
    bool success = DeleteService(hService);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return success;
}

// Обертка для обработки сигналов из SCM -> StartServiceCtrlDispatcherW
void WinService::Run()
{
    SERVICE_TABLE_ENTRYW dispatchTable[] = {
        { const_cast<LPWSTR>(L"ZIoVPOService"), ServiceMain },
        { nullptr, nullptr }
    };
    StartServiceCtrlDispatcherW(dispatchTable);
}

VOID WINAPI WinService::ServiceMain(DWORD dwArgc, LPWSTR* lpszArgv)
{
    WinService& service = WinService::GetInstance();
    g_pWinService = &service;
    service.m_hStatusHandle = RegisterServiceCtrlHandlerExW(L"ZIoVPOService", ServiceCtrlHandlerEx, nullptr);
    if (!service.m_hStatusHandle) return;

    service.SetServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    if (!service.Initialize()) {
        service.SetServiceStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR);
        return;
    }

    service.SetServiceStatus(SERVICE_RUNNING);

    WaitForSingleObject(service.m_hStopEvent, INFINITE);

    service.Shutdown();
    service.SetServiceStatus(SERVICE_STOPPED);
}

DWORD WINAPI WinService::ServiceCtrlHandlerEx(DWORD dwCtrl, DWORD dwEventType,
                                              LPVOID lpEventData, LPVOID lpContext)
{
    WinService* pService = g_pWinService;
    if (!pService) return ERROR_CALL_NOT_IMPLEMENTED;

    switch (dwCtrl) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        return NO_ERROR; 

    case SERVICE_CONTROL_INTERROGATE: // опрос состояния
        pService->SetServiceStatus(pService->m_ServiceStatus.dwCurrentState);
        return NO_ERROR;

    case SERVICE_CONTROL_SESSIONCHANGE: // cмена сессии пользователя
        if (dwEventType == WTS_SESSION_LOGON) {
            WTSSESSION_NOTIFICATION* pSession = (WTSSESSION_NOTIFICATION*)lpEventData;
            if (pSession && pSession->dwSessionId != 0)
                pService->m_pUiClientManager->LaunchProcessForSession(pSession->dwSessionId);
        }
        return NO_ERROR;

    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

// Инцилизация объектов класса
bool WinService::Initialize()
{
    m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!m_hStopEvent) { return false; }

    m_pUiClientManager = std::make_unique<UiClientManager>(GetClientAppPath());

    m_pRpcServer = std::make_unique<RpcServer>();
    m_pRpcServer->SetStopEvent(m_hStopEvent);

    if (!m_pRpcServer->Start()) { return false; }

    // Запуск UI во всех существующих сессиях (при старте службы)
    m_pUiClientManager->LaunchUiInAllSessions();
    return true;
}

// Удаление всех объектов
void WinService::Shutdown()
{
    AuthManager::GetInstance().StopRefreshThread();
    LicenseManager::GetInstance().StopRefreshThread();
    if (m_pRpcServer) m_pRpcServer->Stop();
    if (m_pUiClientManager) m_pUiClientManager->TerminateAllClients();
    if (m_hStopEvent) CloseHandle(m_hStopEvent);
}


// ----->  СЛУЖЕБНОЕ <----- \\ 
// Конструктор
WinService::WinService(): m_hStatusHandle(nullptr) , m_hStopEvent(nullptr) {
    ZeroMemory(&m_ServiceStatus, sizeof(m_ServiceStatus));
    m_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    m_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    m_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_SESSIONCHANGE;
}

// Деконструктор 
WinService::~WinService() {
    Shutdown();
}

// Сингтон
WinService& WinService::GetInstance() {
    static WinService instance;
    return instance;
}

// Установка статуса службы
void WinService::SetServiceStatus(DWORD dwState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
    if (!m_hStatusHandle) return;
    m_ServiceStatus.dwCurrentState = dwState;
    m_ServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
    m_ServiceStatus.dwWaitHint = dwWaitHint;
    ::SetServiceStatus(m_hStatusHandle, &m_ServiceStatus);
}

// Получение пути к UI
std::wstring WinService::GetClientAppPath() const
{
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    std::wstring servicePath(modulePath);
    size_t lastSlash = servicePath.find_last_of(L"\\/");
    std::wstring directory = servicePath.substr(0, lastSlash + 1);
    return directory + L"ClientApp.exe";
}