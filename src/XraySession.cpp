#include "XraySession.h"
#include "AppConfig.h"
#include "AppLogging.h"
#include "DatagateTr.h"
#include "XrayTunLinux.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QThread>

Q_LOGGING_CATEGORY(lcXray, "datagate.xray")

namespace {

bool logLooksLikeTunDenied(const QString& log)
{
    return log.contains(QLatin1String("operation not permitted"), Qt::CaseInsensitive)
        || log.contains(QLatin1String("permission denied"), Qt::CaseInsensitive)
        || (log.contains(QLatin1String("tun"), Qt::CaseInsensitive)
            && log.contains(QLatin1String("failed"), Qt::CaseInsensitive));
}

} // namespace

XraySession::XraySession(QObject* parent)
    : QObject(parent)
    , m_proc(new QProcess(this))
{
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        &XraySession::onFinished);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_proc, &QProcess::errorOccurred, this, &XraySession::onError);
#else
    connect(m_proc, QOverload<QProcess::ProcessError>::of(&QProcess::error), this, &XraySession::onError);
#endif
    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this]() {
        const QByteArray chunk = m_proc->readAllStandardOutput();
        if (!chunk.isEmpty()) {
            const QString line = QString::fromUtf8(chunk);
            qCInfo(lcXray).nospace() << "xray: " << line.trimmed();
            if (!m_tunCapHintEmitted && AppConfig::xrayUseTun() && logLooksLikeTunDenied(line)) {
                m_tunCapHintEmitted = true;
                emit openVpnCapabilitySetupRecommended(Datagate::tr(
                    "Xray TUN requires CAP_NET_ADMIN on DataGateXray. Use Settings → Grant TUN capability "
                    "(also applies to DataGateXray), then fully restart DataGate."));
            }
        }
    });
}

XraySession::~XraySession()
{
    disconnectInternal(true, "~XraySession");
}

bool XraySession::isRunning() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

QString XraySession::resolveXrayExecutable()
{
    const QString configured = AppConfig::xrayExecutablePath().trimmed();
    if (!configured.isEmpty()) {
        const QFileInfo fi(configured);
        if (fi.exists() && fi.isExecutable()) {
            return fi.canonicalFilePath();
        }
        const QString fromPath = QStandardPaths::findExecutable(configured);
        if (!fromPath.isEmpty()) {
            return fromPath;
        }
    }
    const QString beside = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("DataGateXray"));
    if (QFileInfo::exists(beside) && QFileInfo(beside).isExecutable()) {
        return beside;
    }
    const QString named = QStandardPaths::findExecutable(QStringLiteral("DataGateXray"));
    if (!named.isEmpty()) {
        return named;
    }
    const QString xray = QStandardPaths::findExecutable(QStringLiteral("xray"));
    if (!xray.isEmpty()) {
        return xray;
    }
    return {};
}

void XraySession::connectFromConfigText(const QString& configJson, const QString& displayName,
    const QString& localProfileId)
{
    if (isRunning() || m_connecting) {
        emit errorMessage(Datagate::tr("Disconnect the current Xray session first."));
        return;
    }
    if (configJson.trimmed().isEmpty()) {
        emit errorMessage(Datagate::tr("Xray config is empty."));
        return;
    }

    delete m_configFile;
    m_configFile = new QTemporaryFile(this);
    const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_configFile->setFileTemplate(tmpDir + QStringLiteral("/datagate-xray-XXXXXX.json"));
    if (!m_configFile->open()) {
        emit errorMessage(Datagate::tr("Could not create temporary Xray config file."));
        return;
    }
    m_configFile->write(configJson.toUtf8());
    m_configFile->flush();

    m_displayName = displayName.trimmed().isEmpty() ? Datagate::tr("Xray profile") : displayName.trimmed();
    m_localProfileId = localProfileId.trimmed();
    m_lastConfigJson = configJson;
    startProcess(m_configFile->fileName());
}

void XraySession::connectFromConfigFile(const QString& configPath, const QString& displayName,
    const QString& localProfileId)
{
    if (isRunning() || m_connecting) {
        emit errorMessage(Datagate::tr("Disconnect the current Xray session first."));
        return;
    }
    if (!QFileInfo::exists(configPath)) {
        emit errorMessage(Datagate::tr("Xray config file not found."));
        return;
    }
    m_displayName = displayName.trimmed().isEmpty() ? Datagate::tr("Xray profile") : displayName.trimmed();
    m_localProfileId = localProfileId.trimmed();
    if (m_configFile) {
        m_configFile->deleteLater();
        m_configFile = nullptr;
    }
    QFile f(configPath);
    m_lastConfigJson.clear();
    if (f.open(QIODevice::ReadOnly)) {
        m_lastConfigJson = QString::fromUtf8(f.readAll());
    }
    startProcess(configPath);
}

