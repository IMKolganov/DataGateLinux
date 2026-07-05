#pragma once

#include <QJsonObject>
#include <QString>
#include <optional>

class QNetworkAccessManager;

namespace DatagateAuth {

enum class LoginFlowKind {
    Tokens,
    TotpChallenge,
    SetupRequired,
};

struct LoginFlow {
    LoginFlowKind kind = LoginFlowKind::Tokens;
    QJsonObject payload;
    QString loginChallengeId;
    QString displayName;
};

struct TotpStatus {
    bool ok = false;
    QString error;
    bool isAdmin = false;
    bool totpEnabled = false;
    bool requiresTotpSetup = false;
};

struct TotpSetupInfo {
    QString sharedSecret;
    QString otpAuthUri;
    QString issuer;
    QString accountName;
};

LoginFlow resolveLoginFlow(const QJsonObject& data);
bool isLoginChallengeExpiredMessage(const QString& message);
bool jwtIsAdmin(const QString& accessToken);

QString apiErrorMessageFromBody(const QByteArray& body, const QString& fallback);

std::optional<QJsonObject> postTotpVerifyLoginSync(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& loginChallengeId,
    const QString& code,
    QString* errorOut);

TotpStatus fetchTotpStatusSync(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    QString* errorOut);

std::optional<TotpSetupInfo> postTotpSetupSync(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    QString* errorOut);

bool postTotpConfirmSync(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    const QString& code,
    QString* errorOut);

/// Modal setup when GET totp/status reports requiresTotpSetup for an admin. Returns false on cancel → caller should logout.
bool ensureAdminTotpSetupIfRequired(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    QWidget* parent);

struct FreeTierAccessStatus {
    bool ok = false;
    QString error;
    bool isApplicable = false;
    bool isCompliant = true;
    bool isMergedAccount = false;
    bool isChannelSubscribed = false;
    bool isGracePeriod = false;
    bool isLinkedToTelegram = false;
    bool canRequestAccountLinkCode = false;
    QString activePlanName;
    QString requiredChannel;
};

struct TelegramAccountLinkCode {
    QString code;
    int expiresInSeconds = 0;
};

FreeTierAccessStatus fetchFreeTierAccessStatusSync(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    QString* errorOut);

std::optional<TelegramAccountLinkCode> postRequestTelegramAccountLinkCodeSync(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    qint64 telegramId,
    QString* errorOut);

/// Shows onboarding when GET free-tier-access/status reports non-compliance on Free/Default plans.
void showFreeTierOnboardingIfRequired(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    QWidget* parent);

} // namespace DatagateAuth
