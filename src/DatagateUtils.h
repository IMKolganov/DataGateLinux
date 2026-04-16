#pragma once

#include <QtGlobal>

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <utility>
#include <optional>

class QNetworkReply;
class QNetworkAccessManager;

struct BestServer {
    int serverId = 0;
    QString name;
    QString apiUrl;
    int countConnectedClients = 0;
    bool isOnline = false;
    /// From v2 API; true if missing (v1-compatible).
    bool isAccessibleForUserQuotaPlan = true;
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

/// ASP.NET nameidentifier — same as DataGateAndroid [JwtClaimsReader] (nameid / SOAP claim).
QString userIdFromJwt(const QString& accessToken);

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

/// Quota plan summary — plans + assignments; usage from [GET api/open-vpn-clients/overview/summary] (dashboard parity).
struct UserVpnAccessInfo {
    QString planName;
    QString effectiveFrom;
    QString assignmentNote;
    /// Monthly quota from matched plan (display), MB; -1 if unknown
    double monthlyQuotaMb = -1;
    /// -1 if unknown (usage not always exposed by these endpoints)
    double trafficUsedMb = -1;
    QDateTime validUntil;
    bool canUseVpn = true;
    QString restrictionMessage;
    QString quotaApiError;
    /// Limit for the current period (month or day), bytes; <=0 means no cap / unknown
    qint64 quotaLimitBytes = 0;
    /// Same semantics as OpenVpnGateMonitorFrontend UserTrafficQuotaProgress (monthly vs daily cap).
    bool quotaPeriodIsMonthly = true;
    /// Totals for ExternalId in [From, To]; -1 if not fetched
    qint64 trafficUsedBytesForPeriod = -1;
    /// JWT had no sub/externalId — overview summary cannot filter by client
    bool trafficUsageNeedsExternalId = false;
};

QString formatDataSizeBytes(qint64 bytes);
/// ISO8601 date/time string → human text using QLocale::system() / setDefault (language-aware).
QString formatIsoDateTimeForLocale(const QString& iso);

/// POST api/quota-plans/get-all + GET api/user-quota-plans/get-by-user-id/{userId} (Android parity).
bool fetchUserVpnAccessInfoSync(QNetworkAccessManager* nam, const QString& apiBaseUrl, const QString& bearerToken,
    UserVpnAccessInfo* out, QString* errorOut);

#if defined(__linux__)
/// Linux: `getcap` output for this path (best-effort).
QString linuxFileGetcapLine(const QString& canonicalExecutablePath);

/// `/proc/self/status` TracerPid, or -1 if unreadable. Non-zero ⇒ ptrace (debugger attached).
int linuxProcSelfTracerPid();

/// Multi-line diagnosis: /proc/self/exe vs Qt path, CapEff/CapPrm, TracerPid, getcap on running binary.
QString linuxEmbeddedVpnTunDiagnosis(const QString& qAppExecutablePath);

/// `DataGateOvpn3Helper` next to the main binary when the split-process helper is built (Linux embedded).
QString linuxEmbeddedOvpn3HelperPath();

/// setcap target for embedded TUN: helper when present, otherwise the GUI binary.
QString linuxEmbeddedTunCapabilityTargetExecutablePath();

/// If CAP_NET_ADMIN is permitted but not effective, promote to effective (needs DATAGATE_HAVE_LIBCAP + libcap).
bool linuxTryRaiseEffectiveCapNetAdmin();

/// Lower 64 capability bits from /proc/self/status (CapPrm / CapEff).
quint64 linuxProcSelfCapPermittedU64();
quint64 linuxProcSelfCapEffectiveU64();
#endif

} // namespace DatagateUtils
