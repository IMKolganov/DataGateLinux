#include "XrayClientLinksApi.h"
#include "DatagateTr.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace XrayClientLinksApi {
namespace {

QString normalizeBase(QString base)
{
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    return base;
}

QByteArray runPost(QNetworkAccessManager* nam, const QNetworkRequest& req, const QByteArray& body, int* httpOut)
{
    QNetworkReply* reply = nam->post(req, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(60000, &loop, &QEventLoop::quit);
    loop.exec();
    if (httpOut) {
        *httpOut = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    const QByteArray raw = reply->readAll();
    reply->deleteLater();
    return raw;
}

QString apiError(const QByteArray& raw, const QString& fallback)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject o = doc.object();
        const QString msg = o.value(QStringLiteral("message")).toString();
        if (!msg.isEmpty()) {
            return msg;
        }
        const QString errStr = o.value(QStringLiteral("error")).toString();
        if (!errStr.isEmpty()) {
            return errStr;
        }
    }
    if (!raw.isEmpty()) {
        return QString::fromUtf8(raw.left(800));
    }
    return fallback;
}

bool looksNotFound(int http, const QByteArray& raw)
{
    if (http == 404) {
        return true;
    }
    if (http != 400) {
        return false;
    }
    const QString txt = QString::fromUtf8(raw);
    return txt.contains(QStringLiteral("not found"), Qt::CaseInsensitive);
}

bool looksAlreadyExists(int http, const QByteArray& raw)
{
    if (http == 409) {
        return true;
    }
    const QString txt = QString::fromUtf8(raw);
    return txt.contains(QStringLiteral("already exists"), Qt::CaseInsensitive)
        || txt.contains(QStringLiteral("already exist"), Qt::CaseInsensitive)
        || txt.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive);
}

std::optional<QString> decodeContentBase64(const QByteArray& raw, QString* errorOut)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Invalid JSON while loading Xray link.");
        }
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QString b64 = data.value(QStringLiteral("content")).toString();
    if (b64.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Xray link response has no content (base64).");
        }
        return std::nullopt;
    }
    return QString::fromUtf8(QByteArray::fromBase64(b64.toUtf8()));
}

std::optional<IssuedLink> issuedFromDownload(const QByteArray& raw, const QString& commonName, QString* errorOut)
{
    const auto text = decodeContentBase64(raw, errorOut);
    if (!text.has_value()) {
        return std::nullopt;
    }
    IssuedLink link;
    link.contentUtf8 = *text;
    link.commonName = commonName;
    return link;
}

QStringList uniqueNonEmpty(const QStringList& in)
{
    QStringList out;
    for (const QString& s : in) {
        const QString t = s.trimmed();
        if (!t.isEmpty() && !out.contains(t)) {
            out << t;
        }
    }
    return out;
}

} // namespace

std::optional<IssuedLink> ensureAndDownloadByCn(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    int vpnServerId,
    const QStringList& commonNames,
    const QString& externalId,
    const QString& issuedTo,
    QString* errorOut)
{
    const QStringList cnCandidates = uniqueNonEmpty(commonNames);
    if (!nam || apiBaseUrl.trimmed().isEmpty() || bearerToken.trimmed().isEmpty() || vpnServerId <= 0
        || cnCandidates.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Not signed in or missing server id.");
        }
        return std::nullopt;
    }

    const QString base = normalizeBase(apiBaseUrl);
    const QByteArray auth = (QStringLiteral("Bearer ") + bearerToken.trimmed()).toUtf8();
    const QString primaryCn = cnCandidates.first();

    auto downloadOnce = [&](const QString& cn, int* httpOut) -> QByteArray {
        QJsonObject body;
        body.insert(QStringLiteral("vpnServerId"), vpnServerId);
        body.insert(QStringLiteral("commonName"), cn);
        QNetworkRequest req(QUrl(base + QStringLiteral("/api/xray-client-links/download-file-by-cn")));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
        req.setRawHeader("Accept", "application/json");
        req.setRawHeader("Authorization", auth);
        return runPost(nam, req, QJsonDocument(body).toJson(QJsonDocument::Compact), httpOut);
    };

    auto tryDownloadAny = [&]() -> std::optional<IssuedLink> {
        QString lastHardErr;
        for (const QString& cn : cnCandidates) {
            int http = 0;
            const QByteArray raw = downloadOnce(cn, &http);
            if (http == 200) {
                return issuedFromDownload(raw, cn, errorOut);
            }
            if (!looksNotFound(http, raw)) {
                lastHardErr = apiError(raw, Datagate::tr("Xray link download failed (HTTP %1).").arg(http));
            }
        }
        if (errorOut && !lastHardErr.isEmpty()) {
            *errorOut = lastHardErr;
        }
        return std::nullopt;
    };

    if (auto existing = tryDownloadAny()) {
        return existing;
    }

    QJsonObject createBody;
    createBody.insert(QStringLiteral("vpnServerId"), vpnServerId);
    createBody.insert(QStringLiteral("commonName"), primaryCn);
    createBody.insert(QStringLiteral("externalId"), externalId);
    createBody.insert(QStringLiteral("issuedTo"), issuedTo);
    QNetworkRequest createReq(QUrl(base + QStringLiteral("/api/xray-client-links/add-with-token")));
    createReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    createReq.setRawHeader("Accept", "application/json");
    createReq.setRawHeader("Authorization", auth);
    int createHttp = 0;
    const QByteArray createRaw =
        runPost(nam, createReq, QJsonDocument(createBody).toJson(QJsonDocument::Compact), &createHttp);

    if (createHttp >= 200 && createHttp < 300) {
        QString createDecodeErr;
        if (auto fromCreate = issuedFromDownload(createRaw, primaryCn, &createDecodeErr)) {
            return fromCreate;
        }
        int http = 0;
        const QByteArray raw = downloadOnce(primaryCn, &http);
        if (http == 200) {
            return issuedFromDownload(raw, primaryCn, errorOut);
        }
        if (errorOut) {
            *errorOut = apiError(raw, Datagate::tr("Xray link download failed (HTTP %1).").arg(http));
        }
        return std::nullopt;
    }

    if (looksAlreadyExists(createHttp, createRaw)) {
        if (auto existing = tryDownloadAny()) {
            return existing;
        }
        if (errorOut) {
            *errorOut = apiError(createRaw,
                Datagate::tr(
                    "Xray link already exists on the server, but download failed. "
                    "Import the VLESS URI manually on Profiles, or try another server."));
        }
        return std::nullopt;
    }

    if (errorOut) {
        *errorOut = apiError(createRaw, Datagate::tr("Create Xray link failed (HTTP %1).").arg(createHttp));
    }
    return std::nullopt;
}

} // namespace XrayClientLinksApi
