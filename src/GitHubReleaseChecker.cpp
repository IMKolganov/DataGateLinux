#include "GitHubReleaseChecker.h"
#include "AppLogging.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVersionNumber>

namespace {

QVersionNumber parseReleaseTag(const QString& tagName)
{
    QString t = tagName.trimmed();
    if (t.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        t = t.mid(1);
    }
    return QVersionNumber::fromString(t);
}

} // namespace

GitHubReleaseChecker::GitHubReleaseChecker(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent)
    , m_nam(nam)
{
}

void GitHubReleaseChecker::start(const QString& owner, const QString& repo)
{
    if (!m_nam || owner.isEmpty() || repo.isEmpty()) {
        emit finished(false, QString(), QUrl());
        return;
    }

    const QUrl apiUrl(
        QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest").arg(owner, repo));
    QNetworkRequest req(apiUrl);
    req.setRawHeader("Accept", QByteArrayLiteral("application/vnd.github+json"));
    req.setRawHeader(
        "User-Agent",
        QByteArrayLiteral("DataGateLinux/") + QCoreApplication::applicationVersion().toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcUi, "GitHub release check failed: %s", qPrintable(reply->errorString()));
            emit finished(false, QString(), QUrl());
            return;
        }
        QJsonParseError pe{};
        const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            qCDebug(lcUi, "GitHub release check: invalid JSON");
            emit finished(false, QString(), QUrl());
            return;
        }
        const QJsonObject o = doc.object();
        const QString tagName = o.value(QStringLiteral("tag_name")).toString();
        const QString htmlUrl = o.value(QStringLiteral("html_url")).toString();
        if (tagName.isEmpty() || htmlUrl.isEmpty()) {
            emit finished(false, QString(), QUrl());
            return;
        }

        const QVersionNumber latest = parseReleaseTag(tagName);
        const QVersionNumber current = QVersionNumber::fromString(QCoreApplication::applicationVersion());
        const bool haveBoth = !latest.isNull() && !current.isNull();
        const bool newer = haveBoth && QVersionNumber::compare(latest, current) > 0;
        emit finished(newer, tagName, QUrl(htmlUrl));
    });
}
