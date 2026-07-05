#include "DatagateAuthLogin.h"
#include "AdminTotpSetupDialog.h"
#include "AuthSession.h"
#include "FreeTierOnboardingDialog.h"
#include "DatagateTr.h"
#include "DatagateUtils.h"

#include <QMessageBox>

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QWidget>

namespace {

QString normalizeApiBase(QString base)
{
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    return base;
}

QString strKey(const QJsonObject& o, std::initializer_list<const char*> keys)
{
    for (const char* k : keys) {
        const QJsonValue v = o.value(QString::fromLatin1(k));
        if (v.isString()) {
            const QString s = v.toString().trimmed();
            if (!s.isEmpty()) {
                return s;
            }
        }
    }
    return {};
}

bool boolKey(const QJsonObject& o, std::initializer_list<const char*> keys)
{
    for (const char* k : keys) {
        if (o.contains(QString::fromLatin1(k))) {
            return o.value(QString::fromLatin1(k)).toBool(false);
        }
    }
    return false;
}

int intKey(const QJsonObject& o, std::initializer_list<const char*> keys)
{
    for (const char* k : keys) {
        const QJsonValue v = o.value(QString::fromLatin1(k));
        if (v.isDouble()) {
            return v.toInt();
        }
        if (v.isString()) {
            bool ok = false;
            const int n = v.toString().trimmed().toInt(&ok);
            if (ok) {
                return n;
            }
        }
    }
    return 0;
}

using HttpResult = std::tuple<int, QNetworkReply::NetworkError, QByteArray>;

HttpResult runRequest(QNetworkAccessManager* nam, QNetworkRequest req, const QByteArray& payload = {})
{
    QNetworkReply* rep = payload.isEmpty() ? nam->get(req) : nam->post(req, payload);
    QEventLoop loop;
    QObject::connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const int code = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError nerr = rep->error();
    const QByteArray raw = rep->readAll();
    rep->deleteLater();
    return {code, nerr, raw};
}

bool envelopeSuccess(const QByteArray& raw, QString* errorOut)
{
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Invalid JSON from API.");
        }
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("success")).toBool(false)) {
        return true;
    }
    if (errorOut) {
        *errorOut = root.value(QStringLiteral("message")).toString();
        if (errorOut->isEmpty()) {
            *errorOut = Datagate::tr("API request failed.");
        }
    }
    return false;
}

std::optional<QJsonObject> parseSuccessData(const QByteArray& raw, QString* errorOut)
{
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Invalid JSON from API.");
        }
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("success")).toBool(false)) {
        return root.value(QStringLiteral("data")).toObject();
    }
    if (errorOut) {
        *errorOut = root.value(QStringLiteral("message")).toString();
        if (errorOut->isEmpty()) {
            *errorOut = Datagate::tr("API request failed.");
        }
    }
    return std::nullopt;
}

QString jwtPayloadJson(const QString& accessToken)
{
    const QStringList parts = accessToken.split(QLatin1Char('.'));
    if (parts.size() < 2) {
        return {};
    }
    QByteArray b = parts[1].toUtf8();
    b.replace('-', '+');
    b.replace('_', '/');
    const int pad = (4 - (b.size() % 4)) % 4;
    b.append(QByteArray(pad, '='));
    return QString::fromUtf8(QByteArray::fromBase64(b));
}

} // namespace

