#include "XrayConfigBuilder.h"
#include "DatagateTr.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace XrayConfigBuilder {
namespace {

QJsonObject socksInbound(quint16 port)
{
    QJsonObject settings;
    settings.insert(QStringLiteral("udp"), true);
    QJsonObject inbound;
    inbound.insert(QStringLiteral("tag"), QStringLiteral("socks-in"));
    inbound.insert(QStringLiteral("listen"), QStringLiteral("127.0.0.1"));
    inbound.insert(QStringLiteral("port"), static_cast<int>(port));
    inbound.insert(QStringLiteral("protocol"), QStringLiteral("socks"));
    inbound.insert(QStringLiteral("settings"), settings);
    return inbound;
}

QJsonObject tunInbound()
{
    // Official Xray TUN settings are only name + MTU (+ userLevel). OS must configure
    // address/routes (see third_party/xray-core/proxy/tun/README.md).
    QJsonObject settings;
    settings.insert(QStringLiteral("name"), QStringLiteral("datagatex0"));
    settings.insert(QStringLiteral("MTU"), 1500);
    QJsonObject inbound;
    inbound.insert(QStringLiteral("tag"), QStringLiteral("tun-in"));
    inbound.insert(QStringLiteral("protocol"), QStringLiteral("tun"));
    inbound.insert(QStringLiteral("port"), 0);
    inbound.insert(QStringLiteral("settings"), settings);
    return inbound;
}

QJsonArray managedClientInbounds(quint16 socksPort, bool useTun)
{
    QJsonArray inbounds;
    if (useTun) {
        inbounds.append(tunInbound());
    }
    inbounds.append(socksInbound(socksPort));
    return inbounds;
}

/// Replace local socks/http/tun inbounds with app-managed ones (AppConfig port / UseTun).
QJsonObject ensureClientInbounds(QJsonObject root, quint16 socksPort, bool useTun)
{
    QJsonArray kept;
    const QJsonArray existing = root.value(QStringLiteral("inbounds")).toArray();
    for (const QJsonValue& v : existing) {
        const QJsonObject o = v.toObject();
        const QString proto = o.value(QStringLiteral("protocol")).toString();
        if (proto.compare(QStringLiteral("socks"), Qt::CaseInsensitive) == 0
            || proto.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
            || proto.compare(QStringLiteral("tun"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        kept.append(o);
    }
    QJsonArray next = managedClientInbounds(socksPort, useTun);
    for (const QJsonValue& v : kept) {
        next.append(v);
    }
    root.insert(QStringLiteral("inbounds"), next);
    return root;
}

QJsonObject wrapClientConfig(const QJsonObject& outbound, quint16 socksPort, bool useTun)
{
    QJsonObject log;
    log.insert(QStringLiteral("loglevel"), QStringLiteral("warning"));

    QJsonObject routing;
    routing.insert(QStringLiteral("domainStrategy"), QStringLiteral("AsIs"));
    QJsonObject rule;
    rule.insert(QStringLiteral("type"), QStringLiteral("field"));
    rule.insert(QStringLiteral("outboundTag"), QStringLiteral("proxy"));
    rule.insert(QStringLiteral("network"), QStringLiteral("tcp,udp"));
    routing.insert(QStringLiteral("rules"), QJsonArray{rule});

    QJsonObject root;
    root.insert(QStringLiteral("log"), log);
    root.insert(QStringLiteral("inbounds"), managedClientInbounds(socksPort, useTun));
    root.insert(QStringLiteral("outbounds"), QJsonArray{outbound});
    root.insert(QStringLiteral("routing"), routing);
    return root;
}

QString queryValue(const QUrlQuery& q, const QString& key)
{
    return q.queryItemValue(key, QUrl::FullyDecoded).trimmed();
}

} // namespace

QString extractVlessUriFromText(const QString& text)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) {
        return {};
    }
    if (t.startsWith(QStringLiteral("vless://"), Qt::CaseInsensitive)) {
        // Single-line share link (stop at whitespace).
        const int sp = t.indexOf(QRegularExpression(QStringLiteral("[\\s]")));
        return (sp > 0 ? t.left(sp) : t).trimmed();
    }
    static const QRegularExpression re(
        QStringLiteral(R"(vless://[^\s\"'<>]+)"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(t);
    if (m.hasMatch()) {
        QString uri = m.captured(0);
        while (!uri.isEmpty()
            && (uri.endsWith(QLatin1Char('}')) || uri.endsWith(QLatin1Char(')'))
                || uri.endsWith(QLatin1Char(',')) || uri.endsWith(QLatin1Char(';')))) {
            uri.chop(1);
        }
        return uri;
    }
    return {};
}

QString clientJsonFromVlessUri(const QString& uriIn, quint16 localSocksPort, bool useTun, QString* errorOut)
{
    QString uri = extractVlessUriFromText(uriIn);
    if (uri.isEmpty()) {
        uri = uriIn.trimmed();
    }
    if (!uri.startsWith(QStringLiteral("vless://"), Qt::CaseInsensitive)) {
        if (errorOut) {
            *errorOut = Datagate::tr("Expected a vless:// link.");
        }
        return {};
    }

    QUrl url(uri);
    if (!url.isValid() || url.userName().isEmpty() || url.host().isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Invalid vless:// URI (need uuid@host:port).");
        }
        return {};
    }
    int port = url.port();
    if (port <= 0) {
        port = 443;
    }

    const QUrlQuery q(url);
    const QString encryption = queryValue(q, QStringLiteral("encryption"));
    const QString security = queryValue(q, QStringLiteral("security"));
    const QString type = queryValue(q, QStringLiteral("type"));
    const QString flow = queryValue(q, QStringLiteral("flow"));
    const QString sni = queryValue(q, QStringLiteral("sni"));
    const QString fp = queryValue(q, QStringLiteral("fp"));
    const QString pbk = queryValue(q, QStringLiteral("pbk"));
    const QString sid = queryValue(q, QStringLiteral("sid"));
    const QString spx = queryValue(q, QStringLiteral("spx"));
    const QString path = queryValue(q, QStringLiteral("path"));
    const QString hostHeader = queryValue(q, QStringLiteral("host"));
    const QString alpn = queryValue(q, QStringLiteral("alpn"));
    const QString serviceName = queryValue(q, QStringLiteral("serviceName"));
    const QString mode = queryValue(q, QStringLiteral("mode"));

    QJsonObject user;
    user.insert(QStringLiteral("id"), url.userName());
    user.insert(QStringLiteral("encryption"), encryption.isEmpty() ? QStringLiteral("none") : encryption);
    if (!flow.isEmpty()) {
        user.insert(QStringLiteral("flow"), flow);
    }

    QJsonObject vnext;
    vnext.insert(QStringLiteral("address"), url.host());
    vnext.insert(QStringLiteral("port"), port);
    vnext.insert(QStringLiteral("users"), QJsonArray{user});

    QJsonObject settings;
    settings.insert(QStringLiteral("vnext"), QJsonArray{vnext});

    QJsonObject stream;
    const QString network = type.isEmpty() ? QStringLiteral("tcp") : type;
    stream.insert(QStringLiteral("network"), network);

    if (!security.isEmpty() && security.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0) {
        stream.insert(QStringLiteral("security"), security);
        if (security.compare(QStringLiteral("reality"), Qt::CaseInsensitive) == 0) {
            QJsonObject reality;
            if (!pbk.isEmpty()) {
                reality.insert(QStringLiteral("publicKey"), pbk);
            }
            if (!fp.isEmpty()) {
                reality.insert(QStringLiteral("fingerprint"), fp);
            }
            if (!sni.isEmpty()) {
                reality.insert(QStringLiteral("serverName"), sni);
            }
            if (!sid.isEmpty()) {
                reality.insert(QStringLiteral("shortId"), sid);
            }
            if (!spx.isEmpty()) {
                reality.insert(QStringLiteral("spiderX"), spx);
            }
            stream.insert(QStringLiteral("realitySettings"), reality);
        } else if (security.compare(QStringLiteral("tls"), Qt::CaseInsensitive) == 0) {
            QJsonObject tls;
            if (!sni.isEmpty()) {
                tls.insert(QStringLiteral("serverName"), sni);
            }
            if (!fp.isEmpty()) {
                tls.insert(QStringLiteral("fingerprint"), fp);
            }
            if (!alpn.isEmpty()) {
                QJsonArray alpnArr;
                for (const QString& part : alpn.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                    alpnArr.append(part.trimmed());
                }
                tls.insert(QStringLiteral("alpn"), alpnArr);
            }
            stream.insert(QStringLiteral("tlsSettings"), tls);
        }
    }

    if (network.compare(QStringLiteral("ws"), Qt::CaseInsensitive) == 0) {
        QJsonObject ws;
        if (!path.isEmpty()) {
            ws.insert(QStringLiteral("path"), path);
        }
        if (!hostHeader.isEmpty()) {
            QJsonObject headers;
            headers.insert(QStringLiteral("Host"), hostHeader);
            ws.insert(QStringLiteral("headers"), headers);
        }
        stream.insert(QStringLiteral("wsSettings"), ws);
    } else if (network.compare(QStringLiteral("grpc"), Qt::CaseInsensitive) == 0) {
        QJsonObject grpc;
        if (!serviceName.isEmpty()) {
            grpc.insert(QStringLiteral("serviceName"), serviceName);
        }
        if (!mode.isEmpty()) {
            grpc.insert(QStringLiteral("multiMode"), mode.compare(QStringLiteral("multi"), Qt::CaseInsensitive) == 0);
        }
        stream.insert(QStringLiteral("grpcSettings"), grpc);
    } else if (network.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
        || network.compare(QStringLiteral("h2"), Qt::CaseInsensitive) == 0) {
        QJsonObject http;
        if (!path.isEmpty()) {
            http.insert(QStringLiteral("path"), path);
        }
        if (!hostHeader.isEmpty()) {
            http.insert(QStringLiteral("host"), QJsonArray{hostHeader});
        }
        stream.insert(QStringLiteral("httpSettings"), http);
    } else if (network.compare(QStringLiteral("xhttp"), Qt::CaseInsensitive) == 0
        || network.compare(QStringLiteral("splithttp"), Qt::CaseInsensitive) == 0) {
        QJsonObject xhttp;
        if (!path.isEmpty()) {
            xhttp.insert(QStringLiteral("path"), path);
        }
        if (!hostHeader.isEmpty()) {
            xhttp.insert(QStringLiteral("host"), hostHeader);
        }
        if (!mode.isEmpty()) {
            xhttp.insert(QStringLiteral("mode"), mode);
        }
        stream.insert(QStringLiteral("xhttpSettings"), xhttp);
    }

    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("proxy"));
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("vless"));
    outbound.insert(QStringLiteral("settings"), settings);
    outbound.insert(QStringLiteral("streamSettings"), stream);

