#include "AuthSession.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUuid>

#include "AppConfig.h"
#include "AppLogging.h"

namespace {

/// Refresh before this many seconds of remaining lifetime (clock skew + network). Must stay well
/// below short test tokens (e.g. 60s); a 60s margin makes a 60s JWT never satisfy isAccessValid().
constexpr int kAccessValidityMarginSecs = 10;

/// JWT exp (UTC seconds) as a fallback when API expiration JSON is wrong or missing.
QDateTime jwtExpUtc(const QString& bearer)
{
    const QString t = bearer.trimmed();
    const int d1 = t.indexOf(QLatin1Char('.'));
    const int d2 = t.indexOf(QLatin1Char('.'), d1 + 1);
    if (d1 <= 0 || d2 <= d1) {
        return {};
    }
    QByteArray b64 = t.mid(d1 + 1, d2 - d1 - 1).toUtf8();
    const int pad = (4 - (b64.size() % 4)) % 4;
    b64 += QByteArray(pad, '=');
    const QByteArray json = QByteArray::fromBase64(b64, QByteArray::Base64UrlEncoding);
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    const QJsonValue exp = doc.object().value(QStringLiteral("exp"));
    if (!exp.isDouble()) {
        return {};
    }
    return QDateTime::fromSecsSinceEpoch(qint64(exp.toDouble()), Qt::UTC);
}

QString normBase(QString b)
{
    b = b.trimmed();
    while (b.endsWith(QLatin1Char('/'))) {
        b.chop(1);
    }
    return b;
}

QDateTime parseDateTimeVal(const QJsonValue& v)
{
    if (v.isString()) {
        const QString s = v.toString().trimmed();
        QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
        if (!dt.isValid()) {
            dt = QDateTime::fromString(s, Qt::ISODate);
        }
        if (!dt.isValid()) {
            return {};
        }
        if (dt.timeSpec() == Qt::LocalTime) {
            dt = dt.toUTC();
        }
        return dt;
    }
    if (v.isDouble()) {
        const double d = v.toDouble();
        // .NET may send Unix milliseconds; seconds match JWT exp style
        if (d > 1e11) {
            return QDateTime::fromMSecsSinceEpoch(qint64(d), Qt::UTC);
        }
        return QDateTime::fromSecsSinceEpoch(qint64(d), Qt::UTC);
    }
    return {};
}

QString strKey(const QJsonObject& o, const QStringList& keys)
{
    for (const QString& k : keys) {
        const QJsonValue v = o.value(k);
        if (v.isString() && !v.toString().isEmpty()) {
            return v.toString();
        }
    }
    return {};
}

} // namespace

AuthSession::AuthSession(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent)
    , m_nam(nam)
{
}

QString AuthSession::tokenFilePath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(base);
    return QDir(base).filePath(QStringLiteral("auth.json"));
}

QString AuthSession::getOrCreateDeviceId()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(base);
    const QString path = QDir(base).filePath(QStringLiteral("device.id"));
    QFile f(path);
    if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString id = QString::fromUtf8(f.readAll()).trimmed();
        f.close();
        if (!id.isEmpty()) {
            return id;
        }
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(id.toUtf8());
        f.close();
    }
    return id;
}

void AuthSession::applyJsonObject(const QJsonObject& o)
{
    m_tokens.token = strKey(o, {QStringLiteral("token"), QStringLiteral("Token")});
    // Refresh often returns only a new access token; do not wipe stored refresh/display/expiry.
    const QString newRt = strKey(o, {QStringLiteral("refreshToken"), QStringLiteral("RefreshToken")});
    if (!newRt.isEmpty()) {
        m_tokens.refreshToken = newRt;
    }
    const QString newDn = strKey(o, {QStringLiteral("displayName"), QStringLiteral("DisplayName")});
    if (!newDn.isEmpty()) {
        m_tokens.displayName = newDn;
    }

    if (o.contains(QStringLiteral("expiration")) || o.contains(QStringLiteral("Expiration"))) {
        const QJsonValue expV = o.contains(QStringLiteral("expiration"))
            ? o.value(QStringLiteral("expiration"))
            : o.value(QStringLiteral("Expiration"));
        const QDateTime exp = parseDateTimeVal(expV);
        if (exp.isValid()) {
            m_tokens.expiration = exp;
        }
    }
    if (o.contains(QStringLiteral("refreshExpiration")) || o.contains(QStringLiteral("RefreshExpiration"))) {
        const QJsonValue rexpV = o.contains(QStringLiteral("refreshExpiration"))
            ? o.value(QStringLiteral("refreshExpiration"))
            : o.value(QStringLiteral("RefreshExpiration"));
        const QDateTime rexp = parseDateTimeVal(rexpV);
        if (rexp.isValid()) {
            m_tokens.refreshExpiration = rexp;
        }
    }
}

