#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QTcpServer;
class QTimer;

/// PKCE + loopback redirect, then POST /api/auth/google-code-login (same flow as DataGateWin.GoogleAuthService).
class GoogleAuthHelper final : public QObject {
    Q_OBJECT
public:
    explicit GoogleAuthHelper(QNetworkAccessManager* nam, QObject* parent = nullptr);

    void signInWithGoogle(const QString& clientId, int redirectPort, const QString& apiBaseUrl);
    /// Stop waiting for redirect (tab closed, cancel, new attempt).
    void cancelOAuth();

signals:
    /// `data` field from ApiResponse (same shape as GoogleLoginResponse / AuthTokensResponse).
    void finishedSuccess(const QJsonObject& data);
    void finishedError(const QString& message);

private:
    void stopOAuthTimer();

    QNetworkAccessManager* m_nam = nullptr;
    QTcpServer* m_pendingServer = nullptr;
    QTimer* m_oauthTimeout = nullptr;
};
