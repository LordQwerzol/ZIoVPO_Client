#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QFile>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->actionExit, &QAction::triggered, this, [this]() {ServiceClient::StopService();});
}

MainWindow::~MainWindow()
{
    delete ui;
}