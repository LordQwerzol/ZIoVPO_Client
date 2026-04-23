#include "traymanager.h"

#include "traymanager.h"
#include "mainwindow.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QApplication>

TrayManager::TrayManager(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow)
{
    // Создаём иконку
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/rec/resources/appIcon.png"));
    m_trayIcon->setToolTip("Мое приложение");

    // Создаём контекстное меню
    m_contextMenu = new QMenu();
    m_contextMenu->addAction("Открыть", this, &TrayManager::onOpen);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction("Выход", this, &TrayManager::onExit);

    m_trayIcon->setContextMenu(m_contextMenu);

    // Подключаем обработку кликов
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &TrayManager::onActivated);

    // Показываем иконку в трее
    m_trayIcon->show();
}

TrayManager::~TrayManager()
{
    delete m_contextMenu;
    delete m_trayIcon;
}

void TrayManager::restoreTray()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
        m_trayIcon->show();
    }
}

void TrayManager::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    // Левая кнопка мыши -> показать главное окно
    if (reason == QSystemTrayIcon::Trigger) {
        onOpen();
    }
}

void TrayManager::onOpen()
{
    if (m_mainWindow) {
        m_mainWindow->show();
        m_mainWindow->raise();      // Выводим на передний план
        m_mainWindow->activateWindow();
    }
}

void TrayManager::onExit()
{
    qApp->quit();
}