namespace DatagateAuth {

LoginFlow resolveLoginFlow(const QJsonObject& data)
{
    LoginFlow flow;
    flow.payload = data;
    const bool requiresTotp = boolKey(data, {"requiresTotp", "RequiresTotp"});
    flow.loginChallengeId = strKey(data, {"loginChallengeId", "LoginChallengeId"});
    flow.displayName = strKey(data, {"displayName", "DisplayName"});

    if (requiresTotp && !flow.loginChallengeId.isEmpty()) {
        flow.kind = LoginFlowKind::TotpChallenge;
        return flow;
    }

    const QString token = strKey(data, {"token", "Token"});
    if (token.isEmpty()) {
        flow.kind = LoginFlowKind::Tokens;
        return flow;
    }

    if (boolKey(data, {"requiresTotpSetup", "RequiresTotpSetup"})) {
        flow.kind = LoginFlowKind::SetupRequired;
        return flow;
    }

    flow.kind = LoginFlowKind::Tokens;
    return flow;
}

bool isLoginChallengeExpiredMessage(const QString& message)
{
    static const QRegularExpression re(
        QStringLiteral("challenge expired|too many invalid attempts|sign in again"),
        QRegularExpression::CaseInsensitiveOption);
    return re.match(message).hasMatch();
}

bool jwtIsAdmin(const QString& accessToken)
{
    if (accessToken.isEmpty()) {
        return false;
    }
    const QString payload = jwtPayloadJson(accessToken);
    if (payload.isEmpty()) {
        return false;
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    static const QString kRoleClaim = QStringLiteral("http://schemas.microsoft.com/ws/2008/06/identity/claims/role");
    const auto roleMatches = [](const QJsonValue& v) {
        if (v.isString()) {
            return v.toString().compare(QStringLiteral("Admin"), Qt::CaseInsensitive) == 0;
        }
        if (v.isArray()) {
            for (const QJsonValue& item : v.toArray()) {
                if (item.isString()
                    && item.toString().compare(QStringLiteral("Admin"), Qt::CaseInsensitive) == 0) {
                    return true;
                }
            }
        }
        return false;
    };
    if (roleMatches(o.value(kRoleClaim))) {
        return true;
    }
    if (roleMatches(o.value(QStringLiteral("role")))) {
        return true;
    }
    return false;
}

QString apiErrorMessageFromBody(const QByteArray& body, const QString& fallback)
{
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        return fallback;
    }
    const QJsonObject root = doc.object();
    QString msg = root.value(QStringLiteral("message")).toString().trimmed();
    if (msg.isEmpty()) {
        msg = root.value(QStringLiteral("detail")).toString().trimmed();
    }
    if (msg.isEmpty() && root.value(QStringLiteral("success")).toBool(false) == false) {
        msg = root.value(QStringLiteral("Message")).toString().trimmed();
    }
    return msg.isEmpty() ? fallback : msg;
}

std::optional<QJsonObject> postTotpVerifyLoginSync(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& loginChallengeId,
    const QString& code,
    QString* errorOut)
{
    if (!nam) {
        if (errorOut) {
            *errorOut = Datagate::tr("Internal error (no network manager).");
        }
        return std::nullopt;
    }
    const QString base = normalizeApiBase(apiBaseUrl);
    if (base.isEmpty() || loginChallengeId.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Invalid TOTP verify request.");
        }
        return std::nullopt;
    }

    QJsonObject body;
    body.insert(QStringLiteral("loginChallengeId"), loginChallengeId);
    body.insert(QStringLiteral("code"), code.trimmed());

    QNetworkRequest req(QUrl(base + QStringLiteral("/api/auth/totp/verify-login")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    req.setRawHeader("Accept", "application/json");
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    const auto [codeHttp, nerr, raw] = runRequest(nam, req, payload);

    if (nerr != QNetworkReply::NoError) {
        if (errorOut) {
            *errorOut = apiErrorMessageFromBody(raw, QString::fromUtf8(raw.left(400)));
        }
        return std::nullopt;
    }
    if (codeHttp < 200 || codeHttp >= 300) {
        if (errorOut) {
            *errorOut = apiErrorMessageFromBody(raw, Datagate::tr("TOTP verification failed (%1).").arg(codeHttp));
        }
        return std::nullopt;
    }
    return parseSuccessData(raw, errorOut);
}

TotpStatus fetchTotpStatusSync(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    QString* errorOut)
{
    TotpStatus st;
    if (!nam) {
        st.error = Datagate::tr("Internal error (no network manager).");
        return st;
    }
    const QString base = normalizeApiBase(apiBaseUrl);
    const QString auth = bearerToken.trimmed();
    if (base.isEmpty() || auth.isEmpty()) {
        st.error = Datagate::tr("Not signed in.");
        return st;
    }

    QNetworkRequest req(QUrl(base + QStringLiteral("/api/auth/totp/status")));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + auth).toUtf8());

    const auto [codeHttp, nerr, raw] = runRequest(nam, req);
    if (nerr != QNetworkReply::NoError || codeHttp < 200 || codeHttp >= 300) {
        st.error = apiErrorMessageFromBody(raw, Datagate::tr("Could not load TOTP status (%1).").arg(codeHttp));
        return st;
    }

