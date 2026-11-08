#include "ClientIdentity.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include "AppLogging.h"

namespace ClientIdentity {

namespace {
QUrl g_backendBase;

QJsonValue valueCi(const QJsonObject& o, const QString& key)
{
    for (auto it = o.begin(); it != o.end(); ++it) {
        if (it.key().compare(key, Qt::CaseInsensitive) == 0) {
            return it.value();
        }
    }
    return {};
}
} // namespace

QString clientVersion()
{
    return sanitizeVersion(QCoreApplication::applicationVersion());
}

QString sanitizeVersion(const QString& raw)
{
    QString out;
    out.reserve(raw.size());
    for (const QChar c : raw) {
        if (c.unicode() >= 0x20 && c.unicode() <= 0x7E) {
            out.append(c);
        }
    }
    out = out.trimmed().left(kMaxVersionLength).trimmed();
    return out.isEmpty() ? QStringLiteral("0") : out;
}

void setBackendBaseUrl(const QUrl& baseUrl)
{
    g_backendBase = baseUrl;
}

QUrl backendBaseUrl()
{
    return g_backendBase;
}

bool isBackendUrl(const QUrl& url, const QUrl& backendBase)
{
    if (!url.isValid() || !backendBase.isValid() || backendBase.host().isEmpty()) {
        return false;
    }
    const QString scheme = url.scheme().toLower();
    const int defaultPort = scheme == QLatin1String("https") ? 443 : 80;
    return scheme == backendBase.scheme().toLower()
        && url.host().compare(backendBase.host(), Qt::CaseInsensitive) == 0
        && url.port(defaultPort) == backendBase.port(defaultPort);
}

void apply(QNetworkRequest& request, const QString& version)
{
    request.setRawHeader(kNameHeader, kClientName);
    request.setRawHeader(kVersionHeader, version.toLatin1());
}

QUrl appendToHubUrl(const QUrl& url, const QString& version)
{
    QUrl out(url);
    QUrlQuery q(out);
    q.removeAllQueryItems(QString::fromLatin1(kNameQuery));
    q.removeAllQueryItems(QString::fromLatin1(kVersionQuery));
    q.addQueryItem(QString::fromLatin1(kNameQuery), QString::fromLatin1(kClientName));
    q.addQueryItem(QString::fromLatin1(kVersionQuery), version);
    out.setQuery(q);
    return out;
}

QString rejectionCode(const QByteArray& body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        return {};
    }
    const QJsonValue data = valueCi(doc.object(), QStringLiteral("data"));
    if (!data.isObject()) {
        return {};
    }
    const QString code = valueCi(data.toObject(), QStringLiteral("code")).toString().trimmed();
    return code.startsWith(QLatin1String("client_")) ? code : QString();
}

Events* Events::instance()
{
    static Events s;
    return &s;
}

void Events::notifyRejected(const QString& code)
{
    qCWarning(lcAuth, "Backend rejected client identity (%s): update the app", qPrintable(code));
    if (m_emitted) {
        return;
    }
    m_emitted = true;
    emit rejected(code);
}

} // namespace ClientIdentity

DatagateNetworkAccessManager::DatagateNetworkAccessManager(QObject* parent)
    : QNetworkAccessManager(parent)
{
}

QNetworkReply* DatagateNetworkAccessManager::createRequest(
    Operation op, const QNetworkRequest& request, QIODevice* outgoingData)
{
    if (!ClientIdentity::isBackendUrl(request.url(), ClientIdentity::backendBaseUrl())) {
        return QNetworkAccessManager::createRequest(op, request, outgoingData);
    }

    QNetworkRequest withIdentity(request);
    ClientIdentity::apply(withIdentity);
    QNetworkReply* reply = QNetworkAccessManager::createRequest(op, withIdentity, outgoingData);

    // Peek on finish so the caller still reads the full body.
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply]() {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 400) {
            return;
        }
        const QString code = ClientIdentity::rejectionCode(reply->peek(reply->bytesAvailable()));
        if (!code.isEmpty()) {
            ClientIdentity::Events::instance()->notifyRejected(code);
        }
    });
    return reply;
}
