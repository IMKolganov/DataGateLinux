#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>

#include "DatagateUtils.h"

class QNetworkAccessManager;
class QTemporaryFile;
class UdpWssBridge;
class WssTcpBridge;

class VpnSession final : public QObject {
    Q_OBJECT
public:
    explicit VpnSession(QObject* parent = nullptr);
    ~VpnSession() override;

    void connectVpn(const QString& backendBaseUrl, const QString& bearerAccessToken,
        const QString& openVpnExecutable);
    /// reason is logged (e.g. why OpenVPN received SIGTERM). Pass nullptr for default.
    void disconnectVpn(const char* reason = nullptr);

signals:
    void statusMessage(const QString& text);
    void errorMessage(const QString& text);
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

    QNetworkAccessManager* m_nam = nullptr;
    WssTcpBridge* m_tcpBridge = nullptr;
    UdpWssBridge* m_udpBridge = nullptr;
    QProcess* m_ovpn = nullptr;
    QTemporaryFile* m_configFile = nullptr;

    QString m_baseUrl;
    QString m_token;
    QString m_openVpnExe;
    BestServer m_server;
    QString m_commonName;

    bool m_connecting = false;
    bool m_userStopVpn = false;

    /// Accumulated OpenVPN stdout/stderr (MergedChannels); also streamed live via qCDebug(lcVpn).
    QByteArray m_ovpnLogBuffer;
};
