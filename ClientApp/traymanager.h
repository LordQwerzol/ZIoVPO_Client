#ifndef TRAYMANAGER_H
#define TRAYMANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include "ServiceClient.h"

class QSystemTrayIcon;
class QMenu;
class MainWindow;

class TrayManager : public QObject
{
    Q_OBJECT

public:
    explicit TrayManager(MainWindow *mainWindow, QObject *parent = nullptr);
    ~TrayManager();

    void restoreTray();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onOpen();
    void onExit();

signals:
    void exitRequested();

private:
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_contextMenu;
    MainWindow *m_mainWindow;
};

#endif // TRAYMANAGER_H
