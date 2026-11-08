#include "XrayConfigBuilder.h"
#include "DatagateTr.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include <optional>

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
    // Match DataGateAndroid XrayConfigBuilder: system stack + sniffing (no FakeDNS).
    // OS still assigns address/routes (see third_party/xray-core/proxy/tun/README.md).
    QJsonObject settings;
    settings.insert(QStringLiteral("name"), QStringLiteral("datagatex0"));
    settings.insert(QStringLiteral("mtu"), 1500);
    settings.insert(QStringLiteral("stack"), QStringLiteral("system"));
    QJsonObject sniffing;
    sniffing.insert(QStringLiteral("enabled"), true);
    sniffing.insert(QStringLiteral("destOverride"),
        QJsonArray{QStringLiteral("http"), QStringLiteral("tls"), QStringLiteral("quic")});
    QJsonObject inbound;
    inbound.insert(QStringLiteral("tag"), QStringLiteral("tun-in"));
    inbound.insert(QStringLiteral("protocol"), QStringLiteral("tun"));
    inbound.insert(QStringLiteral("port"), 0);
    inbound.insert(QStringLiteral("settings"), settings);
    inbound.insert(QStringLiteral("sniffing"), sniffing);
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

QJsonObject freedomDirectOutbound()
{
    QJsonObject o;
    o.insert(QStringLiteral("tag"), QStringLiteral("direct"));
    o.insert(QStringLiteral("protocol"), QStringLiteral("freedom"));
    o.insert(QStringLiteral("settings"), QJsonObject{});
    return o;
}

QJsonArray privateLanCidrs()
{
    // Same private set as DataGateAndroid XrayConfigBuilder (IPv4). DNS Pi-hole IPs in
    // 172.16.0.0/12 must be routed to proxy *before* this rule (see ensureClientRouting).
    return QJsonArray{
        QStringLiteral("0.0.0.0/8"),
        QStringLiteral("10.0.0.0/8"),
        QStringLiteral("100.64.0.0/10"),
        QStringLiteral("127.0.0.0/8"),
        QStringLiteral("169.254.0.0/16"),
        QStringLiteral("172.16.0.0/12"),
        QStringLiteral("192.0.0.0/24"),
        QStringLiteral("192.0.2.0/24"),
        QStringLiteral("192.168.0.0/16"),
        QStringLiteral("198.18.0.0/15"),
        QStringLiteral("198.51.100.0/24"),
        QStringLiteral("203.0.113.0/24"),
        QStringLiteral("224.0.0.0/4"),
        QStringLiteral("240.0.0.0/4"),
        QStringLiteral("255.255.255.255/32"),
    };
}

QStringList normalizeDnsServerList(const QJsonArray& arr)
{
    QStringList out;
    for (const QJsonValue& v : arr) {
        const QString s = v.toString().trimmed();
        if (s.isEmpty() || out.contains(s, Qt::CaseInsensitive)) {
            continue;
        }
        out.append(s);
    }
    return out;
}

QStringList dnsServersFromObject(const QJsonObject& o)
{
    QJsonValue v = o.value(QStringLiteral("dnsServers"));
    if (!v.isArray()) {
        v = o.value(QStringLiteral("DnsServers"));
    }
    if (!v.isArray()) {
        const QJsonObject meta = o.value(QStringLiteral("datagateClient")).toObject();
        v = meta.value(QStringLiteral("dnsServers"));
        if (!v.isArray()) {
            v = meta.value(QStringLiteral("DnsServers"));
        }
    }
    if (!v.isArray()) {
        return {};
    }
    return normalizeDnsServerList(v.toArray());
}

bool dnsIdentityFromObject(const QJsonObject& o)
{
    auto readBool = [](const QJsonValue& v) -> std::optional<bool> {
        if (v.isBool()) {
            return v.toBool();
        }
        if (v.isString()) {
            const QString s = v.toString().trimmed();
            if (s.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0
                || s == QLatin1String("1") || s.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0
                || s.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0) {
                return true;
            }
            if (s.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0
                || s == QLatin1String("0") || s.compare(QStringLiteral("no"), Qt::CaseInsensitive) == 0
                || s.compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0) {
                return false;
            }
        }
        if (v.isDouble()) {
            return v.toInt() != 0;
        }
        return std::nullopt;
    };

    if (const auto b = readBool(o.value(QStringLiteral("dnsIdentityEnabled")))) {
        return *b;
    }
    if (const auto b = readBool(o.value(QStringLiteral("DnsIdentityEnabled")))) {
        return *b;
    }
    const QJsonObject meta = o.value(QStringLiteral("datagateClient")).toObject();
    if (const auto b = readBool(meta.value(QStringLiteral("dnsIdentityEnabled")))) {
        return *b;
    }
    if (const auto b = readBool(meta.value(QStringLiteral("DnsIdentityEnabled")))) {
        return *b;
    }
    return false;
}

