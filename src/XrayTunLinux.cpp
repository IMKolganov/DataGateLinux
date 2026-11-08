#include "XrayTunLinux.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>

#include <limits>

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

bool waitProcessKeepUiAlive(QProcess& p, int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    while (p.state() != QProcess::NotRunning && t.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        p.waitForFinished(50);
    }
    return p.state() == QProcess::NotRunning;
}

/// Connect path: may wait on pkexec (user expects a prompt once).
bool runIpScript(const QString& scriptBody, QString* errOut = nullptr, QString* stdoutOut = nullptr)
{
    QProcess p;
    p.start(QStringLiteral("bash"), {QStringLiteral("-c"), scriptBody});
    waitProcessKeepUiAlive(p, 45000);
    if (p.state() != QProcess::NotRunning) {
        p.kill();
        p.waitForFinished(2000);
        if (errOut) {
            *errOut = QStringLiteral("Route setup timed out.");
        }
        return false;
    }
    QString out = QString::fromUtf8(p.readAllStandardOutput() + p.readAllStandardError()).trimmed();
    if (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0) {
        if (stdoutOut) {
            *stdoutOut = out;
        }
        return true;
    }
    QProcess pk;
    pk.start(QStringLiteral("pkexec"), {QStringLiteral("bash"), QStringLiteral("-c"), scriptBody});
    // Cap wait — hanging polkit used to freeze Disconnect for minutes.
    const bool finished = waitProcessKeepUiAlive(pk, 45000);
    out = QString::fromUtf8(pk.readAllStandardOutput() + pk.readAllStandardError()).trimmed();
    if (pk.state() != QProcess::NotRunning) {
        pk.kill();
        pk.waitForFinished(2000);
    }
    if (finished && pk.exitStatus() == QProcess::NormalExit && pk.exitCode() == 0) {
        if (stdoutOut) {
            *stdoutOut = out;
        }
        return true;
    }
    if (errOut) {
        *errOut = out.isEmpty() ? QStringLiteral("exit %1").arg(pk.exitCode()) : out;
    }
    return false;
}

/// Disconnect path: never block the UI on polkit. Best-effort plain `ip`, then detached pkexec.
void runIpScriptDisconnectBestEffort(const QString& scriptBody)
{
    QProcess p;
    p.start(QStringLiteral("bash"), {QStringLiteral("-c"), scriptBody});
    waitProcessKeepUiAlive(p, 2500);
    if (p.state() != QProcess::NotRunning) {
        p.kill();
        p.waitForFinished(500);
    }
    if (p.exitCode() == 0) {
        return;
    }
    // Fire-and-forget — do not wait for password dialog on Disconnect.
    QProcess::startDetached(QStringLiteral("pkexec"),
        {QStringLiteral("bash"), QStringLiteral("-c"), scriptBody});
}

struct DefaultRoute {
    QString gateway;
    QString device;
};

DefaultRoute parseDefaultRoute()
{
    const QString out = runCapture(QStringLiteral("ip"),
        {QStringLiteral("-4"), QStringLiteral("route"), QStringLiteral("show"), QStringLiteral("default")});
    // Prefer lowest metric among LAN defaults — never use leftover Xray TUN as "uplink gateway"
    // (that caused: uplink via 10.255.254.1 dev datagatex0 → routing loop / dead DNS).
    static const QRegularExpression re(QStringLiteral(R"(default\s+via\s+(\S+)\s+dev\s+(\S+)(?:.*?\bmetric\s+(\d+))?)"));
    DefaultRoute best;
    int bestMetric = std::numeric_limits<int>::max();
    const QString tun = QString::fromUtf8(kIfaceName);
    const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QRegularExpressionMatch m = re.match(line.trimmed());
        if (!m.hasMatch()) {
            continue;
        }
        const QString gateway = m.captured(1);
        const QString device = m.captured(2);
        if (device == tun || gateway.startsWith(QLatin1String("10.255.254."))) {
            continue;
        }
        const int metric = m.capturedLength(3) > 0 ? m.captured(3).toInt() : 0;
        if (metric < bestMetric) {
            bestMetric = metric;
            best.gateway = gateway;
            best.device = device;
        }
    }
    return best;
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

bool hasResolvectl()
{
    return !QStandardPaths::findExecutable(QStringLiteral("resolvectl")).isEmpty();
}

QString shellQuote(const QString& s)
{
    return QLatin1Char('\'') + QString(s).replace(QLatin1Char('\''), QStringLiteral("'\\''")) + QLatin1Char('\'');
}

