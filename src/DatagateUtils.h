#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <utility>
#include <optional>

class QNetworkReply;

struct BestServer {
    int serverId = 0;
    QString name;
    QString apiUrl;
    int countConnectedClients = 0;
    bool isOnline = false;
};

struct ParsedRemote {
    QString host;
    quint16 port = 0;
};

namespace DatagateUtils {

/// Same default as DataGateWin engine / StartSessionPayloadBuilder.
constexpr quint16 kDefaultBridgePort = 18080;

/// Linux: best-effort kill processes still bound to the local bridge port (orphan openvpn after crash/kill -9).
void releaseStaleBridgePort(quint16 port);

/// 22-char URL-safe token (DataGateWin InstallationIdService).
QString getOrCreateInstallationId();

QString installationIdFromSettings();
void ensureInstallationId();

QString externalIdFromJwt(const QString& accessToken);

/// First `proto` line value (e.g. udp, tcp-client, tcp).
QString tryGetProtoFromOvpn(const QString& ovpnUtf8);

/// First `remote` line: host and port (before local patch).
ParsedRemote parseFirstRemoteFromOvpn(const QString& ovpnUtf8);

/// DataGateWin OvpnConfigProcessor::PatchOvpnRemoteToLocal — prepend local remote, drop other `remote` lines.
/// If ignoreRedirectGateway, append pull-filter so server PUSH redirect-gateway is ignored (Linux/desktop split-tunnel).
/// routeBypassLines: optional `route x.x.x.x 255.255.255.255 net_gateway` lines (WSS/API hosts) so transport survives redirect-gateway.
QString patchOvpnRemoteToLocal(const QString& ovpnUtf8, const QString& localHost, quint16 localPort,
    bool ignoreRedirectGateway = false, const QString& routeBypassLines = QString());

/// Resolve hostnames to IPv4 and build OpenVPN `route … net_gateway` lines (bypass full tunnel for WSS/API peers).
QString openVpnRouteBypassNetGatewayLines(const QStringList& hostnames);

QString httpsToWssProxyBase(const QString& baseApiUrl);

/// wss://…/api/proxy[?mode=udp] — matches engine BridgeManager for UDP.
QUrl wssProxyUrl(const QString& baseApiUrl, bool udpMode);

std::optional<BestServer> pickBestServerWinStyle(const QByteArray& jsonBody);

/// WSS-enabled servers only: id and display name (for manual server combo / Statistics).
QVector<QPair<int, QString>> listWssServersFromStatusJson(const QByteArray& jsonBody);

/// Pick one server by id from get-all-with-status JSON; requires isEnableWss and valid apiUrl.
std::optional<BestServer> pickServerByIdFromStatusJson(const QByteArray& jsonBody, int serverId);

void saveLastSelectedServerId(int id);
int loadLastSelectedServerId();

/// Resolve OpenVPN executable path (absolute path or lookup in PATH), same rules as VpnSession launch.
QString resolvedOpenVpnExecutable(const QString& openVpnCmdOrPath);

/// Human-readable client id for OpenVPN 2 peer-info: "<openvpn semver>_datagate_linux_<app semver>" (e.g. 2.6.12_datagate_linux_1.0.0).
QString datagateLinuxPeerVersionLabel(const QString& resolvedOpenVpnExecutable);

/// User-visible message when the API host is unreachable or returns 5xx. Empty if the caller should show a detailed/server message.
QString userMessageWhenApiUnavailable(QNetworkReply* rep);

#if defined(__linux__)
/// Linux: `getcap` output for this path (best-effort).
QString linuxFileGetcapLine(const QString& canonicalExecutablePath);
#endif

} // namespace DatagateUtils
