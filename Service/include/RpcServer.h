// RpcServer.h
#pragma once
#include "pch.h"

class RpcServer
{
public:
    RpcServer();
    ~RpcServer();

    bool Start();
    void Stop();
    void SetStopEvent(HANDLE hStopEvent);

private:
    static DWORD WINAPI RpcThreadProc(LPVOID lpParam);
    void RpcThread();
    HANDLE m_hThread;
    std::atomic<bool> m_running;
    HANDLE m_hStopEvent;
};