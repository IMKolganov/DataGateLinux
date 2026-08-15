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

/// Assign /30 on TUN, pin uplink via current default gateway, replace default via TUN.
ApplyResult applyRoutes(const QString& uplinkHost);
void tearDownRoutes(const QString& uplinkHost);

} // namespace XrayTunLinux
