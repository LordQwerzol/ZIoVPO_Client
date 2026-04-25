#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>

#include "mainwindow.h"
#include "traymanager.h"
#include "TaskbarCreatedFilter.h"
#include "ServiceClient.h"

// Одиночный запуск через мьютекс (на сессию)
static bool checkSingleInstance() {
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\ZIoVPOClientUIMutex");
    if (mutex == NULL)
        return false;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return false;
    }
    return true;
}

// Получение ID родительского процесса
static DWORD GetParentProcessId(DWORD pid) {
    DWORD parentPid = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (pe.th32ProcessID == pid) {
                    parentPid = pe.th32ParentProcessID;
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }
    return parentPid;
}

// Получение имени процесса по PID (без пути)
static std::wstring GetProcessName(DWORD pid) {
    std::wstring name;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (pe.th32ProcessID == pid) {
                    name = pe.szExeFile;
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }
    return name;
}

// Проверка корректности запуска
static bool ShouldContinueNormalRun() {
    // 1. Определяем родительский процесс
    DWORD myPid = GetCurrentProcessId();
    DWORD parentPid = GetParentProcessId(myPid);
    std::wstring parentName = GetProcessName(parentPid);
    std::string parentNameA(parentName.begin(), parentName.end());
    // 2. Если родитель — наша служба, то это запуск из службы — продолжаем
    if (_wcsicmp(parentName.c_str(), L"ZIoVPOService.exe") == 0) {
        return true;
    }
    // 3. Ограничение на множественный запуск
    if (!checkSingleInstance()) {
        return 0;
    }
    // 4. Запуск пользователем (родитель не служба)
    if (!ServiceClient::IsRunning()) {
        if (ServiceClient::StartAndWait()) {
            return false;
        } else {
            QMessageBox::critical(nullptr, QObject::tr("Ошибка"),
                QObject::tr("Не удалось запустить службу ZIoVPOService.\n"
                            "Убедитесь, что у вас есть права администратора."));
            return false;
        }
    } else {
        return false;
    }
}

int main(int argc, char *argv[]) 
{
    if (!ShouldContinueNormalRun()) {
        return 0;
    }

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    MainWindow mainWindow;
    TrayManager trayManager(&mainWindow);

    if (!QCoreApplication::arguments().contains("--show_hidden")) {
        mainWindow.show();
    } 
    
    TaskbarCreatedFilter filter([&trayManager]() {
        trayManager.restoreTray();
    });
    app.installNativeEventFilter(&filter);
    return QCoreApplication::exec();
}