// UiClientManager.h
#pragma once
#include "pch.h"
#include <string>
#include <vector>
#include <mutex>

// Управляет запуском графического приложения во всех активных сессиях (кроме 0)
// и при новых входах пользователей. Сохраняет дескрипторы запущенных процессов
// в векторе, чтобы при остановке службы завершить их все.
class UiClientManager
{
public:
    // clientPath – полный путь к запускаемому GUI-приложению.
    explicit UiClientManager(const std::wstring& clientPath);
    ~UiClientManager();

    // Запускает приложение в конкретной сессии от имени владельца этой сессии.
    // Окно приложения скрывается (SW_HIDE).
    void LaunchProcessForSession(DWORD sessionId);

    // Перебирает все активные сессии (кроме сессии 0) и запускает приложение в каждой.
    void LaunchUiInAllSessions();

    // Завершает все ранее запущенные клиентские процессы и очищает вектор.
    void TerminateAllClients();

private:
    // Внутренний метод, выполняющий CreateProcessAsUser с переданным токеном.
    // Возвращает true при успехе, иначе false.
    bool LaunchProcessAsUser(HANDLE hToken, DWORD sessionId);

    // Удаляет из вектора дескрипторы уже завершившихся процессов (чтобы не было утечек).
    void CleanupDeadProcesses();

private:
    std::wstring            m_clientPath;
    std::vector<HANDLE>     m_clients;      // дескрипторы запущенных процессов
    std::mutex              m_mutex;        // синхронизация доступа к вектору
};