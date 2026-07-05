#include "GoogleAuthHelper.h"

#include "AppLogging.h"
#include "BrandColors.h"
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

/// Ensure full send before the socket is closed (avoids empty/partial responses in the browser).
bool writeAllToSocket(QTcpSocket* sock, const QByteArray& data)
{
    qint64 total = 0;
    while (total < data.size()) {
        const qint64 n = sock->write(data.constData() + total, data.size() - total);
        if (n < 0) {
            return false;
        }
        total += n;
        if (!sock->waitForBytesWritten(8000)) {
            return false;
        }
    }
    return true;
}

/// CSS variables + rules for OAuth loopback page (aligned with design/datagate-brand.css and BrandColors.h).
QString oauthLoopbackPageCss()
{
    using namespace BrandColors;
    const QString vars = QStringLiteral(
        ":root{"
        "--bg-default:%1;--bg-muted:%2;--bg-subtle:%3;"
        "--text-default:%4;--text-muted:%5;--accent-fg:%6;--accent-emphasis:%7;--border-default:%8;"
        "--danger-fg:%9;--success-fg:%10;"
        "}"
        "@media (prefers-color-scheme:light){:root{"
        "--bg-default:%11;--bg-muted:%12;--bg-subtle:%13;"
        "--text-default:%14;--text-muted:%15;--accent-fg:%16;--accent-emphasis:%16;--border-default:%17;"
        "}}")
        .arg(QString::fromUtf8(Dark::kBgDefault),
            QString::fromUtf8(Dark::kBgMuted),
            QString::fromUtf8(Dark::kBgSubtle),
            QString::fromUtf8(Dark::kTextDefault),
            QString::fromUtf8(Dark::kTextMuted),
            QString::fromUtf8(Dark::kAccentFg),
            QString::fromUtf8(Dark::kAccentEmphasis),
            QString::fromUtf8(Dark::kBorderDefault),
            QString::fromUtf8(Dark::kDanger),
            QString::fromUtf8(Dark::kSuccess),
            QString::fromUtf8(Light::kBgDefault),
            QString::fromUtf8(Light::kBgMuted),
            QString::fromUtf8(Light::kBgSubtle),
            QString::fromUtf8(Light::kTextDefault),
            QString::fromUtf8(Light::kTextMuted),
            QString::fromUtf8(Light::kAccentFg),
            QString::fromUtf8(Light::kBorderDefault));
    return vars
        + QStringLiteral(
              "*{box-sizing:border-box;}"
              "html{font-size:16px;-webkit-font-smoothing:antialiased;}"
              "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI','Noto Sans',Helvetica,Arial,sans-serif;"
              "background:var(--bg-default);color:var(--text-default);min-height:100vh;margin:0;"
              "display:flex;align-items:center;justify-content:center;padding:24px;line-height:1.5;}"
              ".oauth-wrap{width:100%;max-width:480px;}"
              ".oauth-card{background:var(--bg-muted);border:1px solid var(--border-default);border-radius:8px;"
              "padding:32px 28px 28px;text-align:center;"
              "box-shadow:0 8px 24px rgba(1,4,9,.4);}"
              "@media (prefers-color-scheme:light){.oauth-card{box-shadow:0 8px 24px rgba(31,35,40,.1);}}"
              ".brand{font-size:1.0625rem;font-weight:600;color:var(--text-default);margin-bottom:20px;}"
              "h1{font-size:1.25rem;font-weight:600;margin:0 0 12px;line-height:1.35;color:var(--text-default);}"
              ".oauth-main p{font-size:.95rem;color:var(--text-muted);margin:0;line-height:1.55;}"
              ".ok{width:64px;height:64px;margin:0 auto 20px;border-radius:50%;background:var(--bg-subtle);"
              "color:var(--success-fg);display:flex;align-items:center;justify-content:center;font-size:32px;font-weight:600;}"
              ".bad{width:64px;height:64px;margin:0 auto 20px;border-radius:50%;background:var(--bg-subtle);"
              "color:var(--danger-fg);display:flex;align-items:center;justify-content:center;font-size:30px;font-weight:600;}"
              ".oauth-detail{margin-top:16px;font-size:.85rem;color:var(--text-muted);word-break:break-word;}"
              ".oauth-footer{margin-top:24px;padding-top:20px;border-top:1px solid var(--border-default);text-align:center;}"
              ".oauth-more-title{font-weight:600;font-size:.9375rem;color:var(--text-default);margin:14px 0 8px;}"
              ".oauth-muted{font-size:.8125rem;color:var(--text-muted);margin:0 0 8px;line-height:1.45;}"
              ".oauth-line{margin:6px 0;font-size:.875rem;}"
              ".oauth-a{color:var(--accent-fg);text-decoration:none;}"
              ".oauth-a:hover{text-decoration:underline;}");
}

QString oauthHtmlFooter()
{
    const QString more = Datagate::tr("More apps");
    const QString hint = Datagate::tr("You can download other DataGate apps for your devices from the website:");
    const QString site = QStringLiteral("https://datagateapp.com/");
    const QString download = QStringLiteral("https://datagateapp.com/download");
    return QStringLiteral(
               "<footer class=\"oauth-footer\" role=\"contentinfo\">"
               "<p class=\"oauth-line\"><a class=\"oauth-a\" href=\"%1\" rel=\"noopener noreferrer\">%1</a></p>"
               "<p class=\"oauth-more-title\">%2</p>"
               "<p class=\"oauth-muted\">%3</p>"
               "<p class=\"oauth-line\"><a class=\"oauth-a\" href=\"%4\" rel=\"noopener noreferrer\">%4</a></p>"
               "</footer>")
        .arg(site, more.toHtmlEscaped(), hint.toHtmlEscaped(), download);
}