QString dnsSetupSnippet(const QStringList& dnsServers, bool disableDnsOverTls)
{
    if (dnsServers.isEmpty()) {
        return {};
    }
    const QString iface = QString::fromUtf8(kIfaceName);
    QStringList parts;
    for (const QString& s : dnsServers) {
        const QString t = s.trimmed();
        if (!t.isEmpty()) {
            parts.append(shellQuote(t));
        }
    }
    if (parts.isEmpty()) {
        return {};
    }

    QString snip;
    if (hasResolvectl()) {
        // Like Android VpnService.addDnsServer: classic :53 on the TUN iface for all domains.
        // Probe with TCP (UDP DNS over VLESS is flaky right after bring-up). On failure keep
        // tunnel DNS via public resolvers (Android fallback) — never revert to LAN DNS while
        // default route still points at the TUN (that caused DNS_PROBE_STARTED).
        snip = QStringLiteral(
            "sleep 1\n"
            "resolvectl dns %1 %2\n"
            "resolvectl domain %1 '~.'\n"
            "resolvectl default-route %1 yes 2>/dev/null || true\n")
                   .arg(shellQuote(iface), parts.join(QLatin1Char(' ')));
        if (disableDnsOverTls) {
            snip += QStringLiteral("resolvectl dnsovertls %1 no 2>/dev/null || true\n").arg(shellQuote(iface));
        }
        const QString probe = dnsServers.first().trimmed();
        snip += QStringLiteral(
            "DNS_OK=0\n"
            "if command -v dig >/dev/null 2>&1; then\n"
            "  for i in 1 2 3; do\n"
            "    if dig +tcp +time=2 +tries=1 +short @%1 . NS 2>/dev/null | grep -q .; then DNS_OK=1; break; fi\n"
            "    if dig +time=2 +tries=1 +short @%1 . NS 2>/dev/null | grep -q .; then DNS_OK=1; break; fi\n"
            "    sleep 1\n"
            "  done\n"
            "else\n"
            "  DNS_OK=1\n"
            "fi\n"
            "if [ \"$DNS_OK\" != 1 ]; then\n"
            "  resolvectl dns %2 1.1.1.1 8.8.8.8\n"
            "  resolvectl domain %2 '~.'\n"
            "  echo DATAGATE_DNS_FALLBACK_PUBLIC\n"
            "else\n"
            "  echo DATAGATE_DNS_OK\n"
            "fi\n")
                    .arg(shellQuote(probe), shellQuote(iface));
    } else {
        snip = QStringLiteral(
            "cp -a /etc/resolv.conf /etc/resolv.conf.datagate.bak 2>/dev/null || true\n"
            "{\n"
            "  echo '# Generated by DataGateLinux (Xray TUN)'\n");
        for (const QString& s : dnsServers) {
            const QString t = s.trimmed();
            if (!t.isEmpty()) {
                snip += QStringLiteral("  echo 'nameserver %1'\n").arg(t);
            }
        }
        snip += QStringLiteral("} > /etc/resolv.conf\n");
    }
    return snip;
}

QString dnsTeardownSnippet()
{
    const QString iface = QString::fromUtf8(kIfaceName);
    if (hasResolvectl()) {
        // Interface may already be gone; still revert + flush so Chrome/stub leave tunnel DNS.
        return QStringLiteral(
            "resolvectl revert %1 2>/dev/null || true\n"
            "resolvectl flush-caches 2>/dev/null || true\n"
            "resolvectl reset-server-features 2>/dev/null || true\n")
            .arg(shellQuote(iface));
    }
    return QStringLiteral(
        "if [ -f /etc/resolv.conf.datagate.bak ]; then "
        "cp -a /etc/resolv.conf.datagate.bak /etc/resolv.conf; "
        "rm -f /etc/resolv.conf.datagate.bak; "
        "fi\n");
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
        const QJsonArray vnext
            = o.value(QStringLiteral("settings")).toObject().value(QStringLiteral("vnext")).toArray();
        if (!vnext.isEmpty()) {
            const QString addr = vnext.at(0).toObject().value(QStringLiteral("address")).toString().trimmed();
            if (!addr.isEmpty()) {
                return addr;
            }
        }
        const QJsonArray servers
            = o.value(QStringLiteral("settings")).toObject().value(QStringLiteral("servers")).toArray();
        if (!servers.isEmpty()) {
            const QString addr = servers.at(0).toObject().value(QStringLiteral("address")).toString().trimmed();
            if (!addr.isEmpty()) {
                return addr;
            }
        }
    }
    return {};
}

