#include "WssTcpBridge.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QWebSocket>

Q_LOGGING_CATEGORY(lcBridge, "datagate.bridge")

namespace {

class BridgeConnection final : public QObject {
public:
    BridgeConnection(QTcpSocket* tcp, const QUrl& wssUrl, WssTcpBridge* owner)
        : QObject(owner)
        , m_owner(owner)
        , m_wssUrl(wssUrl)
        , m_tcp(tcp)
        , m_ws(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
        , m_pingTimer(new QTimer(this))
    {
        m_tcp->setParent(this);
        if (m_owner) {
            m_owner->_bridgeRegisterSession(this);
        }

        m_pingTimer->setInterval(20000);
        connect(m_pingTimer, &QTimer::timeout, this, [this]() {
            if (!m_ws || m_shuttingDown) {
                return;
            }
            if (m_ws->state() == QAbstractSocket::ConnectedState) {
                m_ws->ping();
            }
        });

        connect(m_tcp, &QTcpSocket::readyRead, this, [this]() { onTcpReady(); });
        connect(m_tcp, &QTcpSocket::disconnected, this, [this]() {
            qCDebug(lcBridge) << "OpenVPN closed local TCP to bridge";
            shutdown();
        });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect(m_ws, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError e) {
            qCWarning(lcBridge) << "WebSocket error" << e << m_ws->errorString() << m_wssUrl;
            shutdown();
        });
#else
        connect(m_ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
            [this](QAbstractSocket::SocketError e) {
                qCWarning(lcBridge) << "WebSocket error" << e << m_ws->errorString() << m_wssUrl;
                shutdown();
            });
#endif
        connect(m_ws, &QWebSocket::binaryMessageReceived, this, [this](const QByteArray& m) { onWsBinary(m); });
        connect(m_ws, &QWebSocket::disconnected, this, [this]() {
            if (!m_shuttingDown && m_ws) {
                qCWarning(lcBridge) << "WebSocket closed by proxy or network"
                                    << "closeCode=" << m_ws->closeCode() << "reason=" << m_ws->closeReason()
                                    << "url=" << m_wssUrl;
            }
            shutdown();
        });

        connect(m_ws, &QWebSocket::connected, this, [this]() {
            qCDebug(lcBridge) << "WebSocket connected for bridge" << m_wssUrl;
            m_pingTimer->start();
        });

        m_ws->open(m_wssUrl);
    }

    void requestShutdown() { shutdown(); }

private:
    void onTcpReady()
    {
        if (!m_tcp || !m_ws) {
            return;
        }
        const QByteArray chunk = m_tcp->readAll();
        if (chunk.isEmpty()) {
            return;
        }
        m_ws->sendBinaryMessage(chunk);
    }

    void onWsBinary(const QByteArray& message)
    {
        if (!m_tcp || message.isEmpty()) {
            return;
        }
        m_tcp->write(message);
        m_tcp->flush();
    }

    void shutdown()
    {
        if (m_shuttingDown) {
            return;
        }
        m_shuttingDown = true;
        m_pingTimer->stop();

        if (m_owner) {
            m_owner->_bridgeUnregisterSession(this);
            m_owner = nullptr;
        }

        if (m_tcp) {
            QObject::disconnect(m_tcp, nullptr, this, nullptr);
        }
        if (m_ws) {
            QObject::disconnect(m_ws, nullptr, this, nullptr);
        }

        if (m_ws) {
            m_ws->abort();
            m_ws->deleteLater();
            m_ws = nullptr;
        }
        if (m_tcp) {
            m_tcp->abort();
            m_tcp->deleteLater();
            m_tcp = nullptr;
        }
        deleteLater();
    }

    WssTcpBridge* m_owner = nullptr;
    QUrl m_wssUrl;
    QTcpSocket* m_tcp = nullptr;
    QWebSocket* m_ws = nullptr;
    QTimer* m_pingTimer = nullptr;
    bool m_shuttingDown = false;
};

} // namespace

WssTcpBridge::WssTcpBridge(QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &WssTcpBridge::onNewConnection);
}

WssTcpBridge::~WssTcpBridge()
{
    stop();
}

bool WssTcpBridge::start(quint16 port, const QUrl& wssUrl)
{
    stop();
    m_wssUrl = wssUrl;
    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        qCWarning(lcBridge) << "listen failed on" << port << m_server->errorString();
        return false;
    }
    qCDebug(lcBridge) << "TCP bridge listening on 127.0.0.1:" << port << "->" << wssUrl;
    return true;
}

void WssTcpBridge::stop()
{
    if (m_bridgeSession) {
        static_cast<BridgeConnection*>(m_bridgeSession)->requestShutdown();
    }
    if (m_server->isListening()) {
        m_server->close();
    }
}

void WssTcpBridge::_bridgeRegisterSession(void* bridgeConnection)
{
    m_bridgeSession = bridgeConnection;
}

void WssTcpBridge::_bridgeUnregisterSession(void* bridgeConnection)
{
    if (m_bridgeSession == bridgeConnection) {
        m_bridgeSession = nullptr;
    }
}

void WssTcpBridge::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* tcp = m_server->nextPendingConnection();
        tcp->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        // OpenVPN may briefly overlap two local TCP connects on restart; killing the first session
        // caused live tunnels to reset. Reject the extra socket — the active tunnel stays up.
        if (m_bridgeSession) {
            qCWarning(lcBridge) << "Rejecting extra TCP to bridge port" << m_server->serverPort()
                                << "(active OpenVPN session already bound)";
            tcp->abort();
            tcp->deleteLater();
            continue;
        }
        new BridgeConnection(tcp, m_wssUrl, this);
    }
}
