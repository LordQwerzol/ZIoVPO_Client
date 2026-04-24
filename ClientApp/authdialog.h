#ifndef AUTHDIALOG_H
#define AUTHDIALOG_H

#include <QDialog>
#include <QShortcut>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

namespace Ui {
class AuthDialog;
}

class AuthDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { LoginMode, ActivationMode };
    explicit AuthDialog(QWidget *parent = nullptr);
    ~AuthDialog();

    void setMode(Mode mode);

signals:
    void authSuccess();

private slots:
    void slotVisPassword();
    void slotLogin();
    void slotActivate();

private:
    Ui::AuthDialog *ui;
    Mode m_mode;
    QRegularExpression emailRegex = QRegularExpression("([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    QRegularExpression codeRegex = QRegularExpression("([a-zA-Z0-9]{5}-[a-zA-Z0-9]{5}-[a-zA-Z0-9]{5}-[a-zA-Z0-9]{5})");
};

#endif // AUTHDIALOG_H