    const std::optional<QJsonObject> data = parseSuccessData(raw, &st.error);
    if (!data.has_value()) {
        return st;
    }
    st.ok = true;
    st.isAdmin = boolKey(*data, {"isAdmin", "IsAdmin"});
    st.totpEnabled = boolKey(*data, {"totpEnabled", "TotpEnabled"});
    st.requiresTotpSetup = boolKey(*data, {"requiresTotpSetup", "RequiresTotpSetup"});
    return st;
}

std::optional<TotpSetupInfo> postTotpSetupSync(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    QString* errorOut)
{
    const QString base = normalizeApiBase(apiBaseUrl);
    const QString auth = bearerToken.trimmed();
    if (!nam || base.isEmpty() || auth.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Not signed in.");
        }
        return std::nullopt;
    }

    QNetworkRequest req(QUrl(base + QStringLiteral("/api/auth/totp/setup")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + auth).toUtf8());
    const QByteArray payload = QJsonDocument(QJsonObject()).toJson(QJsonDocument::Compact);

    const auto [codeHttp, nerr, raw] = runRequest(nam, req, payload);
    if (nerr != QNetworkReply::NoError || codeHttp < 200 || codeHttp >= 300) {
        if (errorOut) {
            *errorOut = apiErrorMessageFromBody(raw, Datagate::tr("TOTP setup failed (%1).").arg(codeHttp));
        }
        return std::nullopt;
    }
    const std::optional<QJsonObject> data = parseSuccessData(raw, errorOut);
    if (!data.has_value()) {
        return std::nullopt;
    }
    TotpSetupInfo info;
    info.sharedSecret = strKey(*data, {"sharedSecret", "SharedSecret"});
    info.otpAuthUri = strKey(*data, {"otpAuthUri", "OtpAuthUri"});
    info.issuer = strKey(*data, {"issuer", "Issuer"});
    info.accountName = strKey(*data, {"accountName", "AccountName"});
    if (info.sharedSecret.isEmpty() && info.otpAuthUri.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("TOTP setup returned no secret.");
        }
        return std::nullopt;
    }
    return info;
}

bool postTotpConfirmSync(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    const QString& code,
    QString* errorOut)
{
    const QString base = normalizeApiBase(apiBaseUrl);
    const QString auth = bearerToken.trimmed();
    if (!nam || base.isEmpty() || auth.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Not signed in.");
        }
        return false;
    }

    QJsonObject body;
    body.insert(QStringLiteral("code"), code.trimmed());

    QNetworkRequest req(QUrl(base + QStringLiteral("/api/auth/totp/confirm")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + auth).toUtf8());
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    const auto [codeHttp, nerr, raw] = runRequest(nam, req, payload);
    if (nerr != QNetworkReply::NoError || codeHttp < 200 || codeHttp >= 300) {
        if (errorOut) {
            *errorOut = apiErrorMessageFromBody(raw, Datagate::tr("TOTP confirm failed (%1).").arg(codeHttp));
        }
        return false;
    }
    return envelopeSuccess(raw, errorOut);
}

bool ensureAdminTotpSetupIfRequired(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    QWidget* parent)
{
    const QString token = bearerToken.trimmed();
    if (token.isEmpty() || !jwtIsAdmin(token)) {
        return true;
    }

    QString err;
    const TotpStatus st = fetchTotpStatusSync(nam, apiBaseUrl, token, &err);
    if (!st.ok) {
        if (parent) {
            QMessageBox::warning(parent, Datagate::tr("Admin security"), err);
        }
        return false;
    }
    if (!st.requiresTotpSetup) {
        return true;
    }

    AdminTotpSetupDialog dlg(nam, apiBaseUrl, token, parent);
    return dlg.exec() == QDialog::Accepted;
}