void attachDatagateClientMeta(QJsonObject& root, const QStringList& dnsServers, bool dnsIdentityEnabled)
{
    QJsonObject meta;
    QJsonArray arr;
    for (const QString& s : dnsServers) {
        arr.append(s);
    }
    meta.insert(QStringLiteral("dnsServers"), arr);
    meta.insert(QStringLiteral("dnsIdentityEnabled"), dnsIdentityEnabled);
    root.insert(QStringLiteral("datagateClient"), meta);
}

QJsonObject ensureDirectOutbound(QJsonObject root)
{
    QJsonArray outs = root.value(QStringLiteral("outbounds")).toArray();
    for (const QJsonValue& v : outs) {
        const QString tag = v.toObject().value(QStringLiteral("tag")).toString();
        if (tag.compare(QStringLiteral("direct"), Qt::CaseInsensitive) == 0) {
            return root;
        }
    }
    outs.append(freedomDirectOutbound());
    root.insert(QStringLiteral("outbounds"), outs);
    return root;
}

/// DNS /32 → proxy (before private), private LAN → direct, catch-all → proxy.
QJsonObject ensureClientRouting(QJsonObject root, const QStringList& dnsServers)
{
    root = ensureDirectOutbound(std::move(root));

    QJsonArray rules;
    // One rule for all tunnel DNS IPs (same as Android appendProxyDnsRules).
    QJsonArray dnsIps;
    for (const QString& dns : dnsServers) {
        const QString ip = dns.trimmed();
        if (ip.isEmpty()) {
            continue;
        }
        dnsIps.append(ip.contains(QLatin1Char('/')) ? ip : (ip + QStringLiteral("/32")));
    }
    if (!dnsIps.isEmpty()) {
        QJsonObject rule;
        rule.insert(QStringLiteral("type"), QStringLiteral("field"));
        rule.insert(QStringLiteral("outboundTag"), QStringLiteral("proxy"));
        rule.insert(QStringLiteral("ip"), dnsIps);
        rules.append(rule);
    }

    QJsonObject privateRule;
    privateRule.insert(QStringLiteral("type"), QStringLiteral("field"));
    privateRule.insert(QStringLiteral("outboundTag"), QStringLiteral("direct"));
    privateRule.insert(QStringLiteral("ip"), privateLanCidrs());
    rules.append(privateRule);

    QJsonObject catchAll;
    catchAll.insert(QStringLiteral("type"), QStringLiteral("field"));
    catchAll.insert(QStringLiteral("outboundTag"), QStringLiteral("proxy"));
    catchAll.insert(QStringLiteral("network"), QStringLiteral("tcp,udp"));
    rules.append(catchAll);

    QJsonObject routing = root.value(QStringLiteral("routing")).toObject();
    if (routing.isEmpty()) {
        routing.insert(QStringLiteral("domainStrategy"), QStringLiteral("AsIs"));
    }
    routing.insert(QStringLiteral("rules"), rules);
    root.insert(QStringLiteral("routing"), routing);
    return root;
}

QJsonObject wrapClientConfig(const QJsonObject& outbound, quint16 socksPort, bool useTun,
    const QStringList& dnsServers, bool dnsIdentityEnabled)
{
    QJsonObject log;
    // info: startup / failures show in UI journal; per-flow "accepted" lines are filtered out of UI.
    log.insert(QStringLiteral("loglevel"), QStringLiteral("info"));

    QJsonObject root;
    root.insert(QStringLiteral("log"), log);
    root.insert(QStringLiteral("inbounds"), managedClientInbounds(socksPort, useTun));
    root.insert(QStringLiteral("outbounds"), QJsonArray{outbound});
    root = ensureClientRouting(root, dnsServers);
    attachDatagateClientMeta(root, dnsServers, dnsIdentityEnabled);
    return root;
}

QString queryValue(const QUrlQuery& q, const QString& key)
{
    return q.queryItemValue(key, QUrl::FullyDecoded).trimmed();
}

QString profileVlessField(const QJsonObject& o)
{
    QString v = o.value(QStringLiteral("vless")).toString().trimmed();
    if (v.isEmpty()) {
        v = o.value(QStringLiteral("Vless")).toString().trimmed();
    }
    return v;
}

bool looksLikeDashboardProfile(const QJsonObject& o)
{
    if (o.contains(QStringLiteral("outbounds"))) {
        return false;
    }
    const QString proto = o.value(QStringLiteral("protocol")).toString();
    if (!proto.isEmpty()) {
        return false;
    }
    return !profileVlessField(o).isEmpty();
}

} // namespace

QStringList extractDnsServersFromProfileText(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (!trimmed.startsWith(QLatin1Char('{'))) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8());
    if (!doc.isObject()) {
        return {};
    }
    return dnsServersFromObject(doc.object());
}

bool extractDnsIdentityEnabled(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (!trimmed.startsWith(QLatin1Char('{'))) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8());
    if (!doc.isObject()) {
        return false;
    }
    return dnsIdentityFromObject(doc.object());
}

