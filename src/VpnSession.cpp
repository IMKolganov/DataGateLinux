#include "VpnSession.h"

#include "AppConfig.h"
#include "DatagateTr.h"
#include "DatagateUtils.h"
#include "UdpWssBridge.h"
#include "WssTcpBridge.h"

#if defined(DATAGATE_EMBEDDED_OPENVPN3)
#include "DatagateOvpn3Client.h"
#include <thread>
#endif

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStringList>
#include <QTemporaryFile>
#include <QUrl>

Q_LOGGING_CATEGORY(lcVpn, "datagate.vpn")

namespace {

QString joinUrl(QString base, const QString& path)
{
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    QString p = path;
    if (p.startsWith(QLatin1Char('/'))) {
        p = p.mid(1);
    }
    return base + QLatin1Char('/') + p;
}

// Do not ioctl(TUNSETIFF) in the DataGate process: only the openvpn child gets file capabilities
// (setcap). The GUI would always fail EPERM here even when /usr/sbin/openvpn is correctly capped.

bool logLooksLikeTunPermissionDenied(const QString& log)
{
    return log.contains(QStringLiteral("TUNSETIFF"), Qt::CaseInsensitive)
        && log.contains(QStringLiteral("Operation not permitted"), Qt::CaseInsensitive);
}

} // namespace

VpnSession::VpnSession(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_tcpBridge(new WssTcpBridge(this))
    , m_udpBridge(new UdpWssBridge(this))
    , m_ovpn(new QProcess(this))
{
    connect(m_ovpn, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        &VpnSession::onOpenVpnFinished);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_ovpn, &QProcess::errorOccurred, this, &VpnSession::onOpenVpnError);
#else
    connect(m_ovpn, QOverload<QProcess::ProcessError>::of(&QProcess::error), this, &VpnSession::onOpenVpnError);
#endif
    connect(m_ovpn, &QProcess::readyReadStandardOutput, this, [this]() {
        if (!m_ovpn) {
            return;
        }
        const QByteArray chunk = m_ovpn->readAllStandardOutput();
        if (chunk.isEmpty()) {
            return;
        }
        m_ovpnLogBuffer.append(chunk);
        qCDebug(lcVpn).nospace() << "openvpn: " << chunk;
    });
}

VpnSession::~VpnSession()
{
    disconnectVpnInternal(true, "~VpnSession (app exit, window closed, or MainWindow destroyed e.g. logout)");
}

void VpnSession::abortPendingReplies()
{
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply.clear();
    }
}

void VpnSession::connectVpn(const QString& backendBaseUrl, const QString& bearerAccessToken,
    const QString& openVpnExecutable, bool autoPickServer, int manualServerId)
{
#if defined(DATAGATE_EMBEDDED_OPENVPN3)
    if (m_ovpn3Thread && m_ovpn3Thread->joinable()) {
        emit errorMessage(Datagate::tr("Disconnect the current OpenVPN session first."));
        return;
    }
#endif
    if (m_ovpn->state() != QProcess::NotRunning) {
        emit errorMessage(Datagate::tr("Disconnect the current OpenVPN session first."));
        return;
    }
    if (m_connecting) {
        emit errorMessage(Datagate::tr("Connection already in progress."));
        return;
    }
    abortPendingReplies();
    m_connecting = true;
    m_userStopVpn = false;
    m_autoPickServer = autoPickServer;
    m_manualServerId = manualServerId;
    m_baseUrl = backendBaseUrl.trimmed();
    m_token = bearerAccessToken.trimmed();
    m_openVpnExe = DatagateUtils::resolvedOpenVpnExecutable(openVpnExecutable);

    DatagateUtils::ensureInstallationId();
    const QString ext = DatagateUtils::externalIdFromJwt(m_token);
    if (ext.isEmpty()) {
        m_connecting = false;
        emit errorMessage(Datagate::tr("Could not read externalId from JWT (need sub, externalId, or nameid)."));
        return;
    }

    emit statusMessage(Datagate::tr("Requesting server list…"));
    stepFetchServers();
}

