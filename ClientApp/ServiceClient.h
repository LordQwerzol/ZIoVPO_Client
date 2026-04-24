#pragma once
#include <windows.h>
#include <string>

class ServiceClient {
public:
    // Проверка, запущена ли служба
    static bool IsRunning();

    // Запуск службы (если не запущена) и ожидание состояния SERVICE_RUNNING
    // Возвращает true, если служба запущена (или уже была запущена)
    static bool StartAndWait();

    // Остановка службы через RPC
    static bool StopService();

    static int GetCurrentUser(std::wstring& outUsername, std::wstring& errorMessage);
    static int GetLicenseStatus(std::wstring& outStatus, std::wstring& outExpirationDate, std::wstring& errorMessage);
    static int Login(const std::wstring& username, const std::wstring& password, std::wstring& outUsername, std::wstring& errorMessage);
    static void Logout();
    static int ActivateProduct(const std::wstring& activationCode, std::wstring& outStatus, std::wstring& outExpirationDate, std::wstring& errorMessage);

private:
    static bool WaitForServiceState(DWORD desiredState, DWORD timeoutMs = 30000);
};