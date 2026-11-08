#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkRequest;

/// Who is calling the DataGateMonitor backend: `X-Client-Name: linux` + the app build version.
///
/// Source of truth: NuGet DataGateMonitor.SharedModels (1.0.89+),
/// `DataGateMonitor.SharedModels.ClientIdentity.ClientIdentityRules` / `ClientNames`.
/// When the backend rules change, change these values together with them.
namespace ClientIdentity {

inline constexpr char kNameHeader[] = "X-Client-Name";
inline constexpr char kVersionHeader[] = "X-Client-Version";
inline constexpr char kNameQuery[] = "clientName";
inline constexpr char kVersionQuery[] = "clientVersion";
inline constexpr char kClientName[] = "linux";
inline constexpr int kMaxVersionLength = 64;

/// QCoreApplication::applicationVersion() (CMake PROJECT_VERSION), made backend-safe.
QString clientVersion();

/// Printable ASCII only (0x20–0x7E), 1–64 chars; "0" when nothing is left.
QString sanitizeVersion(const QString& raw);

/// Base URL of the backend (Api:BaseUrl). Requests to other hosts never get the headers.
void setBackendBaseUrl(const QUrl& baseUrl);
QUrl backendBaseUrl();

/// Same scheme, host and port as @p backendBase.
bool isBackendUrl(const QUrl& url, const QUrl& backendBase);

/// Sets both headers on @p request (overwrites).
void apply(QNetworkRequest& request, const QString& version = clientVersion());

/// Adds clientName/clientVersion to a SignalR / WebSocket `/api/hubs/*` URL, keeping the existing query.
QUrl appendToHubUrl(const QUrl& url, const QString& version = clientVersion());

/// `data.code` of a client-identity 400 (`client_name_missing`, …) or empty for any other body.
QString rejectionCode(const QByteArray& body);

/// Emits rejected() once per process when the backend refuses this build.
class Events : public QObject {
    Q_OBJECT
public:
    static Events* instance();
    void notifyRejected(const QString& code);

signals:
    void rejected(const QString& code);

private:
    bool m_emitted = false;
};

} // namespace ClientIdentity

/// The one QNetworkAccessManager for backend traffic: adds the client identity headers to every
/// request whose URL is on the backend host and reports `client_*` 400 rejections.
/// Other hosts (GitHub releases, Google OAuth) pass through untouched; VPN node WSS uses QWebSocket.
class DatagateNetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT
public:
    explicit DatagateNetworkAccessManager(QObject* parent = nullptr);

protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& request, QIODevice* outgoingData) override;
};