bool AuthSession::loadFromFile()
{
    const QString path = tokenFilePath();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray raw = f.readAll();
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    applyJsonObject(doc.object());
    return !m_tokens.token.isEmpty();
}

bool AuthSession::saveToFile() const
{
    const QString path = tokenFilePath();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    QJsonObject o;
    o.insert(QStringLiteral("token"), m_tokens.token);
    o.insert(QStringLiteral("refreshToken"), m_tokens.refreshToken);
    o.insert(QStringLiteral("displayName"), m_tokens.displayName);
    if (m_tokens.expiration.isValid()) {
        o.insert(QStringLiteral("expiration"), m_tokens.expiration.toUTC().toString(Qt::ISODateWithMs));
    }
    if (m_tokens.refreshExpiration.isValid()) {
        o.insert(QStringLiteral("refreshExpiration"), m_tokens.refreshExpiration.toUTC().toString(Qt::ISODateWithMs));
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return true;
}

bool AuthSession::clearFile()
{
    m_tokens = {};
    QFile::remove(tokenFilePath());
    return true;
}

void AuthSession::initialize()
{
    QMutexLocker lock(&m_mutex);
    const bool ok = loadFromFile();
    qCInfo(lcAuth, "initialize: loadFromFile %s", ok ? "ok" : "no file or empty");
}

bool AuthSession::isAccessValid() const
{
    if (m_tokens.token.isEmpty()) {
        return false;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    static const QDateTime kMinPlausible = QDateTime(QDate(2020, 1, 1), QTime(0, 0), Qt::UTC);
    const QDateTime jwtExp = jwtExpUtc(m_tokens.token);

    // API expiration may outlive the JWT; the server validates the JWT. Use the earliest bound.
    bool have = false;
    QDateTime deadline;
    auto consider = [&](const QDateTime& dt) {
        if (!dt.isValid()) {
            return;
        }
        if (!have || dt < deadline) {
            deadline = dt;
            have = true;
        }
    };
    if (m_tokens.expiration.isValid()) {
        consider(m_tokens.expiration.toUTC());
    }
    if (jwtExp.isValid() && jwtExp > kMinPlausible) {
        consider(jwtExp);
    }
    if (!have) {
        return true; // opaque token, no parsed expiry
    }
    return deadline > now.addSecs(kAccessValidityMarginSecs);
}

void AuthSession::bumpAccessExpirationIfInvalid()
{
    if (m_tokens.token.isEmpty() || isAccessValid()) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime jwtExp = jwtExpUtc(m_tokens.token);
    static const QDateTime kMinPlausible = QDateTime(QDate(2020, 1, 1), QTime(0, 0), Qt::UTC);
    if (jwtExp.isValid() && jwtExp > kMinPlausible) {
        // Align stored expiration with JWT when API skew made isAccessValid false; only clear if
        // exp is past wall time (the old branch matched any future JWT not past +60s and wiped it).
        if (jwtExp <= now) {
            m_tokens.expiration = {};
        } else {
            m_tokens.expiration = jwtExp;
        }
        return;
    }
    static constexpr int kFallbackAccessTtlSecs = 300;
    m_tokens.expiration = now.addSecs(kFallbackAccessTtlSecs);
}

QString AuthSession::accessToken() const
{
    QMutexLocker lock(&m_mutex);
    return m_tokens.token;
}

bool AuthSession::refreshTokenSync()
{
    QString refreshTok;
    {
        QMutexLocker lock(&m_mutex);
        if (m_tokens.refreshToken.isEmpty()) {
            qCWarning(lcAuth, "refreshTokenSync: no refresh token");
            return false;
        }
        // If refreshExpiration is known and already past, do not call the API (Windows parity).
        // If API omitted refreshExpiration, still try POST /api/auth/refresh.
        if (m_tokens.refreshExpiration.isValid()
            && m_tokens.refreshExpiration.toUTC() <= QDateTime::currentDateTimeUtc().addSecs(5)) {
            qCWarning(lcAuth, "refreshTokenSync: refresh token expired");
            return false;
        }
        refreshTok = m_tokens.refreshToken;
    }

    const QString base = normBase(AppConfig::apiBaseUrl());
    if (base.isEmpty() || !m_nam) {
        qCWarning(lcAuth, "refreshTokenSync: no API base or QNetworkAccessManager");
        return false;
    }

    QJsonObject body;
    body.insert(QStringLiteral("refreshToken"), refreshTok);
    body.insert(QStringLiteral("deviceId"), getOrCreateDeviceId());
    body.insert(QStringLiteral("userAgent"), QStringLiteral("DataGateLinux/1.0 (Linux)"));

    const QUrl url(base + QStringLiteral("/api/auth/refresh"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    qCInfo(lcAuth, "refreshTokenSync: POST %s/api/auth/refresh", qPrintable(base));
    QNetworkReply* rep = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QObject::connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const int code = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError nerr = rep->error();
    const QByteArray raw = rep->readAll();
    rep->deleteLater();

    if (code == 401 || code == 403) {
        qCWarning(lcAuth, "refreshTokenSync: HTTP %d — clearing session", code);
        QMutexLocker lock(&m_mutex);
        clearFile();
        return false;
    }
    if (nerr != QNetworkReply::NoError) {
        qCWarning(lcAuth, "refreshTokenSync: network error code %d", int(nerr));
        return false;
    }

    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcAuth, "refreshTokenSync: bad JSON %s", qPrintable(pe.errorString()));
        return false;
    }
    const QJsonObject root = doc.object();
    if (!root.value(QStringLiteral("success")).toBool(false)) {
        qCWarning(lcAuth, "refreshTokenSync: success=false");
        return false;
    }
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    if (data.isEmpty()) {
        qCWarning(lcAuth, "refreshTokenSync: empty data");
        return false;
    }

    QMutexLocker lock(&m_mutex);
    applyJsonObject(data);
    const bool saved = saveToFile();
    qCInfo(lcAuth, "refreshTokenSync: saved=%s", saved ? "yes" : "no");
    return saved;
}

bool AuthSession::ensureValidAccessToken()
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_tokens.token.isEmpty()) {
            qCWarning(lcAuth, "ensureValidAccessToken: no access token");
            return false;
        }
        if (isAccessValid()) {
            qCDebug(lcAuth, "ensureValidAccessToken: access still valid");
            return true;
        }
        qCInfo(lcAuth, "ensureValidAccessToken: access expired or invalid — refreshing");
    }
    QMutexLocker refreshLock(&m_refreshMutex);
    {
        QMutexLocker lock(&m_mutex);
        if (isAccessValid()) {
            qCDebug(lcAuth, "ensureValidAccessToken: access valid after waiting for refresh");
            return true;
        }
    }
    if (!refreshTokenSync()) {
        qCWarning(lcAuth, "ensureValidAccessToken: refresh failed");
        return false;
    }
    QMutexLocker lock(&m_mutex);
    if (isAccessValid()) {
        qCInfo(lcAuth, "ensureValidAccessToken: ok after refresh");
        return true;
    }
    bumpAccessExpirationIfInvalid();
    const bool saved = saveToFile();
    if (isAccessValid()) {
        qCInfo(lcAuth, "ensureValidAccessToken: ok after normalizing expiration (saved=%s)", saved ? "yes" : "no");
        return true;
    }
    qCWarning(lcAuth,
        "ensureValidAccessToken: refresh succeeded but access still invalid (check JWT exp vs API expiration; saved=%s)",
        saved ? "yes" : "no");
    return false;
}

void AuthSession::setFromLoginJson(const QJsonObject& data)
{
    QMutexLocker lock(&m_mutex);
    applyJsonObject(data);
    saveToFile();
}

void AuthSession::logout()
{
    QMutexLocker lock(&m_mutex);
    clearFile();
}