FreeTierAccessStatus fetchFreeTierAccessStatusSync(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    QString* errorOut)
{
    FreeTierAccessStatus st;
    if (!nam) {
        st.error = Datagate::tr("Internal error (no network manager).");
        if (errorOut) {
            *errorOut = st.error;
        }
        return st;
    }
    const QString base = normalizeApiBase(apiBaseUrl);
    const QString auth = bearerToken.trimmed();
    if (base.isEmpty() || auth.isEmpty()) {
        st.error = Datagate::tr("Not signed in.");
        if (errorOut) {
            *errorOut = st.error;
        }
        return st;
    }

    QNetworkRequest req(QUrl(base + QStringLiteral("/api/auth/free-tier-access/status")));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + auth).toUtf8());

    const auto [codeHttp, nerr, raw] = runRequest(nam, req);
    if (nerr != QNetworkReply::NoError || codeHttp < 200 || codeHttp >= 300) {
        st.error = apiErrorMessageFromBody(raw, Datagate::tr("Could not load free-tier status (%1).").arg(codeHttp));
        if (errorOut) {
            *errorOut = st.error;
        }
        return st;
    }

    const std::optional<QJsonObject> data = parseSuccessData(raw, &st.error);
    if (!data.has_value()) {
        if (errorOut) {
            *errorOut = st.error;
        }
        return st;
    }
    st.ok = true;
    st.isApplicable = boolKey(*data, {"isApplicable", "IsApplicable"});
    st.isCompliant = boolKey(*data, {"isCompliant", "IsCompliant"});
    st.isMergedAccount = boolKey(*data, {"isMergedAccount", "IsMergedAccount"});
    st.isChannelSubscribed = boolKey(*data, {"isChannelSubscribed", "IsChannelSubscribed"});
    st.isGracePeriod = boolKey(*data, {"isGracePeriod", "IsGracePeriod"});
    st.isLinkedToTelegram = boolKey(*data, {"isLinkedToTelegram", "IsLinkedToTelegram"});
    st.canRequestAccountLinkCode = boolKey(*data, {"canRequestAccountLinkCode", "CanRequestAccountLinkCode"});
    st.activePlanName = strKey(*data, {"activePlanName", "ActivePlanName"});
    st.requiredChannel = strKey(*data, {"requiredChannel", "RequiredChannel"});
    return st;
}

std::optional<TelegramAccountLinkCode> postRequestTelegramAccountLinkCodeSync(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    qint64 telegramId,
    QString* errorOut)
{
    const QString base = normalizeApiBase(apiBaseUrl);
    const QString auth = bearerToken.trimmed();
    if (!nam || base.isEmpty() || auth.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Not signed in.");
        }
        return std::nullopt;
    }
    if (telegramId <= 0) {
        if (errorOut) {
            *errorOut = Datagate::tr("Telegram user ID must be a positive number.");
        }
        return std::nullopt;
    }

    QJsonObject body;
    body.insert(QStringLiteral("telegramId"), static_cast<double>(telegramId));

    QNetworkRequest req(QUrl(base + QStringLiteral("/api/auth/telegram/request-account-link-code")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + auth).toUtf8());
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    const auto [codeHttp, nerr, raw] = runRequest(nam, req, payload);
    if (nerr != QNetworkReply::NoError || codeHttp < 200 || codeHttp >= 300) {
        if (errorOut) {
            *errorOut = apiErrorMessageFromBody(raw, Datagate::tr("Could not request link code (%1).").arg(codeHttp));
        }
        return std::nullopt;
    }
    const std::optional<QJsonObject> data = parseSuccessData(raw, errorOut);
    if (!data.has_value()) {
        return std::nullopt;
    }
    TelegramAccountLinkCode link;
    link.code = strKey(*data, {"code", "Code"});
    link.expiresInSeconds = intKey(*data, {"expiresInSeconds", "ExpiresInSeconds"});
    if (link.code.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Link code request returned no code.");
        }
        return std::nullopt;
    }
    return link;
}

void showFreeTierOnboardingIfRequired(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    AuthSession* session,
    QWidget* parent)
{
    if (!session || !session->ensureValidAccessToken()) {
        return;
    }
    const QString auth = session->accessToken().trimmed();
    if (auth.isEmpty()) {
        return;
    }

    QString err;
    const FreeTierAccessStatus st = fetchFreeTierAccessStatusSync(nam, apiBaseUrl, auth, &err);
    if (!st.ok) {
        if (parent) {
            QMessageBox::warning(parent, Datagate::tr("Telegram setup"), err);
        }
        return;
    }
    if (!st.isApplicable || st.isCompliant) {
        return;
    }

    FreeTierOnboardingDialog dlg(nam, apiBaseUrl, session, st, parent);
    dlg.exec();
}

} // namespace DatagateAuth