void VpnSession::stepFetchServers()
{
    QUrl url(joinUrl(m_baseUrl, QStringLiteral("api/open-vpn-servers/get-all-with-status")));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_token).toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_activeReply == reply) {
            m_activeReply.clear();
        }
        reply->deleteLater();
        if (!m_connecting) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            m_connecting = false;
            const int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString detail = reply->errorString();
            if (httpCode > 0) {
                detail = QStringLiteral("HTTP %1 — %2").arg(httpCode).arg(detail);
            }
            qCWarning(lcVpn, "get-all-with-status failed: %s", qPrintable(detail));
            emit errorMessage(Datagate::tr("Servers: %1").arg(detail));
            return;
        }
        const QByteArray body = reply->readAll();
        std::optional<BestServer> best;
        if (m_autoPickServer) {
            best = DatagateUtils::pickBestServerWinStyle(body);
        } else {
            best = DatagateUtils::pickServerByIdFromStatusJson(body, m_manualServerId);
            if (!best.has_value()) {
                m_connecting = false;
                emit errorMessage(
                    Datagate::tr("Server id %1 not found or is not WSS-enabled (refresh the list on Access).")
                        .arg(m_manualServerId));
                return;
            }
        }
        if (!best.has_value()) {
            m_connecting = false;
            emit errorMessage(Datagate::tr("No WSS-enabled servers available."));
            return;
        }
        m_server = *best;
        emit statusMessage(Datagate::tr("Selected server: %1").arg(m_server.name));

        const QString installId = DatagateUtils::installationIdFromSettings();
        const QString ext = DatagateUtils::externalIdFromJwt(m_token);
        m_commonName = QStringLiteral("ldg-%1-%2-%3")
            .arg(m_server.serverId)
            .arg(ext)
            .arg(installId);

        stepTryDownload(/*afterCreate=*/false);
    });
}

void VpnSession::stepTryDownload(bool afterCreate)
{
    emit statusMessage(Datagate::tr("Downloading OpenVPN profile…"));

    QUrl url(joinUrl(m_baseUrl, QStringLiteral("api/open-vpn-files/download-file-by-cn")));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_token.toUtf8());

    QJsonObject body;
    body.insert(QStringLiteral("vpnServerId"), m_server.serverId);
    body.insert(QStringLiteral("commonName"), m_commonName);
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = m_nam->post(req, payload);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, afterCreate]() {
        if (m_activeReply == reply) {
            m_activeReply.clear();
        }
        reply->deleteLater();
        if (!m_connecting) {
            return;
        }
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();

        if (reply->error() != QNetworkReply::NoError && code == 0) {
            m_connecting = false;
            emit errorMessage(Datagate::tr("Download .ovpn: %1").arg(reply->errorString()));
            return;
        }

        if (code == 200) {
            QJsonParseError jerr{};
            const QJsonDocument doc = QJsonDocument::fromJson(raw, &jerr);
            if (jerr.error != QJsonParseError::NoError || !doc.isObject()) {
                m_connecting = false;
                emit errorMessage(Datagate::tr("Invalid JSON while loading profile."));
                return;
            }
            const QJsonObject root = doc.object();
            const QJsonObject data = root.value(QStringLiteral("data")).toObject();
            const QString b64 = data.value(QStringLiteral("content")).toString();
            if (b64.isEmpty()) {
                m_connecting = false;
                emit errorMessage(Datagate::tr("Response has no content (base64)."));
                return;
            }
            const QByteArray ovpnBytes = QByteArray::fromBase64(b64.toUtf8());
            const QString configText = QString::fromUtf8(ovpnBytes);
            stepStartBridgeAndOpenVpn(configText);
            return;
        }

        if ((code == 404 || code == 400) && !afterCreate) {
            const QString txt = QString::fromUtf8(raw);
            const bool notFound = code == 404
                || (txt.contains(QStringLiteral("not found"), Qt::CaseInsensitive)
                    && txt.contains(QStringLiteral("success"), Qt::CaseInsensitive));
            if (notFound) {
                stepCreateOnServer();
                return;
            }
        }

        m_connecting = false;
        emit errorMessage(Datagate::tr("Profile download failed (HTTP %1). %2").arg(code).arg(QString::fromUtf8(raw)));
    });
}

void VpnSession::stepCreateOnServer()
{
    emit statusMessage(Datagate::tr("Creating profile on server…"));

    QUrl url(joinUrl(m_baseUrl, QStringLiteral("api/open-vpn-files/add-with-token")));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    req.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_token.toUtf8());

    const QString ext = DatagateUtils::externalIdFromJwt(m_token);
    const QString installId = DatagateUtils::installationIdFromSettings();

    QJsonObject body;
    body.insert(QStringLiteral("vpnServerId"), m_server.serverId);
    body.insert(QStringLiteral("commonName"), m_commonName);
    body.insert(QStringLiteral("externalId"), ext);
    const QString appVer = QCoreApplication::applicationVersion().trimmed();
    body.insert(QStringLiteral("issuedTo"),
        QStringLiteral("datagate linux app %1 user %2 device %3")
            .arg(appVer.isEmpty() ? QStringLiteral("unknown") : appVer, ext, installId));
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = m_nam->post(req, payload);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_activeReply == reply) {
            m_activeReply.clear();
        }
        reply->deleteLater();
        if (!m_connecting) {
            return;
        }
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || code < 200 || code >= 300) {
            m_connecting = false;
            emit errorMessage(
                Datagate::tr("Create profile: HTTP %1 %2").arg(code).arg(QString::fromUtf8(reply->readAll())));
            return;
        }
        stepTryDownload(/*afterCreate=*/true);
    });
}

