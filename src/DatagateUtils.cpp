#include "DatagateUtils.h"

#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QAbstractSocket>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QProcess>
#include <QSet>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QVector>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcUtils, "datagate.utils")

namespace {

QString base64UrlDecodePayload(const QString& jwt)
{
    const QStringList parts = jwt.split(QLatin1Char('.'));
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

QString randomTokenUrlSafe(int length)
{
    for (;;) {
        const int byteLen = static_cast<int>(std::ceil(length * 0.75));
        QByteArray bytes(byteLen, 0);
        QFile urandom(QStringLiteral("/dev/urandom"));
        if (urandom.open(QIODevice::ReadOnly) && urandom.read(bytes.data(), byteLen) == byteLen) {
            // ok
        } else {
            for (int i = 0; i < byteLen; ++i) {
                bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
            }
        }
        QString s = QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
        if (s.length() < length) {
            continue;
        }
        s = s.left(length);
        if (!s.endsWith(QLatin1Char('-'))) {
            return s;
        }
    }
}

bool lineStartsWithToken(const QString& line, const QString& token)
{
    int i = 0;
    while (i < line.size() && (line.at(i).isSpace() || line.at(i) == QLatin1Char('\t'))) {
        ++i;
    }
    if (line.size() < i + token.size()) {
        return false;
    }
    if (!line.mid(i).startsWith(token, Qt::CaseInsensitive)) {
        return false;
    }
    if (line.size() == i + token.size()) {
        return true;
    }
    const QChar c = line.at(i + token.size());
    return c.isSpace() || c == QLatin1Char('\t');
}

} // namespace

namespace DatagateUtils {

void releaseStaleBridgePort(quint16 port)
{
#if defined(__linux__)
    const QStringList specs = {
        QStringLiteral("%1/udp").arg(port),
        QStringLiteral("%1/tcp").arg(port),
    };
    for (const QString& spec : specs) {
        QProcess p;
        p.start(QStringLiteral("fuser"), QStringList{QStringLiteral("-k"), spec});
        if (!p.waitForStarted(3000)) {
            qCDebug(lcUtils) << "releaseStaleBridgePort: fuser not available";
            continue;
        }
        p.waitForFinished(4000);
        if (p.exitCode() == 0) {
            qCInfo(lcUtils) << "releaseStaleBridgePort: freed socket" << spec;
        }
    }
#else
    Q_UNUSED(port);
#endif
}

QString getOrCreateInstallationId()
{
    QSettings s;
    QString v = s.value(QStringLiteral("installationId")).toString();
    if (v.length() == 22) {
        return v;
    }
    v = randomTokenUrlSafe(22);
    s.setValue(QStringLiteral("installationId"), v);
    return v;
}

QString installationIdFromSettings()
{
    return getOrCreateInstallationId();
}

void ensureInstallationId()
{
    (void)getOrCreateInstallationId();
}

QString externalIdFromJwt(const QString& accessToken)
{
    if (accessToken.isEmpty()) {
        return {};
    }
    const QString payload = base64UrlDecodePayload(accessToken);
    if (payload.isEmpty()) {
        return {};
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    const QJsonObject o = doc.object();
    const QString ext = o.value(QStringLiteral("externalId")).toString();
    if (!ext.isEmpty()) {
        return ext;
    }
    const QString sub = o.value(QStringLiteral("sub")).toString();
    if (!sub.isEmpty()) {
        return sub;
    }
    return o.value(QStringLiteral("nameid")).toString();
}

QString tryGetProtoFromOvpn(const QString& ovpnUtf8)
{
    const QString normalized = QString(ovpnUtf8).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    const QStringList lines = normalized.split(QLatin1Char('\n'));
    for (QString raw : lines) {
        if (!raw.isEmpty() && raw.back() == QLatin1Char('\r')) {
            raw.chop(1);
        }
        const QString t = raw.trimmed();
        if (t.isEmpty() || t.startsWith(QLatin1Char('#')) || t.startsWith(QLatin1Char(';'))) {
            continue;
        }
        if (lineStartsWithToken(t, QStringLiteral("proto"))) {
            const QStringList parts = t.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                return parts[1].toLower();
            }
        }
    }
    return {};
}

ParsedRemote parseFirstRemoteFromOvpn(const QString& ovpnUtf8)
{
    ParsedRemote r;
    const QString normalized = QString(ovpnUtf8).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    const QStringList lines = normalized.split(QLatin1Char('\n'));
    for (QString raw : lines) {
        if (!raw.isEmpty() && raw.back() == QLatin1Char('\r')) {
            raw.chop(1);
        }
        const QString t = raw.trimmed();
        if (t.isEmpty() || t.startsWith(QLatin1Char('#')) || t.startsWith(QLatin1Char(';'))) {
            continue;
        }
        if (!lineStartsWithToken(t, QStringLiteral("remote"))) {
            continue;
        }
        const QStringList parts = t.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            break;
        }
        r.host = parts[1];
        if (parts.size() >= 3) {
            bool ok = false;
            const quint16 p = parts[2].toUShort(&ok);
            if (ok) {
                r.port = p;
            }
        }
        break;
    }
    return r;
}

QString openVpnRouteBypassNetGatewayLines(const QStringList& hostnames)
{
    QSet<QString> seenIps;
    QString out;
    for (const QString& hostname : hostnames) {
        const QString h = hostname.trimmed();
        if (h.isEmpty()) {
            continue;
        }
        const QHostInfo hi = QHostInfo::fromName(h);
        if (hi.error() != QHostInfo::NoError) {
            qCWarning(lcUtils) << "openVpnRouteBypass: DNS failed for" << h << hi.errorString();
            continue;
        }
        for (const QHostAddress& addr : hi.addresses()) {
            if (addr.protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            const QString ip = addr.toString();
            if (seenIps.contains(ip)) {
                continue;
            }
            seenIps.insert(ip);
            out += QStringLiteral("route %1 255.255.255.255 net_gateway\n").arg(ip);
        }
    }
    if (out.isEmpty() && !hostnames.isEmpty()) {
        qCWarning(lcUtils) << "openVpnRouteBypass: no IPv4 routes generated for hosts" << hostnames;
    }
    return out;
}

QString patchOvpnRemoteToLocal(const QString& ovpnUtf8, const QString& localHost, quint16 localPort,
    bool ignoreRedirectGateway, const QString& routeBypassLines)
{
    QString patched;
    patched.reserve(ovpnUtf8.size() + 256);
    patched += QStringLiteral("remote ");
    patched += localHost;
    patched += QLatin1Char(' ');
    patched += QString::number(localPort);
    patched += QLatin1Char('\n');
    if (ignoreRedirectGateway) {
        patched += QStringLiteral("pull-filter ignore \"redirect-gateway\"\n");
    }
    if (!routeBypassLines.isEmpty()) {
        patched += routeBypassLines;
    }

    const QString normalized = QString(ovpnUtf8).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    const QStringList lines = normalized.split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (!line.isEmpty() && line.back() == QLatin1Char('\r')) {
            line.chop(1);
        }
        if (lineStartsWithToken(line, QStringLiteral("remote"))) {
            continue;
        }
        patched += line;
        patched += QLatin1Char('\n');
    }
    return patched;
}

QString httpsToWssProxyBase(const QString& baseApiUrl)
{
    QUrl u(baseApiUrl.trimmed());
    if (!u.isValid()) {
        return {};
    }
    const QString scheme = u.scheme().toLower();
    if (scheme == QStringLiteral("https")) {
        u.setScheme(QStringLiteral("wss"));
    } else if (scheme == QStringLiteral("http")) {
        u.setScheme(QStringLiteral("ws"));
    } else {
        return {};
    }
    u.setPath(QStringLiteral("/api/proxy"));
    u.setQuery(QUrlQuery());
    u.setFragment(QString());
    return u.toString();
}

QUrl wssProxyUrl(const QString& baseApiUrl, bool udpMode)
{
    const QString base = httpsToWssProxyBase(baseApiUrl);
    if (base.isEmpty()) {
        return {};
    }
    QUrl u(base);
    QUrlQuery q(u.query());
    if (udpMode && !q.hasQueryItem(QStringLiteral("mode"))) {
        q.addQueryItem(QStringLiteral("mode"), QStringLiteral("udp"));
    }
    u.setQuery(q);
    return u;
}

void saveLastSelectedServerId(int id)
{
    QSettings s;
    s.setValue(QStringLiteral("lastSelectedServerId"), id);
}

int loadLastSelectedServerId()
{
    QSettings s;
    return s.value(QStringLiteral("lastSelectedServerId"), -1).toInt();
}

QString resolvedOpenVpnExecutable(const QString& openVpnCmdOrPath)
{
    QString s = openVpnCmdOrPath.trimmed();
    if (s.isEmpty()) {
        s = QStringLiteral("openvpn");
    }
    QFileInfo fi(s);
    if (fi.isAbsolute()) {
        return fi.exists() ? fi.canonicalFilePath() : s;
    }
    const QString found = QStandardPaths::findExecutable(s);
    if (!found.isEmpty()) {
        return found;
    }
    return s;
}

namespace {

std::optional<QVector<BestServer>> parseWssServersFromStatusJson(const QByteArray& jsonBody)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBody, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcUtils) << "parseWssServersFromStatusJson: invalid JSON" << err.errorString();
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    if (!root.value(QStringLiteral("success")).toBool(false)) {
        qCWarning(lcUtils) << "parseWssServersFromStatusJson: success=false"
                           << root.value(QStringLiteral("message")).toString();
        return std::nullopt;
    }
    const QJsonArray arr = root.value(QStringLiteral("data")).toObject()
        .value(QStringLiteral("openVpnServerWithStatuses")).toArray();

