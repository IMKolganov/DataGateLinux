#pragma once

#include <QtGlobal>

#include <QString>
#include <QStringList>

namespace XrayConfigBuilder {

/// Result of building a client config from opaque API/import text (vless://, dashboard
/// profile JSON, or full Xray JSON). dnsServers come only from the profile — never hardcoded.
struct BuiltClientConfig {
    QString json;
    QStringList dnsServers;
    bool dnsIdentityEnabled = false;
};

/// Pull the first vless://… token from arbitrary text (API templates, multi-line bodies).
QString extractVlessUriFromText(const QString& text);

/// dnsServers / DnsServers from a dashboard profile JSON object (text must start with `{`).
QStringList extractDnsServersFromProfileText(const QString& text);
bool extractDnsIdentityEnabled(const QString& text);

/// Read dnsServers embedded in a saved client JSON (`datagateClient` meta or root).
QStringList dnsServersFromClientJson(const QString& clientJson);
bool dnsIdentityEnabledFromClientJson(const QString& clientJson);

/// Convert vless://… into a client JSON (optional TUN + SOCKS inbounds + VLESS outbound).
QString clientJsonFromVlessUri(const QString& uri, quint16 localSocksPort, bool useTun, QString* errorOut,
    const QStringList& dnsServers = {}, bool dnsIdentityEnabled = false);

/// Full Xray config or bare VLESS outbound → normalized client JSON.
QString normalizeImportedJson(const QString& jsonText, quint16 localSocksPort, bool useTun, QString* errorOut,
    const QStringList& dnsServers = {}, bool dnsIdentityEnabled = false);

/// Opaque API/import body: dashboard `{vless,dnsServers,…}`, vless://, or Xray JSON.
BuiltClientConfig buildClientConfigFromOpaqueText(const QString& text, quint16 localSocksPort, bool useTun,
    QString* errorOut);

/// Same as buildClientConfigFromOpaqueText but returns only the JSON (meta embedded).
QString clientConfigFromOpaqueText(const QString& text, quint16 localSocksPort, bool useTun, QString* errorOut);

constexpr quint16 kDefaultSocksPort = 10808;

} // namespace XrayConfigBuilder
