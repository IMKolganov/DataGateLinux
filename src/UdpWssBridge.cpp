#include "UdpWssBridge.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QTimer>
#include <QUdpSocket>
#include <QWebSocket>

Q_LOGGING_CATEGORY(lcUdpBridge, "datagate.udpbridge")

namespace {

void appendU16Be(QByteArray& out, quint16 v)
{
    out.append(static_cast<char>((v >> 8) & 0xff));
    out.append(static_cast<char>(v & 0xff));
}

bool readU16Be(const QByteArray& buf, int off, int len, quint16& out)
{
    if (off + 2 > len) {
        return false;
    }
    const auto* p = reinterpret_cast<const unsigned char*>(buf.constData());
    out = static_cast<quint16>((static_cast<unsigned>(p[off]) << 8) | static_cast<unsigned>(p[off + 1]));
    return true;
}

} // namespace

UdpWssBridge::UdpWssBridge(QObject* parent)
    : QObject(parent)
    , m_udp(new QUdpSocket(this))
    , m_ws(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_pingTimer(new QTimer(this))
{
    m_pingTimer->setInterval(20000);
    connect(m_pingTimer, &QTimer::timeout, this, [this]() {
        if (m_ws->state() == QAbstractSocket::ConnectedState) {
            m_ws->ping();
        }
    });

    connect(m_udp, &QUdpSocket::readyRead, this, &UdpWssBridge::onUdpReady);
    connect(m_ws, &QWebSocket::connected, this, &UdpWssBridge::onWsConnected);
    connect(m_ws, &QWebSocket::binaryMessageReceived, this, &UdpWssBridge::onWsBinary);
    connect(m_ws, &QWebSocket::disconnected, this, &UdpWssBridge::onWsDisconnected);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(m_ws, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError e) {
        qCWarning(lcUdpBridge) << "WSS error" << e << m_ws->errorString();
    });
#else
    connect(m_ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
        [this](QAbstractSocket::SocketError e) {
            qCWarning(lcUdpBridge) << "WSS error" << e << m_ws->errorString();
        });
#endif
}

UdpWssBridge::~UdpWssBridge()
{
    stop();
}