    QVector<BestServer> ranked;
    ranked.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        const QJsonObject item = v.toObject();
        const QJsonObject serverObj = item.value(QStringLiteral("openVpnServerResponses")).toObject()
            .value(QStringLiteral("openVpnServer")).toObject();
        if (!serverObj.value(QStringLiteral("isEnableWss")).toBool(false)) {
            continue;
        }
        const int id = serverObj.value(QStringLiteral("id")).toInt(-1);
        if (id < 0) {
            continue;
        }
        BestServer b;
        b.serverId = id;
        b.name = serverObj.value(QStringLiteral("serverName")).toString().trimmed();
        if (b.name.isEmpty()) {
            b.name = QStringLiteral("Server #%1").arg(b.serverId);
        }
        b.apiUrl = serverObj.value(QStringLiteral("apiUrl")).toString();
        if (b.apiUrl.isEmpty()) {
            continue;
        }
        b.isOnline = serverObj.value(QStringLiteral("isOnline")).toBool(false);
        b.countConnectedClients = item.value(QStringLiteral("countConnectedClients")).toInt(0);
        if (b.countConnectedClients < 0) {
            b.countConnectedClients = 0;
        }
        ranked.push_back(b);
    }
    if (ranked.isEmpty()) {
        return std::nullopt;
    }
    return ranked;
}

} // namespace

