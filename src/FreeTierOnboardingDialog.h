#pragma once

#include "DatagateAuthLogin.h"

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class QNetworkAccessManager;

/// Free/Default plan onboarding: Telegram channel subscription or account link via bot code.
class FreeTierOnboardingDialog final : public QDialog {
    Q_OBJECT
public:
    FreeTierOnboardingDialog(QNetworkAccessManager* nam,
        const QString& apiBaseUrl,
        const QString& bearerToken,
        const DatagateAuth::FreeTierAccessStatus& initialStatus,
        QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void applyStatus(const DatagateAuth::FreeTierAccessStatus& status);
    void refreshStatus();
    void requestLinkCode();
    void clearLinkCodeDisplay();
    void updateLinkCodeExpiryLabel();
    static QUrl channelUrlFromHandle(const QString& requiredChannel);

    QNetworkAccessManager* m_nam = nullptr;
    QString m_apiBase;
    QString m_bearer;
    DatagateAuth::FreeTierAccessStatus m_status;

    QLabel* m_title = nullptr;
    QLabel* m_lead = nullptr;
    QLabel* m_planLabel = nullptr;
    QLabel* m_channelLabel = nullptr;
    QPushButton* m_openChannelBtn = nullptr;
    QLabel* m_linkHeading = nullptr;
    QLabel* m_linkHint = nullptr;
    QLineEdit* m_telegramIdEdit = nullptr;
    QPushButton* m_requestCodeBtn = nullptr;
    QLabel* m_codeLabel = nullptr;
    QLabel* m_codeExpiryLabel = nullptr;
    QLabel* m_codeInstructions = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    QTimer* m_pollTimer = nullptr;
    QTimer* m_codeExpiryTimer = nullptr;
    int m_codeSecondsLeft = 0;
};
