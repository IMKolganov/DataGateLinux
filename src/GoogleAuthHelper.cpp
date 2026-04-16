#include "GoogleAuthHelper.h"

#include "AppLogging.h"
#include "DatagateTr.h"
#include "DatagateUtils.h"

#include <QDesktopServices>
#include <QFile>
#include <QIODevice>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <QCryptographicHash>
#include <memory>

Q_LOGGING_CATEGORY(lcGoogle, "datagate.google")

namespace {

QString base64Url(const QByteArray& data)
{
    return QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString randomTokenUrlSafe(int length)
{
    QByteArray bytes(32, 0);
    QFile urandom(QStringLiteral("/dev/urandom"));
    if (urandom.open(QIODevice::ReadOnly) && urandom.read(bytes.data(), 32) == 32) {
        // ok
    } else {
        for (int i = 0; i < 32; ++i) {
            bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
        }
    }
    return base64Url(bytes).left(length);
}

QString pkceVerifier()
{
    QByteArray verifierBytes(32, 0);
    QFile urandom(QStringLiteral("/dev/urandom"));
    if (urandom.open(QIODevice::ReadOnly) && urandom.read(verifierBytes.data(), 32) == 32) {
        // ok
    } else {
        for (int i = 0; i < 32; ++i) {
            verifierBytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
        }
    }
    return base64Url(verifierBytes);
}

QString pkceChallengeS256(const QString& codeVerifier)
{
    // Same as DataGateWin (ASCII verifier bytes; base64url alphabet matches UTF-8 for this subset).
    const QByteArray hash = QCryptographicHash::hash(codeVerifier.toLatin1(), QCryptographicHash::Sha256);
    return base64Url(hash);
}

/// Parse query parameters from a raw query string (as in redirect); percent-decode without mangling a long `code`.
static QMap<QString, QString> parseQueryParams(const QByteArray& query)
{
    QMap<QString, QString> out;
    const QList<QByteArray> pairs = query.split('&');
    for (const QByteArray& pair : pairs) {
        if (pair.isEmpty()) {
            continue;
        }
        const int eq = pair.indexOf('=');
        if (eq <= 0) {
            continue;
        }
        const QString key = QUrl::fromPercentEncoding(pair.left(eq));
        const QString val = QUrl::fromPercentEncoding(pair.mid(eq + 1));
        out.insert(key, val);
    }
    return out;
}

} // namespace

GoogleAuthHelper::GoogleAuthHelper(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent)
    , m_nam(nam)
{
}

void GoogleAuthHelper::stopOAuthTimer()
{
    if (m_oauthTimeout) {
        m_oauthTimeout->stop();
        m_oauthTimeout->deleteLater();
        m_oauthTimeout = nullptr;
    }
}

void GoogleAuthHelper::cancelOAuth()
{
    qCDebug(lcOAuth, "cancelOAuth");
    stopOAuthTimer();
    if (m_pendingServer) {
        m_pendingServer->close();
        m_pendingServer->deleteLater();
        m_pendingServer = nullptr;
    }
}

void GoogleAuthHelper::signInWithGoogle(const QString& clientId, int redirectPort, const QString& apiBaseUrl)
{
    cancelOAuth();

    qCInfo(lcOAuth, "signInWithGoogle: redirectPort=%d apiBase=%s", redirectPort, qPrintable(apiBaseUrl.trimmed()));

    if (!m_nam || clientId.trimmed().isEmpty()) {
        emit finishedError(Datagate::tr("Google Client ID is required."));
        return;
    }
    if (redirectPort <= 0 || redirectPort > 65535) {
        emit finishedError(Datagate::tr("Invalid OAuth redirect port."));
        return;
    }

    const QString state = randomTokenUrlSafe(43);
    const QString codeVerifier = pkceVerifier();
    const QString codeChallenge = pkceChallengeS256(codeVerifier);

    auto* server = new QTcpServer(this);
    m_pendingServer = server;

    // Same as DataGateWin: single configured port. Scanning ports breaks the redirect_uri registered
    // in Google Cloud and yields invalid_grant on token exchange.
    const QString redirectUri = QStringLiteral("http://127.0.0.1:%1/").arg(redirectPort);
    if (!server->listen(QHostAddress::LocalHost, static_cast<quint16>(redirectPort))) {
        qCWarning(lcOAuth, "listen failed: %s", qPrintable(server->errorString()));
        emit finishedError(
            Datagate::tr("Could not listen on port %1 for OAuth: %2. "
                         "Close another DataGate instance or change RedirectPort in appsettings "
                         "and add http://127.0.0.1:PORT/ in Google Cloud (same as Windows).")
                .arg(redirectPort)
                .arg(server->errorString()));
        server->deleteLater();
        m_pendingServer = nullptr;
        return;
    }

    m_oauthTimeout = new QTimer(this);
    m_oauthTimeout->setSingleShot(true);
    m_oauthTimeout->setInterval(300000); // 5 min if user closed tab without redirect
    connect(m_oauthTimeout, &QTimer::timeout, this, [this]() {
        if (!m_pendingServer) {
            return;
        }
        cancelOAuth();
        emit finishedError(QStringLiteral("[silent]")
            + Datagate::tr("Sign-in timed out or the browser tab was closed. Click Sign in with Google again."));
    });
    m_oauthTimeout->start();

    qCInfo(lcOAuth, "OAuth loopback listening on 127.0.0.1:%d", redirectPort);

    QUrl auth(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
    QUrlQuery aq;
    aq.addQueryItem(QStringLiteral("client_id"), clientId);
    aq.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    aq.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    aq.addQueryItem(QStringLiteral("scope"), QStringLiteral("openid email profile"));
    aq.addQueryItem(QStringLiteral("state"), state);
    aq.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
    aq.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    aq.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    aq.addQueryItem(QStringLiteral("prompt"), QStringLiteral("select_account"));
    auth.setQuery(aq);

    connect(server, &QTcpServer::newConnection, this, [this, server, state, codeVerifier, redirectUri, apiBaseUrl]() {
        stopOAuthTimer();
        qCInfo(lcOAuth, "OAuth redirect received from browser");

        QTcpSocket* sock = server->nextPendingConnection();
        if (!sock) {
            return;
        }
        auto accumulated = std::make_shared<QByteArray>();
        auto done = std::make_shared<bool>(false);
        connect(sock, &QTcpSocket::readyRead, this, [this, sock, server, state, codeVerifier, redirectUri, apiBaseUrl, accumulated, done]() {
            if (*done) {
                return;
            }
            *accumulated += sock->readAll();
            if (!accumulated->contains("\r\n\r\n")) {
                return;
            }
            *done = true;

            const int firstLineEnd = accumulated->indexOf("\r\n");
            if (firstLineEnd < 0) {
                emit finishedError(Datagate::tr("Invalid OAuth HTTP request."));
                sock->close();
                server->close();
                if (m_pendingServer == server) {
                    m_pendingServer = nullptr;
                }
                server->deleteLater();
                return;
            }
            const QByteArray firstLineBytes = accumulated->left(firstLineEnd);
            const int sp1 = firstLineBytes.indexOf(' ');
            const int sp2 = firstLineBytes.indexOf(' ', sp1 + 1);
            if (sp1 < 0 || sp2 < 0 || sp2 <= sp1) {
                emit finishedError(Datagate::tr("Invalid OAuth HTTP request line."));
                sock->close();
                server->close();
                if (m_pendingServer == server) {
                    m_pendingServer = nullptr;
                }
                server->deleteLater();
                return;
            }
            const QByteArray pathAndQueryBytes = firstLineBytes.mid(sp1 + 1, sp2 - sp1 - 1);
            const int qm = pathAndQueryBytes.indexOf('?');
            QMap<QString, QString> q;
            if (qm >= 0) {
                q = parseQueryParams(pathAndQueryBytes.mid(qm + 1));
            }

            const QString err = q.value(QStringLiteral("error"));
            if (!err.isEmpty()) {
                emit finishedError(Datagate::tr("Google OAuth: %1 %2")
                    .arg(err, q.value(QStringLiteral("error_description"))));
                sock->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
                    "<html><body>Error. You can close this window.</body></html>");
                sock->close();
                server->close();
                if (m_pendingServer == server) {
                    m_pendingServer = nullptr;
                }
                server->deleteLater();
                return;
            }

            if (q.value(QStringLiteral("state")) != state) {
                emit finishedError(Datagate::tr("OAuth state mismatch."));
                sock->write("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
                sock->close();
                server->close();
                if (m_pendingServer == server) {
                    m_pendingServer = nullptr;
                }
                server->deleteLater();
                return;
            }

            const QString code = q.value(QStringLiteral("code"));
            if (code.isEmpty()) {
                emit finishedError(Datagate::tr("Authorization code not received."));
                sock->write("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
                sock->close();
                server->close();
                if (m_pendingServer == server) {
                    m_pendingServer = nullptr;
                }
                server->deleteLater();
                return;
            }

            sock->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
                "<html><body>You can close this window.</body></html>");
            sock->close();
            server->close();
            if (m_pendingServer == server) {
                m_pendingServer = nullptr;
            }
            server->deleteLater();

            QString base = apiBaseUrl.trimmed();
            while (base.endsWith(QLatin1Char('/'))) {
                base.chop(1);
            }
            const QUrl apiUrl(base + QStringLiteral("/api/auth/google-code-login"));

            QJsonObject body;
            body.insert(QStringLiteral("code"), code);
            body.insert(QStringLiteral("codeVerifier"), codeVerifier);
            body.insert(QStringLiteral("redirectUri"), redirectUri);

            QNetworkRequest req(apiUrl);
            req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
            QNetworkReply* rep = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
            connect(rep, &QNetworkReply::finished, this, [this, rep]() {
                rep->deleteLater();
                const QByteArray raw = rep->readAll();
                const QString unavailable = DatagateUtils::userMessageWhenApiUnavailable(rep);
                if (!unavailable.isEmpty()) {
                    emit finishedError(unavailable);
                    return;
                }
                if (rep->error() != QNetworkReply::NoError) {
                    emit finishedError(Datagate::tr("API login: %1 %2").arg(rep->errorString(), QString::fromUtf8(raw)));
                    return;
                }
                QJsonParseError pe{};
                const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
                if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
                    emit finishedError(Datagate::tr("Invalid JSON from API."));
                    return;
                }
                const QJsonObject root = doc.object();
                if (!root.value(QStringLiteral("success")).toBool(false)) {
                    emit finishedError(root.value(QStringLiteral("message")).toString());
                    return;
                }
                const QJsonObject data = root.value(QStringLiteral("data")).toObject();
                const QString token = data.value(QStringLiteral("token")).toString();
                if (token.isEmpty()) {
                    emit finishedError(Datagate::tr("No token in API response."));
                    return;
                }
                qCInfo(lcOAuth, "google-code-login API success");
                emit finishedSuccess(data);
            });
        });
    });

    if (!QDesktopServices::openUrl(auth)) {
        stopOAuthTimer();
        server->close();
        if (m_pendingServer == server) {
            m_pendingServer = nullptr;
        }
        server->deleteLater();
        emit finishedError(Datagate::tr("Could not open browser."));
        return;
    }
}
