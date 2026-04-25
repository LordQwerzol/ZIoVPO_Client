#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "AuthDialog.h"
#include <QDebug>
#include <QFile>
#include <QLabel>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    createStatusBar();
    connect(ui->actionExit, &QAction::triggered, this, [this]() {ServiceClient::StopService();});
    connect(ui->actionLogout, &QAction::triggered, this, &MainWindow::onLogout);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // Запускаем таймер, если окно показано
    if (!m_timer.isActive()) {
        connect(&m_timer, &QTimer::timeout, this, &MainWindow::updateWindow);
        m_timer.start(10000);
    }
    // Принудительно обновляем состояние при показе
    updateWindow();
}

void MainWindow::hideEvent(QHideEvent *event)
{
    QMainWindow::hideEvent(event);
    // Останавливаем таймер, когда окно скрыто
    m_timer.stop();
    m_wasVisible = false;
}

void MainWindow::updateWindow()
{
    if (m_authDialog && m_authDialog->isVisible()) 
        return; // диалог уже показан, не создаём новый

    // 1. Проверяем аутентификацию
    std::wstring username, errorAuth;
    int authResult = ServiceClient::GetCurrentUser(username, errorAuth);
    bool authenticated = (authResult == 0);

    // 2. Проверяем лицензию
    bool licensed = false;
    std::wstring m_status;
    std::wstring expirationDate;
    if (authenticated) {
        std::wstring status, errorLic;
        int licResult = ServiceClient::GetLicenseStatus(status, expirationDate, errorLic);
        licensed = (licResult == 0 && status == L"ACTIVE");
        m_status = status;
    }

    bool isVisible = true;
    // 3. Если состояние не изменилось и уже есть диалог/окно – не пересоздаём
    if (authenticated == m_wasAuthenticated && licensed == m_wasLicensed && isVisible == m_wasVisible 
        && (m_authDialog == nullptr || !m_authDialog->isVisible())) {
            m_wasVisible = isVisible;
            return;
        }
    m_wasAuthenticated = authenticated;
    m_wasLicensed = licensed;

    // 4. Если не аутентифицирован – показываем диалог входа (модально)
    if (!authenticated) {
        m_authDialog.reset();
        // Создаём диалог БЕЗ родителя, чтобы он не зависел от видимости MainWindow
        m_authDialog = std::make_unique<AuthDialog>(nullptr);
        connect(m_authDialog.get(), &AuthDialog::authSuccess, this, &MainWindow::updateWindow);
        connect(m_authDialog.get(), &QDialog::rejected, [this]() {ServiceClient::StopService();});
        m_authDialog->setMode(AuthDialog::LoginMode);
        int ret = m_authDialog->exec();
        if (ret == QDialog::Accepted) {
            updateWindow();
        }
        return;
    }

    // 5. Если аутентифицирован, но нет лицензии – показываем диалог активации
    if (!licensed) {
        m_authDialog.reset();
        m_authDialog = std::make_unique<AuthDialog>(nullptr);
        connect(m_authDialog.get(), &AuthDialog::authSuccess, this, &MainWindow::updateWindow);
        connect(m_authDialog.get(), &QDialog::rejected, [this]() { ServiceClient::StopService(); });
        m_authDialog->setMode(AuthDialog::ActivationMode);
        if (m_status == L"BLOCKED") {
            QMessageBox::warning(this, "Лицензия заблокирована", 
            "Ваша лицензия заблокирована. Обратитесь в поддержку или введите код активации.");
        }
        int ret = m_authDialog->exec();
        if (ret == QDialog::Accepted) {
            updateWindow();
        }
        return;
    }

    // 6. Всё хорошо – обновляем статус-бар и разблокируем функции
    updateStatusBar(QString::fromStdWString(username), QString::fromStdWString(expirationDate));
    enableAntivirusFeatures(true);
    show();
    raise();
    activateWindow();
}

void MainWindow::onLogout()
{
    updateStatusBar("Не авторизован", "-");
    ServiceClient::Logout();
    updateWindow();
}

void MainWindow::enableAntivirusFeatures(bool enable)
{
    setEnabled(enable); // Здесь заблокировать/разблокировать все действия, связанные с антивирусом
}

void MainWindow::createStatusBar()
{
    QLabel *iconLabel1 = new QLabel(this);
    QPixmap pixmap1(":/rec/resources/login.png");
    iconLabel1->setPixmap(pixmap1.scaled(16, 16, Qt::KeepAspectRatio));
    iconLabel1->setToolTip("Ваш логин");
    QLabel *textLabel1 = new QLabel("Не авторизован", this);
    QLabel *iconLabel2 = new QLabel(this);
    QPixmap pixmap2(":/rec/resources/event.png");
    iconLabel2->setPixmap(pixmap2.scaled(16, 16, Qt::KeepAspectRatio));
    iconLabel2->setToolTip("Дата истечения лицензии");
    QLabel *textLabel2 = new QLabel("-", this);
    statusBar()->addWidget(iconLabel1);
    statusBar()->addWidget(textLabel1);
    statusBar()->addWidget(iconLabel2);
    statusBar()->addWidget(textLabel2);
    // Сохраним указатели, чтобы потом обновлять
    // (можно найти по objectName или сохранить как члены класса)
}

void MainWindow::updateStatusBar(const QString &login, const QString &expirationDate)
{
    QList<QLabel*> labels = statusBar()->findChildren<QLabel*>();
    if (labels.size() >= 2) {
        labels[1]->setText(login);
        labels[3]->setText(expirationDate);
    }
}
