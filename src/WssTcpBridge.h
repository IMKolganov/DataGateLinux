#pragma once

#include <QObject>
#include <QUrl>

class QTcpServer;

class WssTcpBridge final : public QObject {
    Q_OBJECT
public:
    explicit WssTcpBridge(QObject* parent = nullptr);
    ~WssTcpBridge() override;

    bool start(quint16 port, const QUrl& wssUrl);
    void stop();

    /// BridgeConnection* from WssTcpBridge.cpp — single active OpenVPN↔WSS tunnel.
    void _bridgeRegisterSession(void* bridgeConnection);
    void _bridgeUnregisterSession(void* bridgeConnection);

private:
    void onNewConnection();

    QTcpServer* m_server = nullptr;
    QUrl m_wssUrl;
    void* m_bridgeSession = nullptr;
};
