#pragma once

#include <QString>
#include <QStringList>

/// Linux: Xray TUN only creates the iface — OS must add address + routes (see xray-core proxy/tun/README).
namespace XrayTunLinux {

constexpr const char* kIfaceName = "datagatex0";

struct ApplyResult {
    bool ok = false;
    QString message;
};

/// Extract first VLESS/VMESS outbound host from client JSON (for /32 bypass).
QString uplinkHostFromClientJson(const QString& configJson);

/// Resolve proxy outbound host → IPv4 in-place and keep TLS SNI as the hostname.
/// Avoids DNS chicken-and-egg after resolvectl domain '~.' (Android uses protect(); Linux needs this).
QString pinProxyOutboundToIpv4(const QString& configJson, QString* resolvedIpv4Out = nullptr);

/// Install only uplink/32 via the LAN gateway (call before starting Xray so dials never enter TUN).
ApplyResult installUplinkHostRoute(const QString& uplinkHost);

/// Assign /30 on TUN, pin uplink via current default gateway, replace default via TUN.
/// When dnsServers is non-empty, set classic DNS on the TUN iface (resolvectl preferred).
ApplyResult applyRoutes(const QString& uplinkHost, const QStringList& dnsServers = {},
    bool disableDnsOverTls = false);
void tearDownRoutes(const QString& uplinkHost);

} // namespace XrayTunLinux