QString pinProxyOutboundToIpv4(const QString& configJson, QString* resolvedIpv4Out)
{
    QJsonParseError perr{};
    QJsonDocument doc = QJsonDocument::fromJson(configJson.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        return configJson;
    }
    QJsonObject root = doc.object();
    QJsonArray outs = root.value(QStringLiteral("outbounds")).toArray();
    bool changed = false;
    for (int i = 0; i < outs.size(); ++i) {
        QJsonObject o = outs.at(i).toObject();
        const QString proto = o.value(QStringLiteral("protocol")).toString();
        if (proto.compare(QStringLiteral("vless"), Qt::CaseInsensitive) != 0
            && proto.compare(QStringLiteral("vmess"), Qt::CaseInsensitive) != 0
            && proto.compare(QStringLiteral("trojan"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        QJsonObject settings = o.value(QStringLiteral("settings")).toObject();
        QJsonArray vnext = settings.value(QStringLiteral("vnext")).toArray();
        QJsonArray servers = settings.value(QStringLiteral("servers")).toArray();
        auto pinAddr = [&](QJsonObject& node) -> bool {
            const QString host = node.value(QStringLiteral("address")).toString().trimmed();
            if (host.isEmpty()) {
                return false;
            }
            const QString ip = resolveIpv4(host);
            if (ip.isEmpty() || ip == host) {
                if (resolvedIpv4Out && !ip.isEmpty()) {
                    *resolvedIpv4Out = ip;
                }
                return false;
            }
            // Keep TLS/REALITY SNI as the original hostname.
            QJsonObject stream = o.value(QStringLiteral("streamSettings")).toObject();
            const QString security = stream.value(QStringLiteral("security")).toString();
            if (security.compare(QStringLiteral("tls"), Qt::CaseInsensitive) == 0) {
                QJsonObject tls = stream.value(QStringLiteral("tlsSettings")).toObject();
                if (tls.value(QStringLiteral("serverName")).toString().trimmed().isEmpty()) {
                    tls.insert(QStringLiteral("serverName"), host);
                    stream.insert(QStringLiteral("tlsSettings"), tls);
                    o.insert(QStringLiteral("streamSettings"), stream);
                }
            } else if (security.compare(QStringLiteral("reality"), Qt::CaseInsensitive) == 0) {
                QJsonObject reality = stream.value(QStringLiteral("realitySettings")).toObject();
                if (reality.value(QStringLiteral("serverName")).toString().trimmed().isEmpty()) {
                    reality.insert(QStringLiteral("serverName"), host);
                    stream.insert(QStringLiteral("realitySettings"), reality);
                    o.insert(QStringLiteral("streamSettings"), stream);
                }
            }
            node.insert(QStringLiteral("address"), ip);
            if (resolvedIpv4Out) {
                *resolvedIpv4Out = ip;
            }
            return true;
        };
        if (!vnext.isEmpty()) {
            QJsonObject node = vnext.at(0).toObject();
            if (pinAddr(node)) {
                vnext.replace(0, node);
                settings.insert(QStringLiteral("vnext"), vnext);
                o.insert(QStringLiteral("settings"), settings);
                outs.replace(i, o);
                changed = true;
                break;
            }
        } else if (!servers.isEmpty()) {
            QJsonObject node = servers.at(0).toObject();
            if (pinAddr(node)) {
                servers.replace(0, node);
                settings.insert(QStringLiteral("servers"), servers);
                o.insert(QStringLiteral("settings"), settings);
                outs.replace(i, o);
                changed = true;
                break;
            }
        }
        break;
    }
    if (!changed) {
        return configJson;
    }
    root.insert(QStringLiteral("outbounds"), outs);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

ApplyResult installUplinkHostRoute(const QString& uplinkHost)
{
    ApplyResult r;
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
    const QString script = QStringLiteral("ip route replace \"%1\"/32 via \"%2\" dev \"%3\"\n")
                               .arg(uplinkIp, def.gateway, def.device);
    QString err;
    if (!runIpScript(script, &err)) {
        r.message = QStringLiteral("Uplink host-route failed: %1")
                        .arg(err.isEmpty() ? QStringLiteral("unknown") : err);
        return r;
    }
    r.ok = true;
    r.message = QStringLiteral("uplink %1 via %2 (%3)").arg(uplinkIp, def.gateway, def.device);
    return r;
}

ApplyResult applyRoutes(const QString& uplinkHost, const QStringList& dnsServers, bool disableDnsOverTls)
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
    // Single privilege prompt: routes + optional DNS + in-script DNS probe/revert.
    QString script = QStringLiteral(
        "set -e\n"
        "added_low=0\n"
        "added_high=0\n"
        "rollback() {\n"
        "  status=$?\n"
        "  if [ \"$status\" != 0 ]; then\n"
        "    [ \"$added_low\" != 1 ] || ip -4 route del 0.0.0.0/1 via 10.255.254.1 dev \"%1\" metric 0 || true\n"
        "    [ \"$added_high\" != 1 ] || ip -4 route del 128.0.0.0/1 via 10.255.254.1 dev \"%1\" metric 0 || true\n"
        "  fi\n"
        "  exit \"$status\"\n"
        "}\n"
        "trap rollback EXIT\n"
        "ip link set \"%1\" up\n"
        "ip addr flush dev \"%1\" || true\n"
        "ip addr add 10.255.254.2/30 peer 10.255.254.1 dev \"%1\"\n"
        "ip route replace \"%2\"/32 via \"%3\" dev \"%4\"\n"
        // More-specific /1 routes take precedence even when the LAN default has metric 0.
        // Use add: a conflicting VPN route must fail without overwriting another VPN's route.
        "ip -4 route add 0.0.0.0/1 via 10.255.254.1 dev \"%1\" metric 0\n"
        "added_low=1\n"
        "ip -4 route add 128.0.0.0/1 via 10.255.254.1 dev \"%1\" metric 0\n"
        "added_high=1\n")
                         .arg(iface, uplinkIp, def.gateway, def.device);
    script += dnsSetupSnippet(dnsServers, disableDnsOverTls);
    script += QStringLiteral("trap - EXIT\n");

    QString err;
    QString out;
    if (!runIpScript(script, &err, &out)) {
        r.message = QStringLiteral("TUN route setup failed: %1")
                        .arg(err.isEmpty() ? QStringLiteral("unknown") : err);
        return r;
    }

    r.ok = true;
    r.message = QStringLiteral("TUN %1 up; default via tunnel; uplink %2 via %3 (%4)")
                    .arg(iface, uplinkIp, def.gateway, def.device);
    if (!dnsServers.isEmpty()) {
        if (out.contains(QLatin1String("DATAGATE_DNS_FALLBACK_PUBLIC"))) {
            r.message += QStringLiteral(
                "; VPN DNS %1 did not answer — using 1.1.1.1/8.8.8.8 via tunnel")
                             .arg(dnsServers.first());
        } else if (out.contains(QLatin1String("DATAGATE_DNS_PROBE_FAILED"))) {
            r.message += QStringLiteral(
                "; DNS %1 did not respond via tunnel — left system DNS unchanged")
                             .arg(dnsServers.first());
        } else if (out.contains(QLatin1String("DATAGATE_DNS_OK"))) {
            r.message += QStringLiteral("; DNS on %1: %2")
                             .arg(iface, dnsServers.join(QLatin1Char(' ')));
        } else if (!hasResolvectl()) {
            r.message += QStringLiteral("; DNS via resolv.conf: %1").arg(dnsServers.join(QLatin1Char(' ')));
        }
    }
    return r;
}

void tearDownRoutes(const QString& uplinkHost)
{
    const QString iface = QString::fromUtf8(kIfaceName);
    const QString uplinkIp = resolveIpv4(uplinkHost);
    // Disconnect must stay snappy: never wait on pkexec (that froze the GUI for minutes).
    QString script = QStringLiteral("set +e\n");
    script += dnsTeardownSnippet();
    script += QStringLiteral(
        "ip -4 route del 0.0.0.0/1 via 10.255.254.1 dev \"%1\" metric 0 2>/dev/null || true\n"
        "ip -4 route del 128.0.0.0/1 via 10.255.254.1 dev \"%1\" metric 0 2>/dev/null || true\n"
        "ip route del default via 10.255.254.1 dev \"%1\" 2>/dev/null || true\n"
        "ip addr flush dev \"%1\" 2>/dev/null || true\n"
        "ip link set \"%1\" down 2>/dev/null || true\n")
                  .arg(iface);
    if (!uplinkIp.isEmpty()) {
        script += QStringLiteral("ip route del \"%1\"/32 2>/dev/null || true\n").arg(uplinkIp);
    }
    // Non-zero only if leftovers remain → triggers detached pkexec, not a UI wait.
    script += QStringLiteral(
        "ip -4 route show | grep -E 'via 10\\.255\\.254\\.1|[[:space:]]dev %1([[:space:]]|$)' "
        ">/dev/null && exit 2\n"
        "ip -o link show dev %1 2>/dev/null | grep -qE '<([^>]*,)?UP(,|>)' && exit 3\n")
                  .arg(iface);
    if (!uplinkIp.isEmpty()) {
        script += QStringLiteral(
            "ip -4 route show \"%1\" 2>/dev/null | grep -q . && exit 4\n").arg(uplinkIp);
    }
    script += QStringLiteral("exit 0\n");
    runIpScriptDisconnectBestEffort(script);
}

} // namespace XrayTunLinux
