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

private:
    void onNewConnection();

    QTcpServer* m_server = nullptr;
    QUrl m_wssUrl;
};