QStringList dnsServersFromClientJson(const QString& clientJson)
{
    const QJsonDocument doc = QJsonDocument::fromJson(clientJson.toUtf8());
    if (!doc.isObject()) {
        return {};
    }
    return dnsServersFromObject(doc.object());
}

bool dnsIdentityEnabledFromClientJson(const QString& clientJson)
{
    const QJsonDocument doc = QJsonDocument::fromJson(clientJson.toUtf8());
    if (!doc.isObject()) {
        return false;
    }
    return dnsIdentityFromObject(doc.object());
}

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

QString clientJsonFromVlessUri(const QString& uriIn, quint16 localSocksPort, bool useTun, QString* errorOut,
    const QStringList& dnsServers, bool dnsIdentityEnabled)
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

    const QJsonObject root = wrapClientConfig(outbound, localSocksPort, useTun, dnsServers, dnsIdentityEnabled);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString normalizeImportedJson(const QString& jsonText, quint16 localSocksPort, bool useTun, QString* errorOut,
    const QStringList& dnsServers, bool dnsIdentityEnabled)
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
    QStringList dns = dnsServers;
    bool identity = dnsIdentityEnabled;
    if (dns.isEmpty()) {
        dns = dnsServersFromObject(root);
    }
    if (!identity) {
        identity = dnsIdentityFromObject(root);
    }

    if (root.contains(QStringLiteral("outbounds"))) {
        root = ensureClientInbounds(root, localSocksPort, useTun);
        root = ensureClientRouting(root, dns);
        attachDatagateClientMeta(root, dns, identity);
        return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
    if (root.contains(QStringLiteral("protocol"))
        && root.value(QStringLiteral("protocol")).toString().compare(QStringLiteral("vless"), Qt::CaseInsensitive)
            == 0) {
        if (!root.contains(QStringLiteral("tag"))) {
            root.insert(QStringLiteral("tag"), QStringLiteral("proxy"));
        }
        const QJsonObject wrapped = wrapClientConfig(root, localSocksPort, useTun, dns, identity);
        return QString::fromUtf8(QJsonDocument(wrapped).toJson(QJsonDocument::Indented));
    }
    if (errorOut) {
        *errorOut = Datagate::tr("JSON must be a full Xray config or a VLESS outbound object.");
    }
    return {};
}

BuiltClientConfig buildClientConfigFromOpaqueText(const QString& text, quint16 localSocksPort, bool useTun,
    QString* errorOut)
{
    BuiltClientConfig result;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Xray config is empty.");
        }
        return result;
    }

    // Dashboard profile: {"vless":"…","dnsServers":[…],…} — parse before URI scrape so dnsServers survive.
    if (trimmed.startsWith(QLatin1Char('{'))) {
        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &perr);
        if (perr.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject obj = doc.object();
            result.dnsServers = dnsServersFromObject(obj);
            result.dnsIdentityEnabled = dnsIdentityFromObject(obj);

            if (looksLikeDashboardProfile(obj)) {
                QString buildErr;
                result.json = clientJsonFromVlessUri(profileVlessField(obj), localSocksPort, useTun, &buildErr,
                    result.dnsServers, result.dnsIdentityEnabled);
                if (result.json.isEmpty()) {
                    if (errorOut) {
                        *errorOut = buildErr;
                    }
                    return {};
                }
                return result;
            }

            QString jsonErr;
            result.json = normalizeImportedJson(trimmed, localSocksPort, useTun, &jsonErr, result.dnsServers,
                result.dnsIdentityEnabled);
            if (!result.json.isEmpty()) {
                // Re-read meta after normalize (embedded).
                result.dnsServers = dnsServersFromClientJson(result.json);
                result.dnsIdentityEnabled = dnsIdentityEnabledFromClientJson(result.json);
                return result;
            }
            // Fall through to vless:// scrape / error below if not a recognized JSON shape.
            if (errorOut) {
                *errorOut = jsonErr;
            }
        }
    }

    const QString uri = extractVlessUriFromText(trimmed);
    if (!uri.isEmpty()) {
        QString buildErr;
        result.json = clientJsonFromVlessUri(uri, localSocksPort, useTun, &buildErr, result.dnsServers,
            result.dnsIdentityEnabled);
        if (result.json.isEmpty()) {
            if (errorOut) {
                *errorOut = buildErr;
            }
            return {};
        }
        if (errorOut) {
            errorOut->clear();
        }
        return result;
    }

    if (errorOut && errorOut->isEmpty()) {
        *errorOut = Datagate::tr(
            "Could not find a vless:// link or a valid Xray JSON config in the response.");
    }
    return {};
}

QString clientConfigFromOpaqueText(const QString& text, quint16 localSocksPort, bool useTun, QString* errorOut)
{
    return buildClientConfigFromOpaqueText(text, localSocksPort, useTun, errorOut).json;
}

} // namespace XrayConfigBuilder
