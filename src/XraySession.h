#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>

class QTemporaryFile;

/// Runs DataGateXray / xray as a child process with a client JSON config (no WSS).
class XraySession final : public QObject {
    Q_OBJECT
public:
    explicit XraySession(QObject* parent = nullptr);
    ~XraySession() override;

    void connectFromConfigText(const QString& configJson, const QString& displayName,
        const QString& localProfileId = QString());
    void connectFromConfigFile(const QString& configPath, const QString& displayName,
        const QString& localProfileId = QString());

    void disconnectXray(const char* reason = nullptr);

    QString activeDisplayName() const { return m_displayName; }
    QString activeLocalProfileId() const { return m_localProfileId; }
    bool isRunning() const;

    /// Resolve DataGateXray next to the app, then PATH `xray` / `DataGateXray`.
    static QString resolveXrayExecutable();

signals:
    void statusMessage(const QString& text);
    void errorMessage(const QString& text);
    void openVpnCapabilitySetupRecommended(const QString& detail);
    void vpnUp();
    void vpnDown();

private:
    void startProcess(const QString& configPath);
    void onFinished(int exitCode, QProcess::ExitStatus st);
    void onError(QProcess::ProcessError e);
    void disconnectInternal(bool force, const char* reason);
    void applyTunRoutesIfNeeded(const QString& configJson);
    void clearTunRoutes();

    QProcess* m_proc = nullptr;
    QTemporaryFile* m_configFile = nullptr;
    QString m_displayName;
    QString m_localProfileId;
    QString m_tunUplinkHost;
    QString m_lastConfigJson;
    bool m_connecting = false;
    bool m_disconnecting = false;
    bool m_userStop = false;
    bool m_tunCapHintEmitted = false;
    bool m_tunRoutesApplied = false;
};
