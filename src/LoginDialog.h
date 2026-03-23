#pragma once

#include <QDialog>

class QCloseEvent;
class QShowEvent;

class AuthSession;
class GoogleAuthHelper;
class QLabel;
class QPushButton;
class QNetworkAccessManager;

/// Login window shown before MainWindow (same role as DataGateWin.LoginWindow).
class LoginDialog final : public QDialog {
    Q_OBJECT
public:
    LoginDialog(AuthSession* session, QNetworkAccessManager* nam, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* e) override;
    void closeEvent(QCloseEvent* e) override;

private:
    AuthSession* m_session = nullptr;
    GoogleAuthHelper* m_google = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_signIn = nullptr;
    QPushButton* m_cancel = nullptr;
    bool m_centerOnFirstShow = true;
};
