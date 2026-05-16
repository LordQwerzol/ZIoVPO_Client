#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "AuthDialog.h"
#include "DesktopManager.h"
#include <QDebug>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    createStatusBar();
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onExit);
    connect(ui->actionLogout, &QAction::triggered, this, &MainWindow::onLogout);
    connect(ui->actionGoHome, &QAction::triggered, this, [this]{ui->stackedWidget->setCurrentWidget(ui->pageWelcome);});
    connect(ui->actionScanFile, &QAction::triggered, this, [this]{MainWindow::scanPath(0);});
    connect(ui->actionScanFloder, &QAction::triggered, this, [this]{MainWindow::scanPath(1);});
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::scanPath(int pathType)
{
    QString path;
    if (pathType == 0) {
        path = QFileDialog::getOpenFileName(this,
            tr("Выберите файл для сканирования"),
            tr("C:\\Users\\Public"),
            tr("Все файлы (*.*)"));
    }
    else if (pathType == 1) {
        path = QFileDialog::getExistingDirectory(this,
            tr("Выберите папку для сканирования"),
            tr("C:\\Users\\Public"),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    }
    if (path.isEmpty())
        return;

    std::vector<ThreatInfoRpc> threats;
    std::wstring err;
    int ret =  ServiceClient::ScanPath(path.toStdWString(), threats, err);
    if (ret != 0){
        QMessageBox::warning(this, "Ошибка", "Что-то пошло не так...\n" + QString::fromStdWString(err));
        ServiceClient::StopService();
    }
    int rowCount = static_cast<int>(threats.size());
    ui->tableWidget->setRowCount(0);
    ui->tableWidget->setRowCount(rowCount);

    for (int i = 0; i < rowCount; ++i) {
        const ThreatInfoRpc& threat = threats[i];
        QTableWidgetItem* numItem = new QTableWidgetItem(QString::number(i + 1));
        ui->tableWidget->setItem(i, 0, numItem);

        QString typeStr = QString::fromStdWString(threat.objectTypeString);
        QTableWidgetItem* typeItem = new QTableWidgetItem(typeStr);
        ui->tableWidget->setItem(i, 1, typeItem);

        QString nameStr = QString::fromStdWString(threat.threatName);
        QTableWidgetItem* nameItem = new QTableWidgetItem(nameStr);
        ui->tableWidget->setItem(i, 2, nameItem);

        QString pathStr = QString::fromStdWString(threat.filePath);
        QTableWidgetItem* pathItem = new QTableWidgetItem(pathStr);
        ui->tableWidget->setItem(i, 3, pathItem);

    }

    ui->stackedWidget->setCurrentWidget(ui->pageTable);
}

void MainWindow::onExit()
{
    if (DesktopManager::confirmation())
        ServiceClient::StopService();
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
    uint64_t dbTimestamp = 0;
    uint32_t dbRecordCount = 0;
    std::wstring dbError;
    int dbResult = ServiceClient::GetDatabaseInfo(dbTimestamp, dbRecordCount, dbError);
    if (!dbResult == 0){
        QMessageBox::warning(this, "Ошибка", "Что-то пошло не так...\n" + QString::fromStdWString(dbError));
        ServiceClient::StopService();
    }
    updateStatusBar(QString::fromStdWString(username), QString::fromStdWString(expirationDate),
                    QDateTime::fromMSecsSinceEpoch(dbTimestamp).toString("dd.MM.yyyy hh:mm:ss"), 
                    QString::number(dbRecordCount));
    enableAntivirusFeatures(true);
    show();
    raise();
    activateWindow();
}

void MainWindow::onLogout()
{
    updateStatusBar("Не авторизован", "-", "-", "-");
    ServiceClient::Logout();
    enableAntivirusFeatures(false);
    updateWindow();
}

void MainWindow::enableAntivirusFeatures(bool enable)
{
    setEnabled(enable); // Здесь заблокировать/разблокировать все действия, связанные с антивирусом
}

void MainWindow::createStatusBar()
{
    QLabel *iconLabel1 = new QLabel(this);
    QLabel *iconLabel2 = new QLabel(this);
    QLabel *iconLabel3 = new QLabel(this);
    QLabel *iconLabel4 = new QLabel(this);
    QPixmap pixmap1(":/rec/resources/login.png");
    QPixmap pixmap2(":/rec/resources/event.png");
    QPixmap pixmap3(":/rec/resources/date_db.png");
    QPixmap pixmap4(":/rec/resources/record_cnt.png");
    QLabel *textLabel1 = new QLabel("Не авторизован", this);
    QLabel *textLabel2 = new QLabel("-", this);
    QLabel *textLabel3 = new QLabel("-", this);
    QLabel *textLabel4 = new QLabel("-", this);
    
    iconLabel1->setPixmap(pixmap1.scaled(16, 16, Qt::KeepAspectRatio));
    iconLabel1->setToolTip("Ваш логин");
    iconLabel2->setPixmap(pixmap2.scaled(16, 16, Qt::KeepAspectRatio));
    iconLabel2->setToolTip("Дата истечения лицензии");
    iconLabel3->setPixmap(pixmap3.scaled(16, 16, Qt::KeepAspectRatio));
    iconLabel3->setToolTip("Дата загрузки даты базы антивирусных сигнатур");
    iconLabel4->setPixmap(pixmap4.scaled(16, 16, Qt::KeepAspectRatio));
    iconLabel4->setToolTip("Кол-во записей в базе антивирусных сигнатур");
    
    statusBar()->addWidget(iconLabel1);
    statusBar()->addWidget(textLabel1);
    statusBar()->addWidget(iconLabel2);
    statusBar()->addWidget(textLabel2);
    statusBar()->addWidget(iconLabel3);
    statusBar()->addWidget(textLabel3);
    statusBar()->addWidget(iconLabel4);
    statusBar()->addWidget(textLabel4);
}

void MainWindow::updateStatusBar(const QString &login, const QString &expirationDate
                                , const QString &databaseDate, const QString &records)
{
    QList<QLabel*> labels = statusBar()->findChildren<QLabel*>();
    if (labels.size() >= 2) {
        labels[1]->setText(login);
        labels[3]->setText(expirationDate);
        labels[5]->setText(databaseDate);
        labels[7]->setText(records);
    }
}
