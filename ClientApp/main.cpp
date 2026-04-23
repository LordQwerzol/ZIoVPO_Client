#include "mainwindow.h"
#include "traymanager.h"
#include "TaskbarCreatedFilter.h"
#include <windows.h>
#include <QApplication>

// Глобальная функция проверки одиночного запуска через именованный мьютекс
static bool checkSingleInstance() {
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\MyUniqueTrayAppMutex");
    if (mutex == NULL)
        return false;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    if (!checkSingleInstance()) {
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
