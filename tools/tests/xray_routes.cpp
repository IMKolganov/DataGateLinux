#include "XrayTunLinux.h"

#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <stdexcept>

static QString run(const QString& program, const QStringList& args)
{
    QProcess process;
    process.start(program, args);
    if (!process.waitForFinished(15000) || process.exitCode() != 0) {
        throw std::runtime_error((program + QLatin1Char(' ') + args.join(QLatin1Char(' '))
            + QString::fromUtf8(process.readAllStandardError())).toStdString());
    }
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

static QString ip(const QStringList& args) { return run(QStringLiteral("ip"), args); }

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        // All mutations below must stay in a private network namespace.
        const auto parent = qEnvironmentVariable("DATAGATE_TEST_PARENT_NET_NS");
        CHECK(!parent.isEmpty());
        CHECK(run("readlink", {"/proc/self/ns/net"}) != parent);
        ip({"link", "add", "lan0", "type", "dummy"});
        ip({"addr", "add", "192.0.2.2/24", "dev", "lan0"});
        ip({"link", "set", "lan0", "up"});
        ip({"route", "add", "default", "via", "192.0.2.1", "dev", "lan0", "metric", "0"});
        ip({"link", "add", "datagatex0", "type", "dummy"});

        const QString uplink = "203.0.113.10";
        CHECK(XrayTunLinux::installUplinkHostRoute(uplink).ok);
        const auto applied = XrayTunLinux::applyRoutes(uplink);
        if (!applied.ok) qCritical().noquote() << applied.message;
        CHECK(applied.ok);
        CHECK(ip({"route", "get", "198.51.100.10"}).contains("dev datagatex0"));
        CHECK(ip({"route", "get", "8.8.8.8"}).contains("dev datagatex0"));
        CHECK(ip({"route", "get", uplink}).contains("dev lan0"));
        CHECK(ip({"route", "show", "default"}).contains("via 192.0.2.1 dev lan0"));
        XrayTunLinux::tearDownRoutes(uplink);
        CHECK(ip({"route", "get", "198.51.100.10"}).contains("dev lan0"));

        // A foreign VPN's split route must survive repeated DataGate cleanup.
        ip({"route", "add", "0.0.0.0/1", "via", "192.0.2.1", "dev", "lan0"});
        XrayTunLinux::tearDownRoutes(uplink);
        CHECK(ip({"route", "show", "0.0.0.0/1"}).contains("via 192.0.2.1 dev lan0"));
        ip({"route", "del", "0.0.0.0/1", "via", "192.0.2.1", "dev", "lan0"});

        // If the second half conflicts, the first half must roll back without touching the conflict.
        ip({"route", "add", "128.0.0.0/1", "via", "192.0.2.1", "dev", "lan0"});
        CHECK(!XrayTunLinux::applyRoutes(uplink).ok);
        CHECK(ip({"route", "show", "0.0.0.0/1"}).isEmpty());
        CHECK(ip({"route", "show", "128.0.0.0/1"}).contains("via 192.0.2.1 dev lan0"));
        CHECK(ip({"route", "show", "default"}).contains("via 192.0.2.1 dev lan0"));
        qInfo("PASS  xray_routes (metric-zero LAN, endpoint bypass, cleanup, conflict rollback)");
        return 0;
    } catch (const std::exception& error) {
        qCritical("FAIL  xray_routes: %s", error.what());
        return 1;
    }
}
