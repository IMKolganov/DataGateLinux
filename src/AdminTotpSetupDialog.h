#pragma once

#include <QDialog>
#include <QString>

class QNetworkAccessManager;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;

/// Mandatory admin TOTP enrollment (parity with DataGateMonitor /settings/security).
class AdminTotpSetupDialog final : public QDialog {
    Q_OBJECT
public:
    AdminTotpSetupDialog(QNetworkAccessManager* nam,
        const QString& apiBaseUrl,
        const QString& bearerToken,
        QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void showSetupInfo(const QString& sharedSecret, const QString& otpAuthUri);

    QNetworkAccessManager* m_nam = nullptr;
    QString m_apiBase;
    QString m_bearer;

    QLabel* m_title = nullptr;
    QLabel* m_lead = nullptr;
    QLabel* m_required = nullptr;
    QLabel* m_status = nullptr;
    QTextEdit* m_secretView = nullptr;
    QLineEdit* m_confirmCode = nullptr;
    QPushButton* m_beginBtn = nullptr;
    QPushButton* m_confirmBtn = nullptr;
    QPushButton* m_logoutBtn = nullptr;
};
