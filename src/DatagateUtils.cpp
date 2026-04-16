#include "DatagateUtils.h"
#include "DatagateTr.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QIODevice>
#include <QAbstractSocket>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QVector>

#include <algorithm>
#include <cmath>

#if defined(__linux__)
#include <filesystem>
#if defined(DATAGATE_HAVE_LIBCAP)
#include <sys/capability.h>
#endif
#endif

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

static QString datagateSanitizeOpenVpnSetenvToken(const QString& raw)
{
    QString out;
    out.reserve(raw.size());
    for (QChar c : raw) {
        if (c.isLetterOrNumber() || c == QLatin1Char('.') || c == QLatin1Char('-') || c == QLatin1Char('_')) {
            out += c;
        } else {
            out += QLatin1Char('_');
        }
    }
    return out;
}

static QString datagateOpenVpnSemverFromExe(const QString& resolvedExe)
{
    static QHash<QString, QString> cache;
    const auto it = cache.constFind(resolvedExe);
    if (it != cache.cend()) {
        return *it;
    }
    QString result = QStringLiteral("unknown");
    QProcess p;
    p.setProgram(resolvedExe);
    p.setArguments({QStringLiteral("--version")});
    p.setProcessChannelMode(QProcess::SeparateChannels);
    p.start();
    if (p.waitForFinished(6000) && p.exitCode() == 0) {
        const QString out = QString::fromUtf8(p.readAllStandardOutput());
        static const QRegularExpression re(QStringLiteral(R"(OpenVPN\s+(\d+\.\d+(?:\.\d+)?))"));
        const QRegularExpressionMatch m = re.match(out);
        if (m.hasMatch()) {
            result = m.captured(1);
        }
    }
    cache.insert(resolvedExe, result);
    return result;
}

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

QString datagateLinuxPeerVersionLabel(const QString& resolvedOpenVpnExecutable)
{
    const QString ov = datagateOpenVpnSemverFromExe(resolvedOpenVpnExecutable);
    QString app = QCoreApplication::applicationVersion().trimmed();
    if (app.isEmpty()) {
        app = QStringLiteral("unknown");
    }
    return datagateSanitizeOpenVpnSetenvToken(QStringLiteral("%1_datagate_linux_%2").arg(ov, app));
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

QString userMessageWhenApiUnavailable(QNetworkReply* rep)
{
    if (!rep) {
        return Datagate::tr("The API is temporarily unavailable. Try again later.");
    }
    const int http = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (http >= 500 && http < 600) {
        return Datagate::tr("The API is temporarily unavailable. Try again later.");
    }
    const QNetworkReply::NetworkError err = rep->error();
    if (err == QNetworkReply::NoError) {
        return {};
    }
    switch (err) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::SslHandshakeFailedError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::ProtocolUnknownError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
        return Datagate::tr("The API is temporarily unavailable. Try again later.");
    default:
        break;
    }
    return {};
}

#if defined(__linux__)

QString linuxFileGetcapLine(const QString& canonicalExecutablePath)
{
    QFileInfo fi(canonicalExecutablePath);
    if (!fi.exists() || !fi.isExecutable()) {
        return {};
    }
    const QString path = fi.canonicalFilePath();
    QProcess p;
    p.start(QStringLiteral("getcap"), QStringList{path});
    if (!p.waitForFinished(8000)) {
        return Datagate::tr("(getcap timed out)");
    }
    if (p.error() == QProcess::FailedToStart) {
        return Datagate::tr("(getcap not found — install libcap2-bin)");
    }
    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (p.exitCode() != 0 && out.isEmpty()) {
        return err.isEmpty() ? Datagate::tr("(getcap failed)") : err;
    }
    if (out.isEmpty() && err.isEmpty()) {
        return Datagate::tr("(no file capabilities — run Grant or sudo setcap)");
    }
    return out.isEmpty() ? err : out;
}

int linuxProcSelfTracerPid()
{
    QFile st(QStringLiteral("/proc/self/status"));
    if (!st.open(QIODevice::ReadOnly)) {
        return -1;
    }
    const QByteArray raw = st.readAll();
    for (const QByteArray& line : raw.split('\n')) {
        if (line.startsWith("TracerPid:")) {
            return line.mid(line.indexOf(':') + 1).trimmed().toInt();
        }
    }
    return -1;
}

namespace {

quint64 parseCapStatusField(const QByteArray& line)
{
    const int tab = line.indexOf('\t');
    if (tab < 0) {
        return 0;
    }
    QByteArray hex = line.mid(tab + 1).trimmed();
    const int sp = hex.indexOf(' ');
    if (sp > 0) {
        hex = hex.left(sp);
    }
    bool ok = false;
    const quint64 v = hex.toULongLong(&ok, 16);
    return ok ? v : 0;
}

} // namespace

