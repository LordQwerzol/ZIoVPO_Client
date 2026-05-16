// RpcServer.cpp
#include "pch.h"
#include "RpcServer.h"
#include "ServiceRpc.h"
#include "AuthManager.h"
#include "LicenseManager.h"
#include "DatabaseLoader.h"
#include "Scanner.h"
#include "Utils.h"

static std::wstring normalizePath(const std::wstring& path) {
    std::wstring result = path;
    for (wchar_t& ch : result) {
        if (ch == L'/') ch = L'\\';
    }
    return result;
}


static HANDLE g_hStopEvent = nullptr;

void StopService() {
    if (g_hStopEvent)
        SetEvent(g_hStopEvent);
}

RpcServer::RpcServer(): m_hThread(nullptr), m_running(false), m_hStopEvent(nullptr) {}

RpcServer::~RpcServer() { Stop(); }

void RpcServer::SetStopEvent(HANDLE hStopEvent) {
    m_hStopEvent = hStopEvent;
    g_hStopEvent = hStopEvent;
}

bool RpcServer::Start() {
    if (m_running) return false;
    m_running = true;
    m_hThread = CreateThread(nullptr, 0, RpcThreadProc, this, 0, nullptr);
    return (m_hThread != nullptr);
}

void RpcServer::Stop() {
    if (!m_running) return;
    m_running = false;
    RpcMgmtStopServerListening(nullptr);
    RpcServerUnregisterIf(nullptr, nullptr, false);
    if (m_hThread) {
        WaitForSingleObject(m_hThread, 5000);
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }
}

DWORD WINAPI RpcServer::RpcThreadProc(LPVOID lpParam) {
    RpcServer* pThis = static_cast<RpcServer*>(lpParam);
    pThis->RpcThread();
    return 0;
}

void RpcServer::RpcThread() {
    RPC_STATUS status;
    // Используем широкие строки для ncalrpc
    status = RpcServerUseProtseqEpW(
        (RPC_WSTR)L"ncalrpc",
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
        (RPC_WSTR)L"ServiceRpcEndpoint",
        nullptr);
    if (status != RPC_S_OK) return;

    status = RpcServerRegisterIf(
        ServiceRpc_v1_0_s_ifspec,
        nullptr, nullptr);
    if (status != RPC_S_OK) return;

    status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, FALSE);
    if (status != RPC_S_OK) return;
}

extern "C" int Login(
    handle_t hBinding,
    const wchar_t* username,
    const wchar_t* password,
    AuthInfo* result)
{
    ZeroMemory(result, sizeof(AuthInfo));

    std::wstring error;
    if (AuthManager::GetInstance().Login(username, password, error)) {
        wcscpy_s(result->username, 256, AuthManager::GetInstance().GetCurrentUsername().c_str());
        result->errorMessage[0] = L'\0';
        return 0;
    } else {
        wcscpy_s(result->errorMessage, 1024, error.c_str());
        return -1;
    }
}

extern "C" void Logout(handle_t hBinding)
{
    AuthManager::GetInstance().Logout();
    LicenseManager::GetInstance().ClearLicense();
}

extern "C" int GetCurrentUser(
    handle_t hBinding,
    AuthInfo* result)
{
    ZeroMemory(result, sizeof(AuthInfo));

    if (!AuthManager::GetInstance().IsAuthenticated()) {
        wcscpy_s(result->errorMessage, 1024, L"Not authenticated");
        return -1;
    }
    wcscpy_s(result->username, 256, AuthManager::GetInstance().GetCurrentUsername().c_str());
    return 0;
}

extern "C" int GetLicenseStatus(
    handle_t hBinding,
    LicenseInfo* result)
{
    ZeroMemory(result, sizeof(LicenseInfo));

    if (!AuthManager::GetInstance().IsAuthenticated()) {
        wcscpy_s(result->status, 64, L"NOT_AUTHENTICATED");
        wcscpy_s(result->errorMessage, 1024, L"User not authenticated");
        return -1;
    }

    std::wstring error;
    if (!LicenseManager::GetInstance().CheckLicenseStatus(error)) {
        wcscpy_s(result->status, 64, L"ERROR");
        wcscpy_s(result->errorMessage, 1024, error.c_str());
        return -1;
    }

    auto ticket = LicenseManager::GetInstance().GetCurrentLicense();
    wcscpy_s(result->status, 64, ticket.status.c_str());
    wcscpy_s(result->expirationDate, 32, ticket.expirationDate.c_str());
    return 0;
}