void XraySession::startProcess(const QString& configPath)
{
    const QString exe = resolveXrayExecutable();
    if (exe.isEmpty()) {
        emit errorMessage(Datagate::tr(
            "DataGateXray was not found next to this program (and no xray on PATH). "
            "Build with Go + third_party/xray-core, or install Xray-core."));
        return;
    }

    m_connecting = true;
    m_userStop = false;
    m_tunCapHintEmitted = false;
    emit statusMessage(Datagate::tr("Starting Xray: %1").arg(m_displayName));

    m_proc->setProgram(exe);
    m_proc->setArguments({QStringLiteral("run"), QStringLiteral("-c"), configPath});
    qCInfo(lcXray, "launching %s run -c %s", qPrintable(exe), qPrintable(configPath));
    m_proc->start();
    if (!m_proc->waitForStarted(8000)) {
        m_connecting = false;
        emit errorMessage(Datagate::tr("Xray did not start within 8s (%1).").arg(m_proc->errorString()));
        return;
    }

    m_connecting = false;
    if (AppConfig::xrayUseTun()) {
        applyTunRoutesIfNeeded(m_lastConfigJson);
        if (m_tunRoutesApplied) {
            emit statusMessage(Datagate::tr("Xray started (TUN %1 + SOCKS :%2).")
                                   .arg(QString::fromUtf8(XrayTunLinux::kIfaceName))
                                   .arg(AppConfig::xraySocksPort()));
        } else {
            emit statusMessage(Datagate::tr(
                "Xray started (SOCKS :%1). TUN interface is up but system routes were not applied — "
                "approve pkexec if prompted, or use SOCKS. Grant TUN capability if needed.")
                                   .arg(AppConfig::xraySocksPort()));
        }
    } else {
        emit statusMessage(Datagate::tr("Xray started (local SOCKS :%1).").arg(AppConfig::xraySocksPort()));
    }
    emit vpnUp();
}

void XraySession::applyTunRoutesIfNeeded(const QString& configJson)
{
    clearTunRoutes();
    if (!AppConfig::xrayUseTun()) {
        return;
    }
    m_tunUplinkHost = XrayTunLinux::uplinkHostFromClientJson(configJson);
    const XrayTunLinux::ApplyResult ar = XrayTunLinux::applyRoutes(m_tunUplinkHost);
    if (!ar.ok) {
        qCWarning(lcXray, "TUN routes: %s", qPrintable(ar.message));
        emit statusMessage(Datagate::tr("TUN routing: %1").arg(ar.message));
        m_tunRoutesApplied = false;
        return;
    }
    qCInfo(lcXray, "TUN routes: %s", qPrintable(ar.message));
    m_tunRoutesApplied = true;
}

void XraySession::clearTunRoutes()
{
    if (m_tunRoutesApplied || !m_tunUplinkHost.isEmpty()) {
        XrayTunLinux::tearDownRoutes(m_tunUplinkHost);
    }
    m_tunRoutesApplied = false;
}

void XraySession::disconnectXray(const char* reason)
{
    disconnectInternal(false, reason);
}

void XraySession::disconnectInternal(bool force, const char* reason)
{
    if (m_disconnecting && !force) {
        return;
    }
    if (m_disconnecting && force) {
        QElapsedTimer t;
        t.start();
        while (m_disconnecting && t.elapsed() < 8000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
    }

    const bool wasActive = isRunning() || m_connecting || m_configFile != nullptr;

    m_disconnecting = true;
    m_connecting = false;
    m_userStop = true;
    qCInfo(lcXray, "disconnectXray: %s", reason ? reason : "unspecified");

    clearTunRoutes();
    m_tunUplinkHost.clear();
    m_lastConfigJson.clear();

    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->terminate();
        if (!m_proc->waitForFinished(4000)) {
            m_proc->kill();
            m_proc->waitForFinished(2000);
        }
    }

    if (m_configFile) {
        m_configFile->close();
        m_configFile->deleteLater();
        m_configFile = nullptr;
    }

    m_localProfileId.clear();
    m_disconnecting = false;
    // Avoid spurious vpnDown when Access/Home calls disconnectAllTunnels while Xray was idle
    // (would flip the Connect button while OpenVPN is still tearing down).
    if (wasActive) {
        emit vpnDown();
    }
}

void XraySession::onFinished(int exitCode, QProcess::ExitStatus st)
{
    Q_UNUSED(st);
    const QString log = QString::fromUtf8(m_proc ? m_proc->readAllStandardOutput() : QByteArray()).trimmed();
    if (!log.isEmpty()) {
        qCWarning(lcXray).nospace() << "xray exit " << exitCode << ":\n" << log;
    }
    emit statusMessage(Datagate::tr("Xray exited (code %1).").arg(exitCode));

    const bool stoppedByUser = m_userStop;
    m_userStop = false;
    m_localProfileId.clear();
    clearTunRoutes();
    m_tunUplinkHost.clear();
    m_lastConfigJson.clear();

    if (exitCode != 0 && !stoppedByUser) {
        QString detail = log;
        if (detail.size() > 2500) {
            detail = QStringLiteral("…\n") + detail.right(2500);
        }
        if (detail.isEmpty()) {
            emit errorMessage(Datagate::tr("Xray failed (exit %1).").arg(exitCode));
        } else {
            emit errorMessage(Datagate::tr("Xray failed (exit %1):\n%2").arg(exitCode).arg(detail));
        }
    }

    if (!stoppedByUser) {
        emit vpnDown();
    }
}

void XraySession::onError(QProcess::ProcessError e)
{
    if (m_userStop) {
        return;
    }
    // FailedToStart is already reported by waitForStarted in startProcess.
    if (e == QProcess::FailedToStart) {
        return;
    }
    emit errorMessage(Datagate::tr("Xray process error: %1").arg(static_cast<int>(e)));
}
