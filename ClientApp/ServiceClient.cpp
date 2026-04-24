#include "ServiceClient.h"
#include <winsvc.h>
#include <rpcdce.h>
#include <vector>

extern "C" {
    #include "ServiceRpc.h"
}

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "rpcrt4.lib")

static const wchar_t* SERVICE_NAME = L"ZIoVPOService";
static const wchar_t* RPC_ENDPOINT = L"ServiceRpcEndpoint";

// Реализация функций выделения памяти для RPC (MIDL)
void* __RPC_USER MIDL_user_allocate(size_t size) {
    return malloc(size);
}

void __RPC_USER MIDL_user_free(void* ptr) {
    free(ptr);
}

bool ServiceClient::IsRunning() {
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return false;
    SC_HANDLE hService = OpenServiceW(hSCM, SERVICE_NAME, SERVICE_QUERY_STATUS);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return false;
    }
    SERVICE_STATUS_PROCESS ss;
    DWORD bytesNeeded;
    bool running = false;
    if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ss, sizeof(ss), &bytesNeeded)) {
        running = (ss.dwCurrentState == SERVICE_RUNNING);
    }
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return running;
}

bool ServiceClient::WaitForServiceState(DWORD desiredState, DWORD timeoutMs) {
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return false;
    SC_HANDLE hService = OpenServiceW(hSCM, SERVICE_NAME, SERVICE_QUERY_STATUS);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return false;
    }
    DWORD startTime = GetTickCount();
    bool reached = false;
    while (!reached && (GetTickCount() - startTime) < timeoutMs) {
        SERVICE_STATUS_PROCESS ss;
        DWORD bytesNeeded;
        if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ss, sizeof(ss), &bytesNeeded)) {
            if (ss.dwCurrentState == desiredState) {
                reached = true;
                break;
            }
        }
        Sleep(500);
    }
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return reached;
}

bool ServiceClient::StartAndWait() {
    if (IsRunning()) return true;
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return false;
    SC_HANDLE hService = OpenServiceW(hSCM, SERVICE_NAME, SERVICE_START);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return false;
    }
    bool started = StartServiceW(hService, 0, nullptr);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    if (!started) return false;
    return WaitForServiceState(SERVICE_RUNNING);
}

bool ServiceClient::StopService() {
    RPC_WSTR pszStringBinding = nullptr;
    RPC_STATUS status = RpcStringBindingComposeW(
        nullptr, (RPC_WSTR)L"ncalrpc", nullptr,
        (RPC_WSTR)RPC_ENDPOINT, nullptr, &pszStringBinding);
    if (status != RPC_S_OK) {
        return false;
    }

    RPC_BINDING_HANDLE hBinding = nullptr;
    status = RpcBindingFromStringBindingW(pszStringBinding, &hBinding);
    RpcStringFreeW(&pszStringBinding);
    if (status != RPC_S_OK) {
        return false;
    }

    // Присваиваем глобальному дескриптору
    extern RPC_BINDING_HANDLE ServiceRpc_BindingHandle;
    ServiceRpc_BindingHandle = hBinding;

    // Вызов удалённой процедуры
    ::StopService();

    // Освобождаем ресурсы
    RpcBindingFree(&hBinding);
    ServiceRpc_BindingHandle = nullptr;
    return true;
}

static RPC_BINDING_HANDLE GetRpcBinding() {
    RPC_WSTR pszStringBinding = nullptr;
    RPC_STATUS status = RpcStringBindingComposeW(nullptr, (RPC_WSTR)L"ncalrpc", nullptr,
                                                  (RPC_WSTR)RPC_ENDPOINT, nullptr, &pszStringBinding);
    if (status != RPC_S_OK) return nullptr;
    RPC_BINDING_HANDLE hBinding = nullptr;
    status = RpcBindingFromStringBindingW(pszStringBinding, &hBinding);
    RpcStringFreeW(&pszStringBinding);
    return (status == RPC_S_OK) ? hBinding : nullptr;
}

int ServiceClient::GetCurrentUser(std::wstring& outUsername, std::wstring& errorMessage) {
    RPC_BINDING_HANDLE hBinding = GetRpcBinding();
    if (!hBinding) return -1;

    AuthInfo result;
    int ret = ::GetCurrentUser(hBinding, &result);
    RpcBindingFree(&hBinding);

    if (ret == 0) {
        outUsername = result.username;
        return 0;
    } else {
        errorMessage = result.errorMessage;
        return -1;
    }
}

int ServiceClient::GetLicenseStatus(std::wstring& outStatus, std::wstring& outExpirationDate, std::wstring& errorMessage) {
    RPC_BINDING_HANDLE hBinding = GetRpcBinding();
    if (!hBinding) return -1;

    LicenseInfo result;
    int ret = ::GetLicenseStatus(hBinding, &result);
    RpcBindingFree(&hBinding);

    if (ret == 0) {
        outStatus = result.status;
        outExpirationDate = result.expirationDate;
        return 0;
    } else {
        errorMessage = result.errorMessage;
        return -1;
    }
}

int ServiceClient::Login(const std::wstring& username, const std::wstring& password,
                         std::wstring& outUsername, std::wstring& errorMessage) {
    RPC_BINDING_HANDLE hBinding = GetRpcBinding();
    if (!hBinding) return -1;

    AuthInfo result;
    int ret = ::Login(hBinding, username.c_str(), password.c_str(), &result);
    RpcBindingFree(&hBinding);

    if (ret == 0) {
        outUsername = result.username;
        return 0;
    } else {
        errorMessage = result.errorMessage;
        return -1;
    }
}

void ServiceClient::Logout() {
    RPC_BINDING_HANDLE hBinding = GetRpcBinding();
    if (!hBinding) return;
    ::Logout(hBinding);
    RpcBindingFree(&hBinding);
}

int ServiceClient::ActivateProduct(const std::wstring& activationCode,
                                   std::wstring& outStatus, std::wstring& outExpirationDate,
                                   std::wstring& errorMessage) {
    RPC_BINDING_HANDLE hBinding = GetRpcBinding();
    if (!hBinding) return -1;

    LicenseInfo result;
    int ret = ::ActivateProduct(hBinding, activationCode.c_str(), &result);
    RpcBindingFree(&hBinding);

    if (ret == 0) {
        outStatus = result.status;
        outExpirationDate = result.expirationDate;
        return 0;
    } else {
        errorMessage = result.errorMessage;
        return -1;
    }
}