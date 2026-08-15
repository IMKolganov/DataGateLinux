#pragma once

#include <QString>
#include <QStringList>
#include <optional>

class QNetworkAccessManager;

/// Client for api/xray-client-links (same response shape as OpenVPN file download).
namespace XrayClientLinksApi {

struct IssuedLink {
    QString contentUtf8; // decoded base64 body (often vless://… text)
    QString commonName;
};

/// Create (if needed) then download VLESS client link for the given VPN server id + CN.
/// commonNames: first is used for create; all are tried for download (handles “already exists”
/// under a legacy CN without installId / with installId).
std::optional<IssuedLink> ensureAndDownloadByCn(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    int vpnServerId,
    const QStringList& commonNames,
    const QString& externalId,
    const QString& issuedTo,
    QString* errorOut);

} // namespace XrayClientLinksApi