std::optional<BestServer> pickBestServerWinStyle(const QByteArray& jsonBody)
{
    const auto rankedOpt = parseWssServersFromStatusJson(jsonBody);
    if (!rankedOpt.has_value()) {
        return std::nullopt;
    }
    QVector<BestServer> ranked = *rankedOpt;

    std::sort(ranked.begin(), ranked.end(), [](const BestServer& a, const BestServer& b) {
        if (a.isOnline != b.isOnline) {
            return a.isOnline > b.isOnline;
        }
        return a.countConnectedClients > b.countConnectedClients;
    });

    const int last = loadLastSelectedServerId();
    int index = 0;
    if (ranked.size() > 1 && last >= 0) {
        for (int i = 0; i < ranked.size(); ++i) {
            if (ranked[i].serverId == last) {
                index = (i + 1) % ranked.size();
                break;
            }
        }
    }
    const BestServer chosen = ranked[index];
    saveLastSelectedServerId(chosen.serverId);
    return chosen;
}

QVector<QPair<int, QString>> listWssServersFromStatusJson(const QByteArray& jsonBody)
{
    const auto rankedOpt = parseWssServersFromStatusJson(jsonBody);
    if (!rankedOpt.has_value()) {
        return {};
    }
    QVector<QPair<int, QString>> out;
    out.reserve(rankedOpt->size());
    for (const BestServer& b : *rankedOpt) {
        out.push_back(qMakePair(b.serverId, b.name));
    }
    std::sort(out.begin(), out.end(), [](const QPair<int, QString>& a, const QPair<int, QString>& b) {
        return a.second.localeAwareCompare(b.second) < 0;
    });
    return out;
}

std::optional<BestServer> pickServerByIdFromStatusJson(const QByteArray& jsonBody, int serverId)
{
    const auto rankedOpt = parseWssServersFromStatusJson(jsonBody);
    if (!rankedOpt.has_value()) {
        return std::nullopt;
    }
    for (const BestServer& b : *rankedOpt) {
        if (b.serverId == serverId) {
            saveLastSelectedServerId(b.serverId);
            return b;
        }
    }
    return std::nullopt;
}

} // namespace DatagateUtils
