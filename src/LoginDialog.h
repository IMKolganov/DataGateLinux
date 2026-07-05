#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QString>

class QCloseEvent;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QStackedWidget;

class AuthSession;
class GoogleAuthHelper;
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
    void showSignInPage();
    void showTotpPage(const QString& displayName);
    void handleLoginData(const QJsonObject& data);

    AuthSession* m_session = nullptr;
    GoogleAuthHelper* m_google = nullptr;
    QNetworkAccessManager* m_nam = nullptr;

    QStackedWidget* m_stack = nullptr;
    QWidget* m_signInPage = nullptr;
    QWidget* m_totpPage = nullptr;

    QLabel* m_title = nullptr;
    QLabel* m_hint = nullptr;
    QLabel* m_langLabel = nullptr;
    QComboBox* m_languageCombo = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_signIn = nullptr;
    QPushButton* m_cancel = nullptr;

    QLabel* m_totpLead = nullptr;
    QLineEdit* m_totpCode = nullptr;
    QLabel* m_totpStatus = nullptr;
    QPushButton* m_totpBack = nullptr;
    QPushButton* m_totpVerify = nullptr;
    QString m_loginChallengeId;

    bool m_centerOnFirstShow = true;
};
