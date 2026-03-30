#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>

#include "DatagateUtils.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryFile;
class UdpWssBridge;
class WssTcpBridge;

class VpnSession final : public QObject {
    Q_OBJECT
public:
    explicit VpnSession(QObject* parent = nullptr);
    ~VpnSession() override;

    void connectVpn(const QString& backendBaseUrl, const QString& bearerAccessToken,
        const QString& openVpnExecutable, bool autoPickServer = true, int manualServerId = 0);

    /// Last server selected after a successful fetch (for UI labels).
    QString activeServerName() const { return m_server.name; }
    /// reason is logged (e.g. why OpenVPN received SIGTERM). Pass nullptr for default.
    void disconnectVpn(const char* reason = nullptr);

signals:
    void statusMessage(const QString& text);
    void errorMessage(const QString& text);
    /// TUN/CAP_NET_ADMIN setup likely needed (empty OpenVPN log, failed start, or TUNSETIFF denied).
    void openVpnCapabilitySetupRecommended(const QString& detail);
    void vpnUp();
    void vpnDown();

private:
    void stepFetchServers();
    void stepTryDownload(bool afterCreate);
    void stepCreateOnServer();
    void stepStartBridgeAndOpenVpn(const QString& configText);
    void stopBridges();

    void onOpenVpnFinished(int exitCode, QProcess::ExitStatus st);
    void onOpenVpnError(QProcess::ProcessError e);

    void disconnectVpnInternal(bool force, const char* reason);
    void abortPendingReplies();

    QNetworkAccessManager* m_nam = nullptr;
    WssTcpBridge* m_tcpBridge = nullptr;
    UdpWssBridge* m_udpBridge = nullptr;
    QProcess* m_ovpn = nullptr;
    QTemporaryFile* m_configFile = nullptr;

    QString m_baseUrl;
    QString m_token;
    QString m_openVpnExe;
    bool m_autoPickServer = true;
    int m_manualServerId = 0;
    BestServer m_server;
    QString m_commonName;

    bool m_connecting = false;
    bool m_disconnecting = false;
    bool m_userStopVpn = false;

    QPointer<QNetworkReply> m_activeReply;

    /// Accumulated OpenVPN stdout/stderr (MergedChannels); also streamed live via qCDebug(lcVpn).
    QByteArray m_ovpnLogBuffer;
};