void VpnSession::stopBridges()
{
    m_tcpBridge->stop();
    m_udpBridge->stop();
}

void VpnSession::stepStartBridgeAndOpenVpn(const QString& configText)
{
    const QString proto = DatagateUtils::tryGetProtoFromOvpn(configText).toLower();
    const bool useUdp = proto.startsWith(QStringLiteral("udp"));
    const ParsedRemote pr = DatagateUtils::parseFirstRemoteFromOvpn(configText);

    const QUrl wss = DatagateUtils::wssProxyUrl(m_server.apiUrl, useUdp);
    if (wss.isEmpty() || !wss.isValid()) {
        m_connecting = false;
        emit errorMessage(Datagate::tr("Invalid server apiUrl."));
        return;
    }

    DatagateUtils::releaseStaleBridgePort(DatagateUtils::kDefaultBridgePort);
    stopBridges();

    const quint16 bridgePort = DatagateUtils::kDefaultBridgePort;
    bool bridgeOk = false;
    if (useUdp) {
        bridgeOk = m_udpBridge->start(bridgePort, wss, pr.host, pr.port, AppConfig::udpBridgeFramed());
    } else {
        bridgeOk = m_tcpBridge->start(bridgePort, wss);
    }

    if (!bridgeOk) {
        m_connecting = false;
        emit errorMessage(
            Datagate::tr("Could not bind local port %1 for the WSS bridge. Another process may still hold it "
               "(e.g. orphan openvpn after killing DataGate). On Linux try: fuser -k %1/udp ; fuser -k %1/tcp")
                .arg(bridgePort));
        return;
    }

    const bool ignoreRg = AppConfig::openVpnIgnoreRedirectGateway();
    if (ignoreRg) {
        qCInfo(lcVpn,
            "OpenVPN: IgnoreRedirectGateway enabled — server push redirect-gateway will be ignored "
            "(see appsettings OpenVpn.IgnoreRedirectGateway)");
    }

    // redirect-gateway sends all traffic via tun; WSS to the proxy and HTTPS to Api:BaseUrl must stay on LAN.
    QStringList bypassHosts;
    bypassHosts << wss.host();
    const QUrl apiBase(AppConfig::apiBaseUrl().trimmed());
    if (apiBase.isValid() && !apiBase.host().isEmpty() && apiBase.host() != wss.host()) {
        bypassHosts << apiBase.host();
    }
    const QString routeBypass = DatagateUtils::openVpnRouteBypassNetGatewayLines(bypassHosts);
    if (!routeBypass.isEmpty()) {
        qCInfo(lcVpn, "OpenVPN: route bypass for WSS/API (net_gateway):\n%s", qPrintable(routeBypass.trimmed()));
    }

    const QString patched = DatagateUtils::patchOvpnRemoteToLocal(
        configText, QStringLiteral("127.0.0.1"), bridgePort, ignoreRg, routeBypass);

#if defined(DATAGATE_EMBEDDED_OPENVPN3)
    if (!AppConfig::openVpnUseSystemBinary()) {
        startEmbeddedOpenVpn3(patched, useUdp, bridgePort);
        return;
    }
#endif

    QString ovpnFileBody = patched;
    {
        const QString peerLabel = DatagateUtils::datagateLinuxPeerVersionLabel(m_openVpnExe);
        if (!ovpnFileBody.contains(QLatin1String("push-peer-info"), Qt::CaseInsensitive)) {
            ovpnFileBody += QStringLiteral("\npush-peer-info\n");
        }
        ovpnFileBody += QStringLiteral("\n# DataGate client version (sent as peer-info UV_DATAGATE_APP)\nsetenv "
                                         "UV_DATAGATE_APP ");
        ovpnFileBody += peerLabel;
        ovpnFileBody += QLatin1Char('\n');
        qCInfo(lcVpn, "OpenVPN peer-info: UV_DATAGATE_APP=%s", qPrintable(peerLabel));
    }

    delete m_configFile;
    m_configFile = new QTemporaryFile(this);
    m_configFile->setFileTemplate(QStringLiteral("datagate-XXXXXX.ovpn"));
    if (!m_configFile->open()) {
        stopBridges();
        m_connecting = false;
        emit errorMessage(Datagate::tr("Could not create temporary config file."));
        return;
    }
    m_configFile->write(ovpnFileBody.toUtf8());
    m_configFile->flush();

    emit statusMessage(useUdp ? Datagate::tr("UDP↔WSS bridge on port %1…").arg(bridgePort)
                              : Datagate::tr("TCP↔WSS bridge on port %1…").arg(bridgePort));
    emit statusMessage(Datagate::tr("Starting OpenVPN…"));

    QStringList args;
    args << QStringLiteral("--config") << m_configFile->fileName();

    m_ovpnLogBuffer.clear();
    m_ovpn->setProgram(m_openVpnExe);
    m_ovpn->setArguments(args);
    m_ovpn->setProcessChannelMode(QProcess::MergedChannels);
    qCInfo(lcVpn, "OpenVPN: launching exe=%s config=%s", qPrintable(m_openVpnExe),
        m_configFile ? qPrintable(m_configFile->fileName()) : "(no file)");
    m_ovpn->start();
    if (!m_ovpn->waitForStarted(8000)) {
        qCWarning(lcVpn, "OpenVPN: waitForStarted failed: %s", qPrintable(m_ovpn->errorString()));
        stopBridges();
        m_connecting = false;
        emit openVpnCapabilitySetupRecommended(
            Datagate::tr("OpenVPN did not start within 8s (%1). Grant CAP_NET_ADMIN to the openvpn binary if TUN is blocked.")
                .arg(m_ovpn->errorString()));
        return;
    }

    m_connecting = false;
    qCInfo(lcVpn, "OpenVPN: process started, pid=%lld", static_cast<long long>(m_ovpn->processId()));
    emit statusMessage(Datagate::tr("OpenVPN started."));
    if (ignoreRg) {
        emit statusMessage(
            Datagate::tr("IgnoreRedirectGateway is on: the server default-route push is ignored, so your public IP "
               "usually stays the same and only VPN-specific routes use the tunnel. Set "
               "OpenVpn.IgnoreRedirectGateway to false in appsettings.json if you need a full tunnel (all "
               "traffic and public IP via VPN)."));
    }
    emit vpnUp();
}

