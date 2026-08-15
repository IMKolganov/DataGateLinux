#include "XrayTunLinux.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QThread>

namespace XrayTunLinux {
namespace {

QString runCapture(const QString& program, const QStringList& args, int* exitCodeOut = nullptr)
{
    QProcess p;
    p.start(program, args);
    p.waitForFinished(15000);
    if (exitCodeOut) {
        *exitCodeOut = p.exitCode();
    }
    return QString::fromUtf8(p.readAllStandardOutput() + p.readAllStandardError()).trimmed();
}

/// Prefer a single pkexec bash -c when plain `ip` lacks CAP_NET_ADMIN.
bool runIpScript(const QString& scriptBody, QString* errOut = nullptr)
{
    int code = -1;
    QProcess p;
    p.start(QStringLiteral("bash"), {QStringLiteral("-c"), scriptBody});
    p.waitForFinished(20000);
    code = p.exitCode();
    QString out = QString::fromUtf8(p.readAllStandardOutput() + p.readAllStandardError()).trimmed();
    if (code == 0) {
        return true;
    }
    QProcess pk;
    pk.start(QStringLiteral("pkexec"), {QStringLiteral("bash"), QStringLiteral("-c"), scriptBody});
    pk.waitForFinished(120000);
    out = QString::fromUtf8(pk.readAllStandardOutput() + pk.readAllStandardError()).trimmed();
    if (pk.exitCode() == 0) {
        return true;
    }
    if (errOut) {
        *errOut = out.isEmpty() ? QStringLiteral("exit %1").arg(pk.exitCode()) : out;
    }
    return false;
}

struct DefaultRoute {
    QString gateway;
    QString device;
};

DefaultRoute parseDefaultRoute()
{
    const QString out = runCapture(QStringLiteral("ip"), {QStringLiteral("-4"), QStringLiteral("route"), QStringLiteral("show"), QStringLiteral("default")});
    // default via 192.168.0.1 dev enp5s0 proto dhcp ...
    static const QRegularExpression re(
        QStringLiteral(R"(default\s+via\s+(\S+)\s+dev\s+(\S+))"));
    const QRegularExpressionMatch m = re.match(out);
    DefaultRoute d;
    if (m.hasMatch()) {
        d.gateway = m.captured(1);
        d.device = m.captured(2);
    }
    return d;
}

QString resolveIpv4(const QString& host)
{
    const QHostAddress asIp(host);
    if (!asIp.isNull() && asIp.protocol() == QAbstractSocket::IPv4Protocol) {
        return asIp.toString();
    }
    const QHostInfo hi = QHostInfo::fromName(host);
    for (const QHostAddress& a : hi.addresses()) {
        if (a.protocol() == QAbstractSocket::IPv4Protocol) {
            return a.toString();
        }
    }
    return {};
}

} // namespace

QString uplinkHostFromClientJson(const QString& configJson)
{
    const QJsonObject root = QJsonDocument::fromJson(configJson.toUtf8()).object();
    const QJsonArray outs = root.value(QStringLiteral("outbounds")).toArray();
    for (const QJsonValue& v : outs) {
        const QJsonObject o = v.toObject();
        const QString proto = o.value(QStringLiteral("protocol")).toString();
        if (proto.compare(QStringLiteral("vless"), Qt::CaseInsensitive) != 0
            && proto.compare(QStringLiteral("vmess"), Qt::CaseInsensitive) != 0
            && proto.compare(QStringLiteral("trojan"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        const QJsonArray vnext = o.value(QStringLiteral("settings")).toObject().value(QStringLiteral("vnext")).toArray();
        if (!vnext.isEmpty()) {
            const QString addr = vnext.at(0).toObject().value(QStringLiteral("address")).toString().trimmed();
            if (!addr.isEmpty()) {
                return addr;
            }
        }
        const QJsonArray servers = o.value(QStringLiteral("settings")).toObject().value(QStringLiteral("servers")).toArray();
        if (!servers.isEmpty()) {
            const QString addr = servers.at(0).toObject().value(QStringLiteral("address")).toString().trimmed();
            if (!addr.isEmpty()) {
                return addr;
            }
        }
    }
    return {};
}

ApplyResult applyRoutes(const QString& uplinkHost)
{
    ApplyResult r;
    if (uplinkHost.trimmed().isEmpty()) {
        r.message = QStringLiteral("Missing Xray uplink host for TUN bypass route.");
        return r;
    }
    const DefaultRoute def = parseDefaultRoute();
    if (def.gateway.isEmpty() || def.device.isEmpty()) {
        r.message = QStringLiteral("Could not detect the current default gateway (ip route).");
        return r;
    }
    const QString uplinkIp = resolveIpv4(uplinkHost);
    if (uplinkIp.isEmpty()) {
        r.message = QStringLiteral("Could not resolve Xray uplink host: %1").arg(uplinkHost);
        return r;
    }

    QString err;
    for (int i = 0; i < 20; ++i) {
        int code = 1;
        runCapture(QStringLiteral("ip"),
            {QStringLiteral("link"), QStringLiteral("show"), QStringLiteral("dev"), QString::fromUtf8(kIfaceName)},
            &code);
        if (code == 0) {
            break;
        }
        QThread::msleep(100);
    }

    const QString iface = QString::fromUtf8(kIfaceName);
    const QString script = QStringLiteral(
        "set -e\n"
        "ip link set \"%1\" up\n"
        "ip addr flush dev \"%1\" || true\n"
        "ip addr add 10.255.254.2/30 peer 10.255.254.1 dev \"%1\"\n"
        "ip route replace \"%2\"/32 via \"%3\" dev \"%4\"\n"
        "ip route replace default via 10.255.254.1 dev \"%1\" metric 50\n")
                               .arg(iface, uplinkIp, def.gateway, def.device);

    if (!runIpScript(script, &err)) {
        r.message = QStringLiteral("TUN route setup failed: %1").arg(err);
        return r;
    }

    r.ok = true;
    r.message = QStringLiteral("TUN %1 up; default via tunnel; uplink %2 via %3 (%4)")
                    .arg(iface, uplinkIp, def.gateway, def.device);
    return r;
}

void tearDownRoutes(const QString& uplinkHost)
{
    const QString iface = QString::fromUtf8(kIfaceName);
    const QString uplinkIp = resolveIpv4(uplinkHost);
    QString script = QStringLiteral(
        "ip route del default via 10.255.254.1 dev \"%1\" 2>/dev/null || true\n"
        "ip addr flush dev \"%1\" 2>/dev/null || true\n")
                         .arg(iface);
    if (!uplinkIp.isEmpty()) {
        script += QStringLiteral("ip route del \"%1\"/32 2>/dev/null || true\n").arg(uplinkIp);
    }
    runIpScript(script, nullptr);
}

} // namespace XrayTunLinux
