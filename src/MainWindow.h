#pragma once

#include <QMainWindow>

class QShowEvent;

class AuthSession;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class QListWidget;
class QStackedWidget;
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

private:
    void loadSettings();
    void saveSettings();
    void refreshServers(bool showErrorDialogs = true);
    void applyWssServerList(const QByteArray& statusJsonBody);
    void appendLog(const QString& line);
    void navigateTo(int index);
    void updateConnectButtonUi(bool vpnUp);
    void grantOpenVpnCapNetAdmin();
    void onOpenVpnCapabilityRecommended(const QString& detail);

    AuthSession* m_session = nullptr;

    QLineEdit* m_openVpnPath = nullptr;
    QPlainTextEdit* m_log = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_connectedToLabel = nullptr;
    QComboBox* m_serverModeCombo = nullptr;
    QComboBox* m_manualServerCombo = nullptr;
    QPushButton* m_connectBtn = nullptr;
    QPushButton* m_saveSettingsBtn = nullptr;
    QPushButton* m_grantTunCapBtn = nullptr;
    QPushButton* m_refreshServersBtn = nullptr;
    QPushButton* m_logoutBtn = nullptr;
    QListWidget* m_serverList = nullptr;
    QListWidget* m_nav = nullptr;
    QStackedWidget* m_stack = nullptr;
    QCheckBox* m_themeDark = nullptr;
    StatisticsPanel* m_statisticsPanel = nullptr;

    bool m_centerOnFirstShow = true;
    bool m_didSilentServerRefresh = false;

    VpnSession* m_vpn = nullptr;
    QNetworkAccessManager* m_nam = nullptr;
};
