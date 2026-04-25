#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <memory>
#include "ServiceClient.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class AuthDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

signals:
    void exitRequested();
    
private slots:
    void updateWindow();
    void onLogout();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    Ui::MainWindow *ui;
    void enableAntivirusFeatures(bool enable);
    void createStatusBar();
    void updateStatusBar(const QString &login, const QString &expirationDate);

    QTimer m_timer;
    bool m_wasAuthenticated;
    bool m_wasLicensed;
    bool m_wasVisible = false;
    std::unique_ptr<AuthDialog> m_authDialog;
};
#endif // MAINWINDOW_H
