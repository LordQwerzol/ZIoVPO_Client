// WinService.h
#pragma once
#include "pch.h"
#include "RpcServer.h"
#include "UiClientManager.h"
#include "AuthManager.h"
#include "LicenseManager.h"
#include "HttpClient.h"

class WinService
{
public:
    // Синглтон
    static WinService& GetInstance();

    // Установка / удаление службы 
    bool Install(const std::wstring& serviceName,
                 const std::wstring& displayName,
                 const std::wstring& appPath);
    bool Uninstall(const std::wstring& serviceName);

    // Точка входа службы (вызывается SCM в потоке ServiceMain)
    void Run();

    // Статические callback-функции для SCM
    static VOID WINAPI ServiceMain(DWORD dwArgc, LPWSTR* lpszArgv);
    static DWORD WINAPI ServiceCtrlHandlerEx(DWORD dwCtrl, DWORD dwEventType,
                                             LPVOID lpEventData, LPVOID lpContext);

private:
    WinService();
    ~WinService();

    // Инициализация и остановка внутренних компонентов
    bool Initialize();
    void Shutdown();

    // Вспомогательный метод для обновления статуса службы в SCM
    void SetServiceStatus(DWORD dwState, DWORD dwWin32ExitCode = NO_ERROR, DWORD dwWaitHint = 0);

private:
    std::wstring GetClientAppPath() const;

    SERVICE_STATUS_HANDLE   m_hStatusHandle;
    SERVICE_STATUS          m_ServiceStatus;
    HANDLE                  m_hStopEvent;

    std::unique_ptr<RpcServer>       m_pRpcServer;
    std::unique_ptr<UiClientManager> m_pUiClientManager;
};