/// Minimal UTF-8 HTML response for the OAuth loopback browser tab.
QByteArray httpUtf8Html(int statusCode, const QString& statusPhrase, const QString& htmlBody)
{
    const QByteArray body = htmlBody.toUtf8();
    QByteArray r = "HTTP/1.1 " + QByteArray::number(statusCode) + ' ' + statusPhrase.toUtf8() + "\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n";
    r += body;
    return r;
}

QString oauthCallbackShell(const QString& innerBody)
{
    QString html;
    html += QStringLiteral("<!DOCTYPE html><html lang=\"en\"><head>"
                           "<meta charset=\"utf-8\">"
                           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                           "<meta name=\"color-scheme\" content=\"dark light\">"
                           "<meta name=\"theme-color\" content=\"#0d1117\">"
                           "<title>DataGate</title><style>");
    html += oauthLoopbackPageCss();
    html += QStringLiteral("</style></head><body><div class=\"oauth-wrap\"><div class=\"oauth-card\">");
    html += innerBody;
    html += oauthHtmlFooter();
    html += QStringLiteral("</div></div></body></html>");
    return html;
}

QString oauthSuccessHtml()
{
    const QString h = Datagate::tr("You're signed in");
    const QString p = Datagate::tr("You can close this tab and return to the DataGate app.");
    return oauthCallbackShell(
        QStringLiteral("<div class=\"brand\">DataGate</div><div class=\"oauth-main\"><div class=\"ok\">✓</div><h1>%1</h1><p>%2</p></div>")
            .arg(h.toHtmlEscaped(), p.toHtmlEscaped()));
}

QString oauthGoogleErrorHtml(const QString& errorCode, const QString& errorDescription)
{
    const QString h = Datagate::tr("Sign-in did not complete");
    const QString hint = Datagate::tr("You can close this tab and try again from DataGate.");
    const QString detail = QStringLiteral("%1 — %2").arg(errorCode, errorDescription);
    return oauthCallbackShell(
        QStringLiteral("<div class=\"brand\">DataGate</div><div class=\"oauth-main\"><div class=\"bad\">✕</div><h1>%1</h1><p>%2</p><p class=\"oauth-detail\">%3</p></div>")
            .arg(h.toHtmlEscaped(), hint.toHtmlEscaped(), detail.toHtmlEscaped()));
}

QString oauthBadRequestHtml()
{
    const QString h = Datagate::tr("Invalid sign-in request");
    const QString p = Datagate::tr("Close this tab and start sign-in again from DataGate.");
    return oauthCallbackShell(
        QStringLiteral("<div class=\"brand\">DataGate</div><div class=\"oauth-main\"><div class=\"bad\">!</div><h1>%1</h1><p>%2</p></div>")
            .arg(h.toHtmlEscaped(), p.toHtmlEscaped()));
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

            // Browsers often open a second connection for /favicon.ico (no query). Treating that as
            // OAuth would mismatch `state` and close the server before the real redirect is handled.
            if (!q.contains(QStringLiteral("code")) && !q.contains(QStringLiteral("error"))) {
                qCInfo(lcOAuth, "loopback: ignoring non-callback request (e.g. favicon): %s",
                    pathAndQueryBytes.constData());
                static const QByteArray noContent = QByteArrayLiteral(
                    "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n");
                (void)writeAllToSocket(sock, noContent);
                sock->close();
                return;
            }

            const QString err = q.value(QStringLiteral("error"));
            if (!err.isEmpty()) {
                emit finishedError(Datagate::tr("Google OAuth: %1 %2")
                    .arg(err, q.value(QStringLiteral("error_description"))));
                (void)writeAllToSocket(sock, httpUtf8Html(200, QStringLiteral("OK"),
                    oauthGoogleErrorHtml(err, q.value(QStringLiteral("error_description")))));
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
                (void)writeAllToSocket(sock, httpUtf8Html(400, QStringLiteral("Bad Request"), oauthBadRequestHtml()));
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
                (void)writeAllToSocket(sock, httpUtf8Html(400, QStringLiteral("Bad Request"), oauthBadRequestHtml()));
                sock->close();
                server->close();
                if (m_pendingServer == server) {
                    m_pendingServer = nullptr;
                }
                server->deleteLater();
                return;
            }

            (void)writeAllToSocket(sock, httpUtf8Html(200, QStringLiteral("OK"), oauthSuccessHtml()));
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
                const bool requiresTotp = data.value(QStringLiteral("requiresTotp")).toBool(false)
                    || data.value(QStringLiteral("RequiresTotp")).toBool(false);
                QString challengeId = data.value(QStringLiteral("loginChallengeId")).toString().trimmed();
                if (challengeId.isEmpty()) {
                    challengeId = data.value(QStringLiteral("LoginChallengeId")).toString().trimmed();
                }
                const QString token = data.value(QStringLiteral("token")).toString();
                if (requiresTotp && !challengeId.isEmpty()) {
                    qCInfo(lcOAuth, "google-code-login: admin TOTP challenge");
                    emit finishedSuccess(data);
                    return;
                }
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
