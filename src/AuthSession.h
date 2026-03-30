#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>

class QNetworkAccessManager;

/// Same idea as DataGateWin.AuthSession + FileTokenStore: auth.json on disk, refresh via /api/auth/refresh.
class AuthSession final : public QObject {
    Q_OBJECT
public:
    explicit AuthSession(QNetworkAccessManager* nam, QObject* parent = nullptr);

    void initialize();

    QString displayName() const { return m_tokens.displayName; }

    /// Load from disk and refresh synchronously when needed.
    bool ensureValidAccessToken();

    QString accessToken() const;

    void setFromLoginJson(const QJsonObject& data);
    void logout();

private:
    bool loadFromFile();
    bool saveToFile() const;
    bool clearFile();

    static QString tokenFilePath();
    static QString getOrCreateDeviceId();

    bool isAccessValid() const;
    bool refreshTokenSync();

    void applyJsonObject(const QJsonObject& o);
    /// If access is still "invalid" after refresh, set expiration from JWT or a short TTL so we do not refresh on every call.
    void bumpAccessExpirationIfInvalid();

    QNetworkAccessManager* m_nam = nullptr;

    struct Tokens {
        QString token;
        QDateTime expiration;
        QString refreshToken;
        QDateTime refreshExpiration;
        QString displayName;
    } m_tokens;

    mutable QMutex m_mutex;
    /// Serializes refresh so concurrent ensureValidAccessToken calls do not POST in parallel (401 / invalidation).
    QMutex m_refreshMutex;
};
