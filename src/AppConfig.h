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

    /// When true (default), UDP↔WSS uses uint16-BE length + payload (DataGateWin-style). Set false if proxy sends raw OpenVPN UDP per WS frame.
    static bool udpBridgeFramed();

    /// If embedded OpenVPN 3 is built in: false = embedded core (default), true = external `openvpn` process.
    static bool openVpnUseSystemBinary();

    /// GitHub `owner/repo` for GET …/releases/latest (optional; defaults match DataGateWin-style check).
    static QString updateGithubOwner();
    static QString updateGithubRepo();
    /// When false, skip startup release check even if owner/repo are set.
    static bool updateCheckOnStartup();

private:
    static QString s_apiBaseUrl;
    static QString s_googleClientId;
    static int s_googleRedirectPort;
    static bool s_openVpnIgnoreRedirectGateway;
    static bool s_udpBridgeFramed;
    static bool s_openVpnUseSystemBinary;
    static QString s_updateGithubOwner;
    static QString s_updateGithubRepo;
    static bool s_updateCheckOnStartup;
};
