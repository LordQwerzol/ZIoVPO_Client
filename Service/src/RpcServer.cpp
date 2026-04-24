// RpcServer.cpp
#include "pch.h"
#include "RpcServer.h"
#include "ServiceRpc.h"
#include "AuthManager.h"
#include "LicenseManager.h"

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