bool UdpWssBridge::start(quint16 port, const QUrl& wssUrl, const QString& remoteHost, quint16 remotePort,
    bool framed)
{
    stop();
    m_wssUrl = wssUrl;
    m_remoteHost = remoteHost;
    m_remotePort = remotePort;
    m_framed = framed;

    const QHostAddress local(QHostAddress::LocalHost);
    if (!m_udp->bind(local, port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qCWarning(lcUdpBridge) << "UDP bind failed on" << port << m_udp->errorString();
        return false;
    }

    m_ws->open(m_wssUrl);
    qCDebug(lcUdpBridge) << "UDP bridge bind 127.0.0.1:" << port << "wss" << m_wssUrl
                         << "framed=" << m_framed;
    return true;
}

void UdpWssBridge::stop()
{
    m_pingTimer->stop();
    if (m_ws->state() != QAbstractSocket::UnconnectedState) {
        m_ws->close();
    }
    if (m_udp->state() == QUdpSocket::BoundState) {
        m_udp->close();
    }
    m_wsRxBuffer.clear();
    m_pendingUdpToWsFramed.clear();
    m_pendingToUdp.clear();
    m_lastPeerPort = 0;
}

void UdpWssBridge::onWsConnected()
{
    // Avoid carrying a partial length prefix across reconnects (would mis-deliver bytes as UDP).
    m_wsRxBuffer.clear();

    QJsonObject o;
    o.insert(QStringLiteral("type"), QStringLiteral("connect"));
    o.insert(QStringLiteral("proto"), QStringLiteral("udp"));
    if (!m_remoteHost.isEmpty()) {
        o.insert(QStringLiteral("host"), m_remoteHost);
    }
    if (m_remotePort != 0) {
        o.insert(QStringLiteral("port"), static_cast<int>(m_remotePort));
    }
    const QByteArray json = QJsonDocument(o).toJson(QJsonDocument::Compact);
    m_ws->sendTextMessage(QString::fromUtf8(json));
    qCDebug(lcUdpBridge) << "UDP connect-handshake" << json;
    while (!m_pendingUdpToWsFramed.isEmpty()) {
        m_ws->sendBinaryMessage(m_pendingUdpToWsFramed.dequeue());
    }
    m_pingTimer->start();
}

void UdpWssBridge::parseAndDeliverOrQueueWs(const QByteArray& chunk)
{
    auto deliver = [this](const QByteArray& payload) {
        if (payload.isEmpty()) {
            return;
        }
        if (m_lastPeerPort == 0) {
            m_pendingToUdp.enqueue(payload);
        } else {
            m_udp->writeDatagram(payload, m_lastPeerAddr, m_lastPeerPort);
        }
    };

    // Fast path: one WebSocket binary frame == one uint16-BE length + UDP payload (common case).
    if (m_wsRxBuffer.isEmpty() && chunk.size() >= 2) {
        quint16 len = 0;
        if (readU16Be(chunk, 0, chunk.size(), len)) {
            const int need = 2 + static_cast<int>(len);
            if (need == chunk.size()) {
                deliver(chunk.mid(2, static_cast<int>(len)));
                return;
            }
            if (need < chunk.size()) {
                // Multiple length-prefixed records in one WS frame, or a full record plus partial next.
                int o = 0;
                const int total = chunk.size();
                while (o + 2 <= total) {
                    if (!readU16Be(chunk, o, total, len)) {
                        break;
                    }
                    const int rec = 2 + static_cast<int>(len);
                    if (o + rec > total) {
                        m_wsRxBuffer.append(chunk.constData() + o, total - o);
                        return;
                    }
                    deliver(chunk.mid(o + 2, static_cast<int>(len)));
                    o += rec;
                }
                if (o < total) {
                    m_wsRxBuffer.append(chunk.constData() + o, total - o);
                }
                return;
            }
        }
    }

    m_wsRxBuffer.append(chunk);

    int off = 0;
    const int n = m_wsRxBuffer.size();
    const char* p = m_wsRxBuffer.constData();

    while (off < n) {
        quint16 len = 0;
        if (!readU16Be(m_wsRxBuffer, off, n, len)) {
            break;
        }
        if (len > 65507) {
            qCWarning(lcUdpBridge) << "UDP WSS stream: invalid length" << len << "— dropping buffered rx"
                                   << n << "bytes";
            m_wsRxBuffer.clear();
            return;
        }
        const int need = 2 + static_cast<int>(len);
        if (off + need > n) {
            break;
        }
        const QByteArray payload = QByteArray(p + off + 2, static_cast<int>(len));
        off += need;

        deliver(payload);
    }

    if (off > 0) {
        m_wsRxBuffer.remove(0, off);
    }
}

void UdpWssBridge::flushPendingToUdp()
{
    if (m_lastPeerPort == 0) {
        return;
    }
    while (!m_pendingToUdp.isEmpty()) {
        const QByteArray payload = m_pendingToUdp.dequeue();
        m_udp->writeDatagram(payload, m_lastPeerAddr, m_lastPeerPort);
    }
}

void UdpWssBridge::onWsBinary(const QByteArray& data)
{
    if (!m_framed) {
        if (data.isEmpty()) {
            return;
        }
        if (m_lastPeerPort == 0) {
            m_pendingToUdp.enqueue(data);
        } else {
            m_udp->writeDatagram(data, m_lastPeerAddr, m_lastPeerPort);
        }
        return;
    }
    parseAndDeliverOrQueueWs(data);
}

void UdpWssBridge::appendUdpToWs(const QByteArray& datagram)
{
    if (datagram.size() > 65535) {
        return;
    }
    QByteArray out;
    if (m_framed) {
        out.reserve(2 + datagram.size());
        appendU16Be(out, static_cast<quint16>(datagram.size()));
        out.append(datagram);
    } else {
        out = datagram;
    }
    if (m_ws->state() == QAbstractSocket::ConnectedState) {
        m_ws->sendBinaryMessage(out);
    } else {
        m_pendingUdpToWsFramed.enqueue(out);
    }
}

void UdpWssBridge::onUdpReady()
{
    while (m_udp->hasPendingDatagrams()) {
        QNetworkDatagram dg = m_udp->receiveDatagram();
        if (!dg.isValid()) {
            continue;
        }
        const QByteArray d = dg.data();
        if (d.isEmpty()) {
            continue;
        }
        m_lastPeerAddr = dg.senderAddress();
        m_lastPeerPort = dg.senderPort();
        flushPendingToUdp();
        appendUdpToWs(d);
    }
}

void UdpWssBridge::onWsDisconnected()
{
    qCDebug(lcUdpBridge) << "WSS disconnected";
}
