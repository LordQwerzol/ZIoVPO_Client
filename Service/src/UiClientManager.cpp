#include "UiClientManager.h"

// Конструктор
UiClientManager::UiClientManager(const std::wstring& clientPath): m_clientPath(clientPath) {}
// Деструктор – завершаем все процессы, если не сделано явно
UiClientManager::~UiClientManager() {TerminateAllClients(); }
// Запуск приложения в конкретной сессии
void UiClientManager::LaunchProcessForSession(DWORD sessionId)
{
    if (sessionId == 0) {
        return;
    }
    CleanupDeadProcesses();
    HANDLE hToken = nullptr;
    if (!WTSQueryUserToken(sessionId, &hToken)) {
        DWORD err = GetLastError();
        return;
    }
    HANDLE hPrimaryToken = nullptr;
    if (!DuplicateTokenEx(hToken, TOKEN_ALL_ACCESS, nullptr, SecurityImpersonation, TokenPrimary, &hPrimaryToken)) {
        DWORD err = GetLastError();
        CloseHandle(hToken);
        return;
    }
    CloseHandle(hToken);
    LaunchProcessAsUser(hPrimaryToken, sessionId);
    CloseHandle(hPrimaryToken);
}
// Запуск во всех существующих активных сессиях
void UiClientManager::LaunchUiInAllSessions() {
    CleanupDeadProcesses();
    PWTS_SESSION_INFO pSessionInfo = nullptr;
    DWORD sessionCount = 0;
    if (!WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &sessionCount)) {
        return;
    }
    for (DWORD i = 0; i < sessionCount; ++i) {
        DWORD sessionId = pSessionInfo[i].SessionId;
        if (sessionId != 0) {
            LaunchProcessForSession(sessionId);
        }
    }
    WTSFreeMemory(pSessionInfo);
}
// Завершение всех клиентских процессов
void UiClientManager::TerminateAllClients() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (HANDLE hProcess : m_clients) {
        if (hProcess && hProcess != INVALID_HANDLE_VALUE)
        {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
    }
    m_clients.clear();
}
// Внутренний метод: запуск процесса с использованием первичного токена
bool UiClientManager::LaunchProcessAsUser(HANDLE hToken, DWORD sessionId) {
    // Формируем командную строку: путь к приложению + аргумент --show_hidden
    std::wstring commandLine = L"\"" + m_clientPath + L"\" --show_hidden";
    std::vector<wchar_t> cmdLineBuffer(commandLine.begin(), commandLine.end());
    cmdLineBuffer.push_back(0);
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    BOOL bResult = CreateProcessAsUserW(
        hToken,
        m_clientPath.c_str(),
        cmdLineBuffer.data(),
        nullptr, nullptr,
        FALSE,
        0,          // никаких специальных флагов
        nullptr, nullptr,
        &si,
        &pi
    );

    if (!bResult) {
        DWORD err = GetLastError();
        return false;
    }
    CloseHandle(pi.hThread); {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clients.push_back(pi.hProcess);
    }
    return true;
}
// Удаление из вектора дескрипторов уже завершившихся процессов
void UiClientManager::CleanupDeadProcesses() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_clients.begin();
    while (it != m_clients.end()) {
        HANDLE h = *it;
        if (h == nullptr || h == INVALID_HANDLE_VALUE) {
            it = m_clients.erase(it);
            continue;
        }
        // Проверяем, жив ли процесс
        DWORD waitResult = WaitForSingleObject(h, 0);
        if (waitResult == WAIT_OBJECT_0) {
            // Процесс завершился – закрываем дескриптор и удаляем из вектора
            CloseHandle(h);
            it = m_clients.erase(it);
        }
        else {
            ++it;
        }
    }
}