    const QJsonObject root = wrapClientConfig(outbound, localSocksPort, useTun);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString normalizeImportedJson(const QString& jsonText, quint16 localSocksPort, bool useTun, QString* errorOut)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Invalid Xray JSON: %1").arg(err.errorString());
        }
        return {};
    }
    QJsonObject root = doc.object();
    if (root.contains(QStringLiteral("outbounds"))) {
        root = ensureClientInbounds(root, localSocksPort, useTun);
        return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
    if (root.contains(QStringLiteral("protocol"))
        && root.value(QStringLiteral("protocol")).toString().compare(QStringLiteral("vless"), Qt::CaseInsensitive)
            == 0) {
        if (!root.contains(QStringLiteral("tag"))) {
            root.insert(QStringLiteral("tag"), QStringLiteral("proxy"));
        }
        const QJsonObject wrapped = wrapClientConfig(root, localSocksPort, useTun);
        return QString::fromUtf8(QJsonDocument(wrapped).toJson(QJsonDocument::Indented));
    }
    if (errorOut) {
        *errorOut = Datagate::tr("JSON must be a full Xray config or a VLESS outbound object.");
    }
    return {};
}

QString clientConfigFromOpaqueText(const QString& text, quint16 localSocksPort, bool useTun, QString* errorOut)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Xray config is empty.");
        }
        return {};
    }

    const QString uri = extractVlessUriFromText(trimmed);
    if (!uri.isEmpty()) {
        return clientJsonFromVlessUri(uri, localSocksPort, useTun, errorOut);
    }

    QString jsonErr;
    const QString fromJson = normalizeImportedJson(trimmed, localSocksPort, useTun, &jsonErr);
    if (!fromJson.isEmpty()) {
        return fromJson;
    }

    if (errorOut) {
        *errorOut = Datagate::tr(
            "Could not find a vless:// link or a valid Xray JSON config in the response.");
    }
    return {};
}

} // namespace XrayConfigBuilder
