// main.cpp
#include "WinService.h"
#include <iostream>

static const std::wstring SERVICE_NAME = L"ZIoVPOService";
static const std::wstring DISPLAY_NAME = L"ZIoVPO Client Service";
static const std::wstring CLIENT_EXE_NAME = L"ClientApp.exe";

int wmain(int argc, wchar_t* argv[])
{
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    std::wstring servicePath(modulePath);
    size_t lastSlash = servicePath.find_last_of(L"\\/");
    std::wstring directory = servicePath.substr(0, lastSlash + 1);
    std::wstring clientPath = directory + CLIENT_EXE_NAME;

    if (argc == 2) {
        std::wstring cmd = argv[1];
        if (cmd == L"--install") {
            if (GetFileAttributesW(clientPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
                return 1;
            }
            if (WinService::GetInstance().Install(SERVICE_NAME, DISPLAY_NAME, clientPath))
                return 0;
            else
                return 1;
        }
        else if (cmd == L"--uninstall") {
            if (WinService::GetInstance().Uninstall(SERVICE_NAME))
                return 0;
            else
                return 1;
        }
    }

    // Нормальный запуск как службы
    SERVICE_TABLE_ENTRYW dispatchTable[] = {
        { L"ZIoVPOService", WinService::ServiceMain },
        { nullptr, nullptr }
    };
    StartServiceCtrlDispatcherW(dispatchTable);

    return 0;
}