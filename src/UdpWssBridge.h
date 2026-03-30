#pragma once

#include <QHostAddress>
#include <QObject>
#include <QQueue>
#include <QUrl>

class QTimer;
class QUdpSocket;
class QWebSocket;

/**
 * UDP ↔ WSS bridge compatible with DataGateWin engine (UdpWssBridge):
 * - After connect: JSON text {"type":"connect","proto":"udp",...}
 * - Binary: uint16 BE length + UDP payload (both directions).
 */
class UdpWssBridge final : public QObject {
    Q_OBJECT
public:
    explicit UdpWssBridge(QObject* parent = nullptr);
    ~UdpWssBridge() override;

    /// @param framed If true, binary WS messages are uint16-BE length + UDP payload both ways; if false, one WS frame = one raw UDP datagram.
    bool start(quint16 port, const QUrl& wssUrl, const QString& remoteHost, quint16 remotePort, bool framed);
    void stop();

private:
    void onWsConnected();
    void onWsBinary(const QByteArray& data);
    void onWsDisconnected();
    void onUdpReady();

    void appendUdpToWs(const QByteArray& datagram);
    void parseAndDeliverOrQueueWs(const QByteArray& chunk);
    void flushPendingToUdp();

    QUdpSocket* m_udp = nullptr;
    QWebSocket* m_ws = nullptr;
    QUrl m_wssUrl;
    QString m_remoteHost;
    quint16 m_remotePort = 0;
    bool m_framed = true;

    QByteArray m_wsRxBuffer;
    QQueue<QByteArray> m_pendingUdpToWsFramed;
    QQueue<QByteArray> m_pendingToUdp;

    QHostAddress m_lastPeerAddr;
    quint16 m_lastPeerPort = 0;

    QTimer* m_pingTimer = nullptr;
};
