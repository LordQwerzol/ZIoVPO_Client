#include "authdialog.h"
#include "ui_authdialog.h"
#include "ServiceClient.h"
#include <QMessageBox>
#include <QRegularExpressionValidator>
#include <QShortcut>



AuthDialog::AuthDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AuthDialog)
    , m_mode(LoginMode)
{
    ui->setupUi(this);
    ui->stackedWidget->setCurrentWidget(ui->pageAuth);
    connect(new QShortcut(QKeySequence("Ctrl+Q"), this), &QShortcut::activated, [=](){ ui->cmdExit->animateClick(); });
    connect(ui->cmdPass, &QPushButton::clicked, this, &AuthDialog::slotVisPassword);
    connect(ui->cmdLogin, &QPushButton::clicked, this, &AuthDialog::slotLogin);
    connect(ui->cmdActivate, &QPushButton::clicked, this, &AuthDialog::slotActivate);
    connect(ui->cmdExit, &QPushButton::clicked, this, [this]() {ServiceClient::StopService();});
}

AuthDialog::~AuthDialog()
{
    delete ui;
}

void AuthDialog::setMode(Mode mode)
{
    m_mode = mode;
    if (mode == LoginMode) {
        ui->stackedWidget->setCurrentWidget(ui->pageAuth);
    } else {
        ui->stackedWidget->setCurrentWidget(ui->pageLicense);
    }
}

void AuthDialog::slotVisPassword()
{
    if (ui->txtPass->echoMode() == QLineEdit::Password) {
        ui->txtPass->setEchoMode(QLineEdit::Normal);
        ui->cmdPass->setIcon(QIcon(":/rec/resources/pass_of.png"));
    } else {
        ui->txtPass->setEchoMode(QLineEdit::Password);
        ui->cmdPass->setIcon(QIcon(":/rec/resources/pass_on.png"));
    }
}

void AuthDialog::slotLogin()
{
    QString login = ui->txtLogin->text();
    QString password = ui->txtPass->text();
    if (login.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите логин и пароль!");
        return;
    }
    QRegularExpressionValidator validator(emailRegex);
    int pos = 0;
    if (validator.validate(login, pos) != QValidator::Acceptable) {
        QMessageBox::warning(this, "Ошибка", "Введите корректный email адрес.");
        return;
    }

    std::wstring error;
    std::wstring outUsername;
    int result = ServiceClient::Login(login.toStdWString(), password.toStdWString(), outUsername, error);
    if (result != 0) {
        QMessageBox::warning(this, "Ошибка", QString::fromStdWString(error));
        return;
    }

    // После успешного логина проверим лицензию, если её нет – переключимся на активацию
    std::wstring status, expiration, licError;
    int licResult = ServiceClient::GetLicenseStatus(status, expiration, licError);
    if (licResult == 0 && status == L"ACTIVE") {
        accept();  // всё хорошо, закрываем диалог
        emit authSuccess();
    } else {
        // Нет активной лицензии – переключимся на страницу активации, но диалог не закрываем
        ui->stackedWidget->setCurrentWidget(ui->pageLicense);
        QMessageBox::information(this, "Лицензия", "У вас нет активной лицензии. Введите код активации.");
    }
}

void AuthDialog::slotActivate()
{
    QString code = ui->txtCode->text();
    if (code.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Код активации не может быть пустым!");
        return;
    }
    QRegularExpressionValidator validator(codeRegex);
    int pos = 0;
    if (validator.validate(code, pos) != QValidator::Acceptable) {
        QMessageBox::warning(this, "Ошибка", "Введите корректный код активации.");
        return;
    }

    std::wstring error;
    std::wstring status, expiration;
    int result = ServiceClient::ActivateProduct(code.toStdWString(), status, expiration, error);
    if (result != 0) {
        QMessageBox::warning(this, "Ошибка", QString::fromStdWString(error));
        return;
    }
    if (status == L"ACTIVE") {
        accept();  // активация успешна, закрываем диалог
        emit authSuccess();
    } else {
        QMessageBox::warning(this, "Ошибка", "Лицензия не активна: " + QString::fromStdWString(status));
    }
}