#pragma once
#include <windows.h>

class ServiceClient {
public:
    // Проверка, запущена ли служба
    static bool IsRunning();

    // Запуск службы (если не запущена) и ожидание состояния SERVICE_RUNNING
    // Возвращает true, если служба запущена (или уже была запущена)
    static bool StartAndWait();

    // Остановка службы через RPC
    static bool StopService();

private:
    static bool WaitForServiceState(DWORD desiredState, DWORD timeoutMs = 30000);
};