void VpnSession::disconnectVpn(const char* reason)
{
    disconnectVpnInternal(false, reason);
}

void VpnSession::disconnectVpnInternal(bool force, const char* reason)
{
    if (m_disconnecting && !force) {
        return;
    }
    if (m_disconnecting && force) {
        QElapsedTimer t;
        t.start();
        while (m_disconnecting && t.elapsed() < 10000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
    }

    m_disconnecting = true;
    qCInfo(lcVpn, "disconnectVpn: %s", reason ? reason : "Disconnect button or unspecified");
    m_connecting = false;
    m_userStopVpn = true;

    abortPendingReplies();

#if defined(DATAGATE_EMBEDDED_OPENVPN3)
    if (Datagate::DatagateOvpn3Client* p = m_ovpn3Active.load()) {
        p->requestStop();
    }
    if (m_ovpn3Thread && m_ovpn3Thread->joinable()) {
        m_ovpn3Thread->join();
    }
    m_ovpn3Thread.reset();
#endif

    if (m_ovpn->state() != QProcess::NotRunning) {
        m_ovpn->terminate();
        if (!m_ovpn->waitForFinished(4000)) {
            m_ovpn->kill();
            m_ovpn->waitForFinished(2000);
        }
    }

    stopBridges();

    if (m_configFile) {
        m_configFile->close();
        m_configFile->deleteLater();
        m_configFile = nullptr;
    }

    m_disconnecting = false;
    emit vpnDown();
}

void VpnSession::onOpenVpnFinished(int exitCode, QProcess::ExitStatus st)
{
    Q_UNUSED(st);
    stopBridges();

    QString log;
    if (m_ovpn) {
        // Buffer from readyRead + anything left in the pipe
        m_ovpnLogBuffer.append(m_ovpn->readAllStandardOutput());
        log = QString::fromUtf8(m_ovpnLogBuffer).trimmed();
        m_ovpnLogBuffer.clear();
    }
    if (!log.isEmpty()) {
        qCWarning(lcVpn).nospace() << "OpenVPN exit " << exitCode << " log:\n"
                                   << log;
    } else {
        qCInfo(lcVpn, "OpenVPN: process exited with code %d (no captured output)", exitCode);
    }

    emit statusMessage(Datagate::tr("OpenVPN exited (code %1).").arg(exitCode));

    const bool stoppedByUser = m_userStopVpn;
    m_userStopVpn = false;

    if (exitCode != 0 && !stoppedByUser) {
        QString detail = log;
        if (detail.size() > 3500) {
            detail = QStringLiteral("…\n") + detail.right(3500);
        }
        if (detail.isEmpty()) {
            emit openVpnCapabilitySetupRecommended(
                Datagate::tr("OpenVPN failed (exit %1) with no captured output. On Linux, TUN usually needs "
                   "CAP_NET_ADMIN on the openvpn binary (Settings → Grant TUN capability, or "
                   "sudo setcap cap_net_admin+ep /path/to/openvpn).")
                    .arg(exitCode));
        } else if (logLooksLikeTunPermissionDenied(detail)) {
            emit openVpnCapabilitySetupRecommended(
                Datagate::tr("TUN creation was denied (CAP_NET_ADMIN required on the openvpn process, not the GUI).\n\n"
                   "OpenVPN failed (exit %1):\n%2")
                    .arg(exitCode)
                    .arg(detail));
        } else {
            emit errorMessage(Datagate::tr("OpenVPN failed (exit %1):\n%2").arg(exitCode).arg(detail));
        }
    }

    // User-initiated disconnect already emitted vpnDown from disconnectVpnInternal.
    if (!stoppedByUser) {
        emit vpnDown();
    }
}

void VpnSession::onOpenVpnError(QProcess::ProcessError e)
{
    emit errorMessage(Datagate::tr("OpenVPN process error: %1").arg(static_cast<int>(e)));
}

#if defined(DATAGATE_EMBEDDED_OPENVPN3)

void VpnSession::startEmbeddedOpenVpn3(const QString& patched, bool useUdp, quint16 bridgePort)
{
    emit statusMessage(useUdp ? Datagate::tr("UDP↔WSS bridge on port %1…").arg(bridgePort)
                              : Datagate::tr("TCP↔WSS bridge on port %1…").arg(bridgePort));
    emit statusMessage(Datagate::tr("Starting OpenVPN 3…"));
    qCInfo(lcVpn, "OpenVPN 3: embedded core (IV_VER set at build time; guiVersion=datagate_linux/<app>)");

    const QString appVer = QCoreApplication::applicationVersion().trimmed();
    const QString guiVer = appVer.isEmpty() ? QStringLiteral("datagate_linux 0")
                                            : QStringLiteral("datagate_linux %1").arg(appVer);

    const std::string profile = patched.toUtf8().toStdString();
    const std::string gui = guiVer.toUtf8().toStdString();

    m_ovpn3Thread = std::make_unique<std::thread>([this, profile, gui]() {
        Datagate::DatagateOvpn3Client client;
        m_ovpn3Active.store(&client);
        client.setCallbacks(
            [this](const std::string& n, const std::string& i) {
                const QString qn = QString::fromStdString(n);
                const QString qi = QString::fromStdString(i);
                QMetaObject::invokeMethod(
                    this,
                    [this, qn, qi]() { deliverOvpn3Event(qn, qi); },
                    Qt::QueuedConnection);
            },
            [](const std::string& line) {
                qCDebug(lcVpn).nospace() << "ovpn3: " << QString::fromStdString(line);
            });
        const std::string err = client.connectBlocking(profile, gui);
        m_ovpn3Active.store(nullptr);
        const QString qerr = err.empty() ? QString() : QString::fromUtf8(QByteArray::fromStdString(err));
        QMetaObject::invokeMethod(
            this,
            [this, qerr]() { onOvpn3ThreadFinished(qerr); },
            Qt::QueuedConnection);
    });
}

void VpnSession::deliverOvpn3Event(const QString& name, const QString& info)
{
    if (info.isEmpty()) {
        emit statusMessage(name);
    } else {
        emit statusMessage(name + QLatin1String(": ") + info);
    }
    if (name.compare(QLatin1String("CONNECTED"), Qt::CaseInsensitive) == 0) {
        m_connecting = false;
        emit vpnUp();
    }
}

void VpnSession::onOvpn3ThreadFinished(const QString& errText)
{
    if (m_ovpn3Thread && m_ovpn3Thread->joinable()) {
        m_ovpn3Thread->join();
    }
    m_ovpn3Thread.reset();

    stopBridges();
    m_connecting = false;

    if (!errText.isEmpty() && !m_userStopVpn) {
        emit errorMessage(Datagate::tr("OpenVPN 3: %1").arg(errText));
    }

    const bool stoppedByUser = m_userStopVpn;
    m_userStopVpn = false;

    if (!stoppedByUser) {
        emit vpnDown();
    }
}

#endif // DATAGATE_EMBEDDED_OPENVPN3