extern "C" int ActivateProduct(
    handle_t hBinding,
    const wchar_t* activationCode,
    LicenseInfo* result)
{
    ZeroMemory(result, sizeof(LicenseInfo));

    if (!AuthManager::GetInstance().IsAuthenticated()) {
        wcscpy_s(result->status, 64, L"NOT_AUTHENTICATED");
        wcscpy_s(result->errorMessage, 1024, L"User not authenticated");
        return -1;
    }

    std::wstring error;
    if (!LicenseManager::GetInstance().ActivateProduct(activationCode, error)) {
        wcscpy_s(result->status, 64, L"ACTIVATION_FAILED");
        wcscpy_s(result->errorMessage, 1024, error.c_str());
        return -1;
    }

    LicenseManager::GetInstance().CheckLicenseStatus(error);
    auto ticket = LicenseManager::GetInstance().GetCurrentLicense();
    wcscpy_s(result->status, 64, ticket.status.c_str());
    wcscpy_s(result->expirationDate, 32, ticket.expirationDate.c_str());
    return 0;
}


int GetDatabaseInfo(
    handle_t hBinding,
    DatabaseInfo* result)
{
    wcscpy_s(result->errorMessage, L""); // очищаем буфер

    auto& loader = DatabaseLoader::instance();
    if (!loader.isLoaded()) {
        wcscpy_s(result->errorMessage, L"Antivirus database not loaded.");
        return 1;
    }

    result->timestamp = loader.getTimestamp();
    result->recordCount = loader.getRecordCount();

    return 0;
}

int ScanPath(handle_t hBinding, const wchar_t* path, ScanResultRpc* result) {
    wcscpy_s(result->errorMessage, 256, L"");
    result->threatsCount = 0;
    result->threats = nullptr;

    auto& loader = DatabaseLoader::instance();
    if (!loader.isLoaded()) {
        wcscpy_s(result->errorMessage, 256, L"Antivirus database not loaded.");
        return 1;
    }
    if (!path || wcslen(path) == 0) {
        wcscpy_s(result->errorMessage, 256, L"Empty path provided.");
        return 2;
    }
    DWORD attr = GetFileAttributesW(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        wcscpy_s(result->errorMessage, 256, L"Path does not exist.");
        return 3;
    }

    ObjectType expectedType = ObjectType::UNKNOWN;

    std::vector<ThreatInfo> threats;
    bool isFolder = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    std::wstring normalizedPath = normalizePath(path);
    if (!isFolder) {
        Scanner::instance().scanFile(std::wstring(normalizedPath), expectedType, threats);
    } else {
        Scanner::instance().scanFolder(std::wstring(normalizedPath), expectedType, threats);
    }

    // Заполняем RPC-структуру результата
    result->threatsCount = static_cast<unsigned long>(threats.size());
    if (result->threatsCount > 0) {
        size_t totalSize = result->threatsCount * sizeof(ThreatInfoRpc);
        result->threats = (ThreatInfoRpc*)MIDL_user_allocate(totalSize);
        ZeroMemory(result->threats, totalSize);

        for (size_t i = 0; i < threats.size(); ++i) {
            const auto& t = threats[i];

            // filePath (уже wstring -> wchar_t*)
            size_t lenPath = t.filePath.size() + 1;
            wchar_t* wp = (wchar_t*)MIDL_user_allocate(lenPath * sizeof(wchar_t));
            wcscpy_s(wp, lenPath, t.filePath.c_str());

            // threatName (string -> wstring -> wchar_t*)
            std::wstring wThreatName = t.threatName;
            size_t lenName = wThreatName.size() + 1;
            wchar_t* np = (wchar_t*)MIDL_user_allocate(lenName * sizeof(wchar_t));
            wcscpy_s(np, lenName, wThreatName.c_str());

            // objectTypeString ( wstring -> wchar_t*)
            std::wstring wObjType = t.objectTypeString;
            size_t lenObj = wObjType.size() + 1;
            wchar_t* op = (wchar_t*)MIDL_user_allocate(lenObj * sizeof(wchar_t));
            wcscpy_s(op, lenObj, wObjType.c_str());

            result->threats[i].filePath = wp;
            result->threats[i].threatName = np;
            result->threats[i].objectTypeString = op;
        }
    }
    return 0;
}

