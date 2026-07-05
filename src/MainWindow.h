#pragma once

#include "DatagateUtils.h"

#include <QMainWindow>

class QEvent;
class QFormLayout;
class QShowEvent;

class AuthSession;
class QCheckBox;
class QComboBox;
class QFrame;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QListWidget;
class QStackedWidget;
class QTableWidget;
class QWidget;
class QNetworkAccessManager;
class VpnSession;
class StatisticsPanel;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AuthSession* session, QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Stop VPN before process exit (quit menu / SIGTERM path via aboutToQuit).
    void shutdownVpn();

signals:
    void logoutRequested();

protected:
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void loadSettings();
    void saveSettings();
    void refreshServers(bool showErrorDialogs = true);
    void applyWssServerList(const QByteArray& statusJsonBody);
    void appendLog(const QString& line);
    void navigateTo(int index);
    void updateConnectButtonUi(bool vpnUp);
    void grantOpenVpnCapNetAdmin();
    void onOpenVpnCapabilityRecommended(const QString& detail);
    void updateServerModeUi();
    void applyVpnAccessInfoToAccessPage();
    void checkFreeTierOnboarding();

    AuthSession* m_session = nullptr;

    QLineEdit* m_openVpnPath = nullptr;
    QPlainTextEdit* m_log = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_connectedToLabel = nullptr;
    QComboBox* m_serverModeCombo = nullptr;
    QComboBox* m_manualServerCombo = nullptr;
    QWidget* m_manualServerWrap = nullptr;
    QPushButton* m_connectBtn = nullptr;
    QPushButton* m_saveSettingsBtn = nullptr;
    QPushButton* m_grantTunCapBtn = nullptr;
    QPushButton* m_refreshServersBtn = nullptr;
    QPushButton* m_logoutBtn = nullptr;
    QTableWidget* m_serverTable = nullptr;
    QFrame* m_accessSubscriptionCard = nullptr;
    QLabel* m_accessSubscriptionHeading = nullptr;
    QLabel* m_accessPlanLabel = nullptr;
    QLabel* m_accessTrafficTitleLabel = nullptr;
    QLabel* m_accessQuotaMetaLabel = nullptr;
    QProgressBar* m_accessQuotaBar = nullptr;
    QLabel* m_accessQuotaStatsLabel = nullptr;
    QLabel* m_accessValidLabel = nullptr;
    DatagateUtils::UserVpnAccessInfo m_lastVpnAccessInfo;
    QLabel* m_accessTotalClientsLabel = nullptr;
    QListWidget* m_nav = nullptr;
    QStackedWidget* m_stack = nullptr;
    QCheckBox* m_themeDark = nullptr;
    QComboBox* m_languageCombo = nullptr;
    StatisticsPanel* m_statisticsPanel = nullptr;

    bool m_centerOnFirstShow = true;
    bool m_didSilentServerRefresh = false;

    VpnSession* m_vpn = nullptr;
    QNetworkAccessManager* m_nam = nullptr;

    QFormLayout* m_settingsVpnForm = nullptr;
    QLabel* m_appTitleLabel = nullptr;
    QLabel* m_welcomeLabel = nullptr;
    QLabel* m_connTitleLabel = nullptr;
    QLabel* m_vpnServerLabel = nullptr;
    QLabel* m_manualServerLabel = nullptr;
    QLabel* m_logSectionTitle = nullptr;
    QLabel* m_accessPageTitle = nullptr;
    QLabel* m_statPageTitle = nullptr;
    QLabel* m_settingsPageTitle = nullptr;
    QLabel* m_langHeading = nullptr;
    QLabel* m_langHint = nullptr;
    QLabel* m_appearHeading = nullptr;
    QLabel* m_appearHint = nullptr;
    QLabel* m_themeLabel = nullptr;
    QLabel* m_downloadHeading = nullptr;
    QLabel* m_downloadHint = nullptr;
    QLabel* m_downloadLinkLabel = nullptr;
    QLabel* m_downloadPlatformWin = nullptr;
    QLabel* m_downloadPlatformLinux = nullptr;
    QLabel* m_downloadPlatformAndroid = nullptr;
    QLabel* m_downloadPlatformIos = nullptr;
    QLabel* m_tunHintLabel = nullptr;
    QLabel* m_accountHeading = nullptr;
    QLabel* m_accountHint = nullptr;
    bool m_vpnTunnelUp = false;
    /// For retranslateUi: last total on Access page, or -1 for "—".
    int m_accessTotalClientsCount = -1;
};
