#pragma once

#include <QDialog>

class QCloseEvent;
class QComboBox;
class QEvent;
class QLabel;
class QShowEvent;

class AuthSession;
class GoogleAuthHelper;
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
    void changeEvent(QEvent* e) override;

private:
    void retranslateUi();

    AuthSession* m_session = nullptr;
    GoogleAuthHelper* m_google = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_hint = nullptr;
    QLabel* m_langLabel = nullptr;
    QComboBox* m_languageCombo = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_signIn = nullptr;
    QPushButton* m_cancel = nullptr;
    bool m_centerOnFirstShow = true;
};
