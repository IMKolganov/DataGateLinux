#pragma once

#include <QtGlobal>

#include <QString>

namespace XrayConfigBuilder {

/// Pull the first vless://… token from arbitrary text (API templates, multi-line bodies).
QString extractVlessUriFromText(const QString& text);

/// Convert vless://… into a client JSON (optional TUN + SOCKS inbounds + VLESS outbound).
QString clientJsonFromVlessUri(const QString& uri, quint16 localSocksPort, bool useTun, QString* errorOut);

/// Full Xray config or bare VLESS outbound → normalized client JSON.
QString normalizeImportedJson(const QString& jsonText, quint16 localSocksPort, bool useTun, QString* errorOut);

/// Opaque API/import body: extract vless://, else JSON. Clear error if neither works.
QString clientConfigFromOpaqueText(const QString& text, quint16 localSocksPort, bool useTun, QString* errorOut);

constexpr quint16 kDefaultSocksPort = 10808;

} // namespace XrayConfigBuilder