bool linuxTryRaiseEffectiveCapNetAdmin()
{
#if !defined(DATAGATE_HAVE_LIBCAP)
    return false;
#else
    cap_t c = cap_get_proc();
    if (!c) {
        return false;
    }
    cap_flag_value_t fv = CAP_CLEAR;
    if (cap_get_flag(c, CAP_NET_ADMIN, CAP_PERMITTED, &fv) != 0 || fv != CAP_SET) {
        cap_free(c);
        return false;
    }
    if (cap_get_flag(c, CAP_NET_ADMIN, CAP_EFFECTIVE, &fv) == 0 && fv == CAP_SET) {
        cap_free(c);
        return true;
    }
    cap_value_t one = CAP_NET_ADMIN;
    if (cap_set_flag(c, CAP_EFFECTIVE, 1, &one, CAP_SET) != 0) {
        cap_free(c);
        return false;
    }
    const int r = cap_set_proc(c);
    cap_free(c);
    if (r != 0) {
        return false;
    }
    qCInfo(lcUtils, "CAP_NET_ADMIN was permitted but not effective — raised effective set (libcap)");
    return true;
#endif
}

QString linuxEmbeddedVpnTunDiagnosis(const QString& qAppExecutablePath)
{
    QString procExe;
    std::error_code ec;
    const std::filesystem::path sym = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        try {
            procExe = QString::fromStdString(std::filesystem::canonical(sym).string());
        } catch (...) {
            procExe = QString::fromStdString(sym.string());
        }
    }
    const QFileInfo fiQt(qAppExecutablePath);
    const QString qtCanon = fiQt.exists() ? fiQt.canonicalFilePath() : qAppExecutablePath;

    QFile st(QStringLiteral("/proc/self/status"));
    quint64 capEff = 0;
    quint64 capPrm = 0;
    int noNewPrivs = -1;
    int tracerPid = -1;
    const bool statusReadOk = st.open(QIODevice::ReadOnly);
    if (statusReadOk) {
        const QByteArray raw = st.readAll();
        for (const QByteArray& line : raw.split('\n')) {
            if (line.startsWith("CapEff:")) {
                capEff = parseCapStatusField(line);
            } else if (line.startsWith("CapPrm:")) {
                capPrm = parseCapStatusField(line);
            } else if (line.startsWith("NoNewPrivs:")) {
                noNewPrivs = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            } else if (line.startsWith("TracerPid:")) {
                tracerPid = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            }
        }
    }

    QString prefix;
    if (tracerPid > 0) {
        prefix = Datagate::tr(
            "——————\n"
            "DEBUGGER / PTRACE (TracerPid is %1 — not 0)\n\n"
            "When the process is under a debugger (ptrace), Linux usually does not give it file capabilities from "
            "setcap: CapPrm and CapEff stay 0 even if getcap on the binary shows cap_net_admin.\n\n"
            "This is not about root or “superuser”, and sudo on the whole app is not the right fix.\n\n"
            "What to do: stop debugging this run — close the IDE “attach debugger” session, or use Run without "
            "debugging / Start without debugger, or run the binary from a normal terminal:\n"
            "  %2\n"
            "——————\n\n")
            .arg(tracerPid)
            .arg(qtCanon);
    }

    QString out;
    out += QStringLiteral("Running binary (/proc/self/exe): %1\n").arg(procExe.isEmpty() ? QStringLiteral("(unknown)") : procExe);
    out += QStringLiteral("QCoreApplication::applicationFilePath: %1\n").arg(qtCanon);
    out += QStringLiteral("Paths match: %1\n\n").arg(!procExe.isEmpty() && procExe == qtCanon ? QStringLiteral("yes")
                                                                                              : QStringLiteral("no"));

    if (!statusReadOk) {
        out += QStringLiteral("(could not read /proc/self/status)\n\n");
    }

    constexpr quint64 kNetAdmin = (1ULL << 12);
    const bool eff = (capEff & kNetAdmin) != 0;
    const bool prm = (capPrm & kNetAdmin) != 0;
    out += QStringLiteral("CapEff=0x%1 — CAP_NET_ADMIN effective: %2\n")
        .arg(capEff, 0, 16)
        .arg(eff ? QStringLiteral("yes") : QStringLiteral("NO ← TUN needs this"));
    out += QStringLiteral("CapPrm=0x%1 — CAP_NET_ADMIN permitted: %2\n")
        .arg(capPrm, 0, 16)
        .arg(prm ? QStringLiteral("yes") : QStringLiteral("no"));
    if (noNewPrivs >= 0) {
        out += QStringLiteral("NoNewPrivs: %1\n").arg(noNewPrivs);
    }
    if (tracerPid > 0) {
        out += QStringLiteral("TracerPid: %1 (debugger — see block above)\n").arg(tracerPid);
    }
    out += QLatin1Char('\n');
    const QString forGetcap = procExe.isEmpty() ? qtCanon : procExe;
    out += QStringLiteral("getcap (running exe):\n%1\n").arg(linuxFileGetcapLine(forGetcap));
#if !defined(DATAGATE_HAVE_LIBCAP)
    out += QStringLiteral(
        "\nInstall libcap-dev (or libcap-devel) and rebuild — DataGate can then promote permitted CAP_NET_ADMIN to "
        "effective when needed.\n");
#endif
    return prefix + out;
}

#endif

} // namespace DatagateUtils
