#include "AppConfig.h"
#include "AppLogging.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

QString AppConfig::s_apiBaseUrl;
QString AppConfig::s_googleClientId;
int AppConfig::s_googleRedirectPort = 51723;
bool AppConfig::s_openVpnIgnoreRedirectGateway = false;
bool AppConfig::s_udpBridgeFramed = true;
bool AppConfig::s_openVpnUseSystemBinary = false;
QString AppConfig::s_updateGithubOwner = QStringLiteral("IMKolganov");
QString AppConfig::s_updateGithubRepo = QStringLiteral("DataGateLinux");
bool AppConfig::s_updateCheckOnStartup = true;

static bool readJsonFile(const QString& path, QJsonObject* out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray raw = f.readAll();
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    *out = doc.object();
    return true;
}

bool AppConfig::load()
{
    s_apiBaseUrl.clear();
    s_googleClientId.clear();
    s_googleRedirectPort = 51723;
    s_openVpnIgnoreRedirectGateway = false;
    s_udpBridgeFramed = true;
#if defined(DATAGATE_EMBEDDED_OPENVPN3)
    s_openVpnUseSystemBinary = false;
#else
    s_openVpnUseSystemBinary = true;
#endif
    s_updateGithubOwner = QStringLiteral("IMKolganov");
    s_updateGithubRepo = QStringLiteral("DataGateLinux");
    s_updateCheckOnStartup = true;

    const QString exeDir = QCoreApplication::applicationDirPath();
    // Prefer cwd first (IDEs often set working dir to project root), then the executable directory.
    const QStringList candidates = {
        QDir(QDir::currentPath()).filePath(QStringLiteral("appsettings.json")),
        QDir(exeDir).filePath(QStringLiteral("appsettings.json")),
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
            + QStringLiteral("/appsettings.json"),
    };

    QJsonObject root;
    QString loadedFrom;
    for (const QString& p : candidates) {
        if (readJsonFile(p, &root)) {
            loadedFrom = p;
            break;
        }
    }
    if (!loadedFrom.isEmpty()) {
        qCInfo(lcUi, "AppConfig: loaded %s", qPrintable(loadedFrom));
    }

    if (root.isEmpty()) {
        return false;
    }

    const QJsonObject api = root.value(QStringLiteral("Api")).toObject();
    s_apiBaseUrl = api.value(QStringLiteral("BaseUrl")).toString().trimmed();

    const QJsonObject ga = root.value(QStringLiteral("GoogleAuth")).toObject();
    s_googleClientId = ga.value(QStringLiteral("ClientId")).toString().trimmed();
    if (s_googleClientId.isEmpty()) {
        s_googleClientId = ga.value(QStringLiteral("DesktopClientId")).toString().trimmed();
    }
    if (ga.contains(QStringLiteral("RedirectPort"))) {
        s_googleRedirectPort = ga.value(QStringLiteral("RedirectPort")).toInt(51723);
    }

    const QJsonObject ovpn = root.value(QStringLiteral("OpenVpn")).toObject();
    s_openVpnIgnoreRedirectGateway = ovpn.value(QStringLiteral("IgnoreRedirectGateway")).toBool(false);
    qCInfo(lcUi, "AppConfig: OpenVpn.IgnoreRedirectGateway=%s",
        s_openVpnIgnoreRedirectGateway ? "true" : "false");
    // Default true (length-prefixed); set false if proxy sends one raw OpenVPN UDP datagram per WS binary frame.
    s_udpBridgeFramed = ovpn.value(QStringLiteral("UdpBridgeFramed")).toBool(true);
    qCInfo(lcUi, "AppConfig: OpenVpn.UdpBridgeFramed=%s", s_udpBridgeFramed ? "true" : "false");

    const QJsonObject update = root.value(QStringLiteral("Update")).toObject();
    if (!update.isEmpty()) {
        if (update.contains(QStringLiteral("GitHubOwner"))) {
            s_updateGithubOwner = update.value(QStringLiteral("GitHubOwner")).toString().trimmed();
        }
        if (update.contains(QStringLiteral("GitHubRepo"))) {
            s_updateGithubRepo = update.value(QStringLiteral("GitHubRepo")).toString().trimmed();
        }
        if (update.contains(QStringLiteral("CheckOnStartup"))) {
            s_updateCheckOnStartup = update.value(QStringLiteral("CheckOnStartup")).toBool(true);
        }
    }
    qCInfo(lcUi, "AppConfig: Update GitHub=%s/%s CheckOnStartup=%s",
        qPrintable(s_updateGithubOwner), qPrintable(s_updateGithubRepo),
        s_updateCheckOnStartup ? "true" : "false");

#if defined(DATAGATE_EMBEDDED_OPENVPN3)
    if (ovpn.contains(QStringLiteral("UseSystemBinary"))) {
        s_openVpnUseSystemBinary = ovpn.value(QStringLiteral("UseSystemBinary")).toBool(false);
    }
    qCInfo(lcUi, "AppConfig: OpenVpn.UseSystemBinary=%s", s_openVpnUseSystemBinary ? "true" : "false");
#endif

    return !s_apiBaseUrl.isEmpty() && !s_googleClientId.isEmpty();
}

QString AppConfig::apiBaseUrl()
{
    return s_apiBaseUrl;
}

QString AppConfig::googleClientId()
{
    return s_googleClientId;
}

int AppConfig::googleRedirectPort()
{
    return s_googleRedirectPort;
}

bool AppConfig::openVpnIgnoreRedirectGateway()
{
    return s_openVpnIgnoreRedirectGateway;
}

bool AppConfig::udpBridgeFramed()
{
    return s_udpBridgeFramed;
}

bool AppConfig::openVpnUseSystemBinary()
{
#if !defined(DATAGATE_EMBEDDED_OPENVPN3)
    return true;
#else
    return s_openVpnUseSystemBinary;
#endif
}

QString AppConfig::updateGithubOwner()
{
    return s_updateGithubOwner;
}

QString AppConfig::updateGithubRepo()
{
    return s_updateGithubRepo;
}

bool AppConfig::updateCheckOnStartup()
{
    return s_updateCheckOnStartup;
}
