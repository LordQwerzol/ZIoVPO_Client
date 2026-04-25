// RpcServer.cpp
#include "pch.h"
#include "RpcServer.h"
#include "ServiceRpc.h"

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