#pragma once

#include <QString>

/// Load appsettings.json next to the binary (same idea as DataGateWin).
struct AppConfig {
    static bool load();

    static QString apiBaseUrl();
    static QString googleClientId();
    static int googleRedirectPort();

    /// When true, patched .ovpn adds pull-filter to ignore server push redirect-gateway (avoids full-tunnel lockout if VPN breaks).
    static bool openVpnIgnoreRedirectGateway();

private:
    static QString s_apiBaseUrl;
    static QString s_googleClientId;
    static int s_googleRedirectPort;
    static bool s_openVpnIgnoreRedirectGateway;
};
