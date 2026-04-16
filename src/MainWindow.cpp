#include "MainWindow.h"

#include "AppConfig.h"
#include "AppLogging.h"
#include "AppTheme.h"
#include "AuthSession.h"
#include "DatagateTr.h"
#include "DatagateTranslator.h"
#include "DatagateUtils.h"
#include "StatisticsPanel.h"
#include "VpnSession.h"

#include <QFileInfo>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QListWidget>
#include <QMessageBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QShowEvent>

namespace {

QIcon iconFromThemeOr(const QStringList& names, QStyle* st, QStyle::StandardPixmap sp)
{
    for (const QString& n : names) {
        const QIcon ico = QIcon::fromTheme(n);
        if (!ico.isNull()) {
            return ico;
        }
    }
    return st->standardIcon(sp);
}

struct PlatformCol {
    QWidget* wrap = nullptr;
    QLabel* caption = nullptr;
};

PlatformCol makePlatformColumn(QWidget* parent, QStyle* style, const QStringList& iconNames,
    QStyle::StandardPixmap fallback, const QString& caption)
{
    PlatformCol out;
    out.wrap = new QWidget(parent);
    auto* vl = new QVBoxLayout(out.wrap);
    vl->setContentsMargins(6, 0, 6, 0);
    vl->setSpacing(6);
    auto* ic = new QLabel(parent);
    const QIcon ico = iconFromThemeOr(iconNames, style, fallback);
    const QPixmap pm = ico.pixmap(32, 32);
    if (!pm.isNull()) {
        ic->setPixmap(pm);
    } else {
        ic->setText(QStringLiteral("·"));
    }
    ic->setAlignment(Qt::AlignCenter);
    out.caption = new QLabel(caption, parent);
    out.caption->setObjectName(QStringLiteral("muted"));
    out.caption->setAlignment(Qt::AlignCenter);
    vl->addWidget(ic);
    vl->addWidget(out.caption);
    return out;
}

} // namespace

MainWindow::MainWindow(AuthSession* session, QWidget* parent)
    : QMainWindow(parent)
    , m_session(session)
    , m_nam(new QNetworkAccessManager(this))
    , m_vpn(new VpnSession(this))
{
    setWindowTitle(Datagate::tr("DataGate OpenVPN 3"));
    resize(1000, 720);
    setMinimumSize(950, 900);

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralRoot"));
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(16, 12, 16, 8);
    auto* appTitle = new QLabel(Datagate::tr("DataGate OpenVPN 3"), this);
    m_appTitleLabel = appTitle;
    QFont tf = appTitle->font();
    tf.setPointSize(11);
    tf.setWeight(QFont::DemiBold);
    appTitle->setFont(tf);
    titleRow->addWidget(appTitle);
    titleRow->addStretch();
    root->addLayout(titleRow);

    auto* body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    m_nav = new QListWidget(this);
    m_nav->setObjectName(QStringLiteral("navPane"));
    m_nav->setFixedWidth(280);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QStyle* st = style();
    m_nav->addItem(new QListWidgetItem(
        iconFromThemeOr({QStringLiteral("folder-home"), QStringLiteral("go-home"), QStringLiteral("user-home")},
            st, QStyle::SP_DirHomeIcon),
        Datagate::tr("Home")));
    m_nav->addItem(new QListWidgetItem(
        iconFromThemeOr({QStringLiteral("network-server"), QStringLiteral("network-wired"),
                            QStringLiteral("applications-internet")},
            st, QStyle::SP_DriveNetIcon),
        Datagate::tr("Access")));
    m_nav->addItem(new QListWidgetItem(
        iconFromThemeOr({QStringLiteral("office-chart-line"), QStringLiteral("view-statistics"),
                            QStringLiteral("insert-chart"), QStringLiteral("x-office-chart")},
            st, QStyle::SP_FileDialogContentsView),
        Datagate::tr("Statistics")));
    m_nav->addItem(new QListWidgetItem(
        iconFromThemeOr({QStringLiteral("preferences-system"), QStringLiteral("applications-system"),
                            QStringLiteral("system-settings"), QStringLiteral("emblem-system")},
            st, QStyle::SP_FileDialogDetailedView),
        Datagate::tr("Settings")));

    m_stack = new QStackedWidget(this);
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // ----- Page: Home
    auto* homePage = new QWidget(this);
    auto* homeOuter = new QVBoxLayout(homePage);
    homeOuter->setContentsMargins(20, 20, 20, 20);
    homeOuter->setSpacing(16);

    auto* welcome = new QLabel(Datagate::tr("Welcome to DataGate OpenVPN 3"), this);
    welcome->setObjectName(QStringLiteral("titleH1"));
    m_welcomeLabel = welcome;
    homeOuter->addWidget(welcome);

    auto* connCard = new QFrame(this);
    connCard->setObjectName(QStringLiteral("card"));
    auto* connLay = new QVBoxLayout(connCard);
    connLay->setContentsMargins(18, 18, 18, 18);
    auto* connTitle = new QLabel(Datagate::tr("Connection status"), this);
    connTitle->setObjectName(QStringLiteral("titleH2"));
    m_connTitleLabel = connTitle;
    connLay->addWidget(connTitle);
    m_status = new QLabel(Datagate::tr("Idle"), this);
    m_status->setObjectName(QStringLiteral("muted"));
    m_status->setWordWrap(true);
    connLay->addWidget(m_status);
    m_connectedToLabel = new QLabel(Datagate::tr("Connected to: —"), this);
    m_connectedToLabel->setObjectName(QStringLiteral("muted"));
    m_connectedToLabel->setWordWrap(true);
    connLay->addWidget(m_connectedToLabel);

    auto* modeRow = new QHBoxLayout();
    auto* vpnServerLbl = new QLabel(Datagate::tr("VPN server:"), this);
    m_vpnServerLabel = vpnServerLbl;
    modeRow->addWidget(vpnServerLbl);
    m_serverModeCombo = new QComboBox(this);
    m_serverModeCombo->addItem(Datagate::tr("Automatic (best server)"), 0);
    m_serverModeCombo->addItem(Datagate::tr("Choose server…"), 1);
    modeRow->addWidget(m_serverModeCombo, 1);
    connLay->addLayout(modeRow);

    m_manualServerWrap = new QWidget(this);
    auto* manRow = new QHBoxLayout(m_manualServerWrap);
    manRow->setContentsMargins(0, 0, 0, 0);
    auto* manualServerLbl = new QLabel(Datagate::tr("Server:"), this);
    m_manualServerLabel = manualServerLbl;
    manRow->addWidget(manualServerLbl);
    m_manualServerCombo = new QComboBox(this);
    m_manualServerCombo->setMinimumWidth(200);
    manRow->addWidget(m_manualServerCombo, 1);
    connLay->addWidget(m_manualServerWrap);

    auto* btnRow = new QHBoxLayout();
    m_connectBtn = new QPushButton(Datagate::tr("Connect"), this);
    m_connectBtn->setProperty("primary", true);
    btnRow->addWidget(m_connectBtn);
    btnRow->addStretch();
    connLay->addLayout(btnRow);
    homeOuter->addWidget(connCard);

    auto* logCard = new QFrame(this);
    logCard->setObjectName(QStringLiteral("card"));
    auto* logLay = new QVBoxLayout(logCard);
    logLay->setContentsMargins(18, 18, 18, 18);
    auto* logTitle = new QLabel(Datagate::tr("Engine logs"), this);
    logTitle->setObjectName(QStringLiteral("titleH2"));
    m_logSectionTitle = logTitle;
    logLay->addWidget(logTitle);
    m_log = new QPlainTextEdit(this);
    m_log->setObjectName(QStringLiteral("engineLog"));
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    logLay->addWidget(m_log, 1);
    homeOuter->addWidget(logCard, 1);

    m_stack->addWidget(homePage);

    // ----- Page: Access
    auto* accessPage = new QWidget(this);
    accessPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* accLay = new QVBoxLayout(accessPage);
    accLay->setContentsMargins(20, 20, 20, 20);
    accLay->setSpacing(12);
    auto* accTitle = new QLabel(Datagate::tr("Access"), this);
    accTitle->setObjectName(QStringLiteral("titleH1"));
    m_accessPageTitle = accTitle;
    accLay->addWidget(accTitle);
    auto* accRow = new QHBoxLayout();
    m_refreshServersBtn = new QPushButton(Datagate::tr("Refresh server list"), this);
    m_refreshServersBtn->setProperty("primary", true);
    accRow->addWidget(m_refreshServersBtn);
    accRow->addStretch();
    accLay->addLayout(accRow);
    m_serverTable = new QTableWidget(this);
    m_serverTable->setObjectName(QStringLiteral("accessServerTable"));
    m_serverTable->setColumnCount(3);
    m_serverTable->setHorizontalHeaderLabels({
        Datagate::tr("Server"),
        Datagate::tr("Online"),
        Datagate::tr("Clients"),
    });
    m_serverTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_serverTable->setMinimumHeight(200);
    m_serverTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_serverTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_serverTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_serverTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_serverTable->setAlternatingRowColors(true);
    m_serverTable->verticalHeader()->setVisible(false);
    m_serverTable->setShowGrid(true);
    accLay->addWidget(m_serverTable, 1);
    m_accessTotalClientsLabel = new QLabel(Datagate::tr("Total clients: —"), this);
    m_accessTotalClientsLabel->setObjectName(QStringLiteral("muted"));
    accLay->addWidget(m_accessTotalClientsLabel);
    m_stack->addWidget(accessPage);

    // ----- Page: Statistics
    auto* statPage = new QWidget(this);
    auto* statLay = new QVBoxLayout(statPage);
    statLay->setContentsMargins(20, 20, 20, 20);
    statLay->setSpacing(12);
    auto* statTitle = new QLabel(Datagate::tr("Statistics"), this);
    statTitle->setObjectName(QStringLiteral("titleH1"));
    m_statPageTitle = statTitle;
    statLay->addWidget(statTitle);
    m_statisticsPanel = new StatisticsPanel(this);
    statLay->addWidget(m_statisticsPanel, 1);
    m_stack->addWidget(statPage);

    // ----- Page: Settings
    auto* settingsPage = new QWidget(this);
    auto* setOuter = new QVBoxLayout(settingsPage);
    setOuter->setContentsMargins(20, 20, 20, 20);
    setOuter->setSpacing(16);
    auto* setTitle = new QLabel(Datagate::tr("Settings"), this);
    setTitle->setObjectName(QStringLiteral("titleH1"));
    m_settingsPageTitle = setTitle;
    setOuter->addWidget(setTitle);

    auto* langCard = new QFrame(this);
    langCard->setObjectName(QStringLiteral("card"));
    auto* langLay = new QVBoxLayout(langCard);
    langLay->setContentsMargins(18, 18, 18, 18);
    auto* langH = new QLabel(Datagate::tr("Language"), this);
    langH->setObjectName(QStringLiteral("titleH2"));
    m_langHeading = langH;
    langLay->addWidget(langH);
    auto* langSub = new QLabel(
        Datagate::tr(
            "Choosing a language updates the interface immediately; it is saved automatically. "
            "Use Save settings for VPN path, theme, and server options."),
        this);
    langSub->setObjectName(QStringLiteral("muted"));
    m_langHint = langSub;
    langSub->setWordWrap(true);
    langLay->addWidget(langSub);
    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_languageCombo->addItem(QStringLiteral("Русский"), QStringLiteral("ru"));
    m_languageCombo->addItem(QStringLiteral("Français"), QStringLiteral("fr"));
    m_languageCombo->addItem(QStringLiteral("Ελληνικά"), QStringLiteral("el"));
    langLay->addWidget(m_languageCombo);
    setOuter->addWidget(langCard);

    auto* appearCard = new QFrame(this);
    appearCard->setObjectName(QStringLiteral("card"));
    auto* appearLay = new QVBoxLayout(appearCard);
    appearLay->setContentsMargins(18, 18, 18, 18);
    auto* appearH = new QLabel(Datagate::tr("Appearance"), this);
    appearH->setObjectName(QStringLiteral("titleH2"));
    m_appearHeading = appearH;
    appearLay->addWidget(appearH);
    auto* appearSub = new QLabel(Datagate::tr("Choose the application theme."), this);
    appearSub->setObjectName(QStringLiteral("muted"));
    m_appearHint = appearSub;
    appearLay->addWidget(appearSub);
    auto* themeRow = new QHBoxLayout();
    auto* themeLbl = new QLabel(Datagate::tr("Theme"), this);
    m_themeLabel = themeLbl;
    m_themeDark = new QCheckBox(Datagate::tr("Dark mode"), this);
    m_themeDark->setChecked(true);
    themeRow->addWidget(themeLbl);
    themeRow->addStretch();
    themeRow->addWidget(m_themeDark);
    appearLay->addLayout(themeRow);
    setOuter->addWidget(appearCard);

    auto* downloadCard = new QFrame(this);
    downloadCard->setObjectName(QStringLiteral("card"));
    auto* downloadLay = new QVBoxLayout(downloadCard);
    downloadLay->setContentsMargins(18, 18, 18, 18);
    downloadLay->setSpacing(12);
    auto* downloadH = new QLabel(Datagate::tr("More apps"), this);
    downloadH->setObjectName(QStringLiteral("titleH2"));
    m_downloadHeading = downloadH;
    downloadLay->addWidget(downloadH);
    auto* downloadSub = new QLabel(
        Datagate::tr("You can download other DataGate apps for your devices from the website:"),
        this);
    downloadSub->setObjectName(QStringLiteral("muted"));
    m_downloadHint = downloadSub;
    downloadSub->setWordWrap(true);
    downloadLay->addWidget(downloadSub);
    auto* platformsRow = new QHBoxLayout();
    platformsRow->setSpacing(8);
    const PlatformCol colWin = makePlatformColumn(this, st,
        {QStringLiteral("computer"), QStringLiteral("user-desktop"), QStringLiteral("start-here")},
        QStyle::SP_FileDialogListView, Datagate::tr("Windows"));
    platformsRow->addWidget(colWin.wrap);
    m_downloadPlatformWin = colWin.caption;
    const PlatformCol colLnx = makePlatformColumn(this, st,
        {QStringLiteral("computer"), QStringLiteral("distributor-logo"), QStringLiteral("start-here")},
        QStyle::SP_ComputerIcon, Datagate::tr("Linux"));
    platformsRow->addWidget(colLnx.wrap);
    m_downloadPlatformLinux = colLnx.caption;
    const PlatformCol colAnd = makePlatformColumn(this, st,
        {QStringLiteral("phone"), QStringLiteral("smartphone"), QStringLiteral("multimedia-player")},
        QStyle::SP_FileDialogContentsView, Datagate::tr("Android"));
    platformsRow->addWidget(colAnd.wrap);
    m_downloadPlatformAndroid = colAnd.caption;
    const PlatformCol colIos = makePlatformColumn(this, st,
        {QStringLiteral("phone"), QStringLiteral("apple"), QStringLiteral("input-dialpad")},
        QStyle::SP_MessageBoxInformation, Datagate::tr("iOS"));
    platformsRow->addWidget(colIos.wrap);
    m_downloadPlatformIos = colIos.caption;
    platformsRow->addStretch();
    downloadLay->addLayout(platformsRow);
    auto* downloadLink = new QLabel(
        QStringLiteral("<a href=\"https://datagateapp.com/download\">https://datagateapp.com/download</a>"),
        this);
    downloadLink->setOpenExternalLinks(true);
    downloadLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    downloadLink->setWordWrap(true);
    downloadLay->addWidget(downloadLink);
    m_downloadLinkLabel = downloadLink;
    setOuter->addWidget(downloadCard);

    auto* vpnCard = new QFrame(this);
    vpnCard->setObjectName(QStringLiteral("card"));
    auto* vpnLay = new QFormLayout(vpnCard);
    vpnLay->setContentsMargins(18, 18, 18, 18);
    vpnLay->setSpacing(12);
    m_openVpnPath = new QLineEdit(this);
    m_openVpnPath->setPlaceholderText(Datagate::tr("openvpn"));
    vpnLay->addRow(Datagate::tr("OpenVPN command"), m_openVpnPath);
    auto* tunHint = new QLabel(
        Datagate::tr(
            "Tunnel (TUN): Linux allows TUN only with CAP_NET_ADMIN on the openvpn binary. "
            "Use Grant TUN capability or: sudo setcap cap_net_admin+ep $(command -v openvpn). "
            "Avoid running the whole app as sudo if possible."),
        this);
    tunHint->setObjectName(QStringLiteral("muted"));
    tunHint->setWordWrap(true);
    m_tunHintLabel = tunHint;
    vpnLay->addRow(tunHint);
    m_grantTunCapBtn = new QPushButton(Datagate::tr("Grant TUN capability…"), this);
    m_grantTunCapBtn->setProperty("secondary", true);
    vpnLay->addRow(m_grantTunCapBtn);
    m_saveSettingsBtn = new QPushButton(Datagate::tr("Save settings"), this);
    m_saveSettingsBtn->setProperty("secondary", true);
    vpnLay->addRow(m_saveSettingsBtn);
    m_settingsVpnForm = vpnLay;
    setOuter->addWidget(vpnCard);

    auto* accountCard = new QFrame(this);
    accountCard->setObjectName(QStringLiteral("card"));
    auto* accCardLay = new QVBoxLayout(accountCard);
    accCardLay->setContentsMargins(18, 18, 18, 18);
    auto* accH = new QLabel(Datagate::tr("Account"), this);
    accH->setObjectName(QStringLiteral("titleH2"));
    m_accountHeading = accH;
    accCardLay->addWidget(accH);
    auto* accSub = new QLabel(Datagate::tr("Sign out from the application."), this);
    accSub->setObjectName(QStringLiteral("muted"));
    m_accountHint = accSub;
    accCardLay->addWidget(accSub);
    m_logoutBtn = new QPushButton(Datagate::tr("Logout"), this);
    m_logoutBtn->setProperty("secondary", true);
    accCardLay->addWidget(m_logoutBtn);
    setOuter->addWidget(accountCard);

    setOuter->addStretch();
    m_stack->addWidget(settingsPage);

    body->addWidget(m_nav);
    body->addWidget(m_stack, 1);
    root->addLayout(body, 1);

    connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_stack->count()) {
            m_stack->setCurrentIndex(row);
        }
        if (row == 1 || row == 2) {
            refreshServers(true);
        }
    });
    m_nav->setCurrentRow(0);

    connect(m_serverModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        updateServerModeUi();
    });

    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString code = m_languageCombo->currentData().toString();
        DatagateTranslator::setLanguageCode(code);
        DatagateTranslator::installForLanguage(code);
        retranslateUi();
        if (m_statisticsPanel) {
            m_statisticsPanel->retranslateUi();
        }
    });

    connect(m_themeDark, &QCheckBox::toggled, this, [this](bool dark) {
        AppTheme::apply(dark);
        QSettings s;
        s.setValue(QStringLiteral("themeDark"), dark);
    });

    connect(m_saveSettingsBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        appendLog(Datagate::tr("Settings saved."));
    });
    connect(m_grantTunCapBtn, &QPushButton::clicked, this, &MainWindow::grantOpenVpnCapNetAdmin);
    connect(m_connectBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        if (!m_connectBtn->isEnabled()) {
            return;
        }
        const QString btnText = m_connectBtn->text();
        if (btnText == Datagate::tr("Connecting…") || btnText == Datagate::tr("Disconnecting…")) {
            return;
        }
        if (m_vpn && btnText == Datagate::tr("Disconnect")) {
            m_connectBtn->setEnabled(false);
            m_connectBtn->setText(Datagate::tr("Disconnecting…"));
            appendLog(Datagate::tr("Disconnecting…"));
            m_vpn->disconnectVpn("Disconnect button");
            return;
        }
        const QString url = AppConfig::apiBaseUrl().trimmed();
        if (url.isEmpty()) {
            QMessageBox::warning(this, Datagate::tr("DataGate"), Datagate::tr("Api:BaseUrl is missing in appsettings.json."));
            return;
        }
        qCInfo(lcUi, "Connect: ensureValidAccessToken");
        if (!m_session->ensureValidAccessToken()) {
            qCWarning(lcUi, "Connect: ensureValidAccessToken failed");
            QMessageBox::warning(
                this,
                Datagate::tr("DataGate"),
                Datagate::tr("Could not obtain an access token. The API may be unavailable — try again later, "
                             "or the session expired — sign out (Settings) and sign in again."));
            return;
        }
        const QString tok = m_session->accessToken();
        const bool autoPick = m_serverModeCombo->currentData().toInt() == 0;
        int manualId = 0;
        if (!autoPick) {
            manualId = m_manualServerCombo->currentData().toInt();
            if (manualId <= 0) {
                QMessageBox::warning(
                    this, Datagate::tr("DataGate"), Datagate::tr("Choose a VPN server or refresh the list."));
                return;
            }
        }
        m_connectBtn->setEnabled(false);
        m_connectBtn->setText(Datagate::tr("Connecting…"));
        m_vpn->connectVpn(url, tok, m_openVpnPath->text(), autoPick, manualId);
    });
    connect(m_refreshServersBtn, &QPushButton::clicked, this, [this]() { refreshServers(true); });

    connect(m_logoutBtn, &QPushButton::clicked, this, [this]() {
        emit logoutRequested();
    });

    connect(m_vpn, &VpnSession::statusMessage, this, [this](const QString& t) {
        m_status->setText(t);
        appendLog(t);
    });
    connect(m_vpn, &VpnSession::errorMessage, this, [this](const QString& t) {
        m_status->setText(t);
        appendLog(Datagate::tr("Error: ") + t);
        QMessageBox::warning(this, Datagate::tr("DataGate"), t);
        updateConnectButtonUi(false);
    });
    connect(m_vpn, &VpnSession::openVpnCapabilitySetupRecommended, this,
        &MainWindow::onOpenVpnCapabilityRecommended);
    connect(m_vpn, &VpnSession::vpnUp, this, [this]() {
        updateConnectButtonUi(true);
        if (m_connectedToLabel) {
            const QString n = m_vpn->activeServerName();
            m_connectedToLabel->setText(Datagate::tr("Connected to: %1").arg(n.isEmpty() ? Datagate::tr("—") : n));
        }
    });
    connect(m_vpn, &VpnSession::vpnDown, this, [this]() {
        updateConnectButtonUi(false);
        if (m_connectedToLabel) {
            m_connectedToLabel->setText(Datagate::tr("Connected to: —"));
        }
    });

    loadSettings();
    updateServerModeUi();

    const QString who = m_session->displayName();
    if (!who.isEmpty()) {
        m_status->setText(Datagate::tr("Signed in as %1.").arg(who));
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::updateConnectButtonUi(bool vpnUp)
{
    m_vpnTunnelUp = vpnUp;
    if (!m_connectBtn) {
        return;
    }
    if (vpnUp) {
        m_connectBtn->setEnabled(true);
        m_connectBtn->setText(Datagate::tr("Disconnect"));
    } else {
        m_connectBtn->setEnabled(true);
        m_connectBtn->setText(Datagate::tr("Connect"));
    }
}

void MainWindow::shutdownVpn()
{
    if (m_vpn) {
        m_vpn->disconnectVpn("application quit");
    }
}

void MainWindow::showEvent(QShowEvent* e)
{
    QMainWindow::showEvent(e);
    if (m_centerOnFirstShow) {
        m_centerOnFirstShow = false;
        AppTheme::centerWidgetOnScreen(this);
    }
    if (!m_didSilentServerRefresh) {
        m_didSilentServerRefresh = true;
        refreshServers(false);
    }
}

void MainWindow::loadSettings()
{
    QSettings s;
    m_openVpnPath->setText(s.value(QStringLiteral("openVpnCmd"), QStringLiteral("openvpn")).toString());

    const bool dark = s.value(QStringLiteral("themeDark"), true).toBool();
    m_themeDark->blockSignals(true);
    m_themeDark->setChecked(dark);
    m_themeDark->blockSignals(false);
    AppTheme::apply(dark);

    const bool autoMode = s.value(QStringLiteral("connectServerModeAuto"), true).toBool();
    m_serverModeCombo->blockSignals(true);
    m_serverModeCombo->setCurrentIndex(autoMode ? 0 : 1);
    m_serverModeCombo->blockSignals(false);

    const int savedId = s.value(QStringLiteral("manualVpnServerId"), 0).toInt();
    if (savedId > 0) {
        const int ix = m_manualServerCombo->findData(savedId);
        if (ix >= 0) {
            m_manualServerCombo->setCurrentIndex(ix);
        }
    }

    const QString lang = DatagateTranslator::currentLanguageCode();
    m_languageCombo->blockSignals(true);
    const int lix = m_languageCombo->findData(lang);
    if (lix >= 0) {
        m_languageCombo->setCurrentIndex(lix);
    } else {
        m_languageCombo->setCurrentIndex(0);
    }
    m_languageCombo->blockSignals(false);
    DatagateTranslator::installForLanguage(lang);

    updateServerModeUi();
    retranslateUi();
    if (m_statisticsPanel) {
        m_statisticsPanel->retranslateUi();
    }
#if defined(__linux__) && defined(DATAGATE_EMBEDDED_OPENVPN3)
    if (!AppConfig::openVpnUseSystemBinary()) {
        const QString capLine = DatagateUtils::linuxFileGetcapLine(
            DatagateUtils::linuxEmbeddedTunCapabilityTargetExecutablePath());
        qCInfo(lcUi, "Embedded OpenVPN 3: getcap %s", qPrintable(capLine));
    }
#endif
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.setValue(QStringLiteral("openVpnCmd"), m_openVpnPath->text().trimmed());
    s.setValue(QStringLiteral("themeDark"), m_themeDark->isChecked());
    s.setValue(QStringLiteral("connectServerModeAuto"), m_serverModeCombo->currentData().toInt() == 0);
    const int mid = m_manualServerCombo->currentData().toInt();
    if (mid > 0) {
        s.setValue(QStringLiteral("manualVpnServerId"), mid);
    }
    s.setValue(QStringLiteral("General/language"), m_languageCombo->currentData().toString());
}

void MainWindow::navigateTo(int index)
{
    if (index >= 0 && index < m_stack->count()) {
        m_nav->setCurrentRow(index);
        m_stack->setCurrentIndex(index);
    }
}

void MainWindow::appendLog(const QString& line)
{
    m_log->appendPlainText(line);
}

void MainWindow::updateServerModeUi()
{
    const bool manual = m_serverModeCombo && m_serverModeCombo->currentData().toInt() != 0;
    if (m_manualServerWrap) {
        m_manualServerWrap->setVisible(manual);
    }
    if (m_manualServerCombo) {
        m_manualServerCombo->setEnabled(manual);
    }
}

void MainWindow::grantOpenVpnCapNetAdmin()
{
#if defined(DATAGATE_EMBEDDED_OPENVPN3)
    const bool capDataGateBinary = !AppConfig::openVpnUseSystemBinary();
#else
    const bool capDataGateBinary = false;
#endif
    const QString exe = capDataGateBinary
#if defined(__linux__)
        ? DatagateUtils::linuxEmbeddedTunCapabilityTargetExecutablePath()
#else
        ? QCoreApplication::applicationFilePath()
#endif
        : DatagateUtils::resolvedOpenVpnExecutable(m_openVpnPath->text());
    const QFileInfo fi(exe);
    if (!fi.exists()) {
        QMessageBox::warning(this, Datagate::tr("DataGate"),
            Datagate::tr("Executable not found: %1").arg(exe));
        return;
    }
    if (!fi.isExecutable()) {
        QMessageBox::warning(this, Datagate::tr("DataGate"),
            Datagate::tr("Not an executable: %1").arg(exe));
        return;
    }
    if (!capDataGateBinary && !exe.contains(QStringLiteral("openvpn"), Qt::CaseInsensitive)) {
        QMessageBox::warning(this, Datagate::tr("DataGate"),
            Datagate::tr("Refusing setcap: path must contain “openvpn”."));
        return;
    }
#if defined(__linux__)
    QProcess p;
    p.start(QStringLiteral("pkexec"),
        QStringList{QStringLiteral("setcap"), QStringLiteral("cap_net_admin+ep"), fi.canonicalFilePath()});
    p.waitForFinished(-1);
    const QString errOut = QString::fromUtf8(p.readAllStandardError()).trimmed();
    const QString stdOut = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    const QString combined = stdOut.isEmpty() ? errOut : (stdOut + Datagate::tr("\n") + errOut);
    if (p.exitCode() == 0) {
        const QString capVerify = DatagateUtils::linuxFileGetcapLine(fi.canonicalFilePath());
        QMessageBox::information(this, Datagate::tr("DataGate"),
            Datagate::tr(
                "setcap finished.\n%1\n\ngetcap:\n%2\n\n"
                "You must quit DataGate completely and start it again — only a new process picks up file capabilities.\n\n"
                "After each CMake rebuild, run Grant again (the new executable loses the previous capability).")
                .arg(combined.isEmpty() ? Datagate::tr("(no pkexec output)") : combined,
                    capVerify.isEmpty() ? Datagate::tr("(empty — run: getcap \"%1\")").arg(fi.canonicalFilePath())
                                        : capVerify));
        appendLog(Datagate::tr("setcap cap_net_admin+ep on ") + fi.canonicalFilePath());
        if (capDataGateBinary) {
            appendLog(Datagate::tr("Restart DataGate (exit app, then launch again) for TUN to work."));
        }
    } else {
        QMessageBox::warning(
            this,
            Datagate::tr("DataGate"),
            Datagate::tr("pkexec/setcap failed (exit %1).\n\n%2\n\nManual:\n"
                           "sudo setcap cap_net_admin+ep %3\ngetcap %3")
                .arg(p.exitCode())
                .arg(combined.isEmpty() ? Datagate::tr("(no output)") : combined)
                .arg(fi.canonicalFilePath()));
    }
#else
    Q_UNUSED(exe);
    QMessageBox::information(this, Datagate::tr("DataGate"), Datagate::tr("Linux only."));
#endif
}

void MainWindow::onOpenVpnCapabilityRecommended(const QString& detail)
{
    m_status->setText(Datagate::tr("TUN / OpenVPN capability setup may be required."));
    appendLog(detail);
    updateConnectButtonUi(false);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(Datagate::tr("DataGate"));
#if defined(DATAGATE_EMBEDDED_OPENVPN3)
    if (!AppConfig::openVpnUseSystemBinary()) {
        box.setText(Datagate::tr(
            "Embedded OpenVPN runs inside DataGate. Linux requires CAP_NET_ADMIN on the DataGate "
            "executable to create a TUN device (not on the separate openvpn binary)."));
    } else
#endif
    {
        box.setText(Datagate::tr(
            "OpenVPN needs CAP_NET_ADMIN on the openvpn binary to create a TUN device. DataGate cannot add this to itself."));
    }
    box.setInformativeText(detail);
    auto* grantBtn = box.addButton(Datagate::tr("Grant TUN capability…"), QMessageBox::AcceptRole);
    auto* settingsBtn = box.addButton(Datagate::tr("Open Settings"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Close);
    box.exec();
    if (box.clickedButton() == grantBtn) {
        grantOpenVpnCapNetAdmin();
    } else if (box.clickedButton() == settingsBtn) {
        navigateTo(3);
    }
}

void MainWindow::applyWssServerList(const QByteArray& statusJsonBody)
{
    const QVector<QPair<int, QString>> wss = DatagateUtils::listWssServersFromStatusJson(statusJsonBody);
    m_manualServerCombo->clear();
    for (const auto& p : wss) {
        m_manualServerCombo->addItem(p.second, p.first);
    }
    if (m_statisticsPanel) {
        m_statisticsPanel->setServers(wss);
        const QString base = AppConfig::apiBaseUrl().trimmed();
        QString tok;
        if (m_session->ensureValidAccessToken()) {
            tok = m_session->accessToken();
        }
        m_statisticsPanel->loadStatistics(base, tok);
    }
    QSettings s;
    const int savedId = s.value(QStringLiteral("manualVpnServerId"), 0).toInt();
    if (savedId > 0) {
        const int ix = m_manualServerCombo->findData(savedId);
        if (ix >= 0) {
            m_manualServerCombo->setCurrentIndex(ix);
        }
    }
}

void MainWindow::refreshServers(bool showErrorDialogs)
{
    QString base = AppConfig::apiBaseUrl().trimmed();
    if (base.isEmpty()) {
        if (showErrorDialogs) {
            QMessageBox::information(
                this,
                Datagate::tr("DataGate"),
                Datagate::tr("Configure Api:BaseUrl in appsettings.json."));
            navigateTo(3);
        }
        return;
    }
    if (!m_session->ensureValidAccessToken()) {
        if (showErrorDialogs) {
            QMessageBox::warning(
                this,
                Datagate::tr("DataGate"),
                Datagate::tr("The API is unavailable — try again later. If your network is fine, sign out (Settings) and sign in again."));
        }
        return;
    }
    const QString tok = m_session->accessToken();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    const QUrl url(base + QStringLiteral("/api/open-vpn-servers/get-all-with-status"));
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + tok).toUtf8());

    m_refreshServersBtn->setEnabled(false);
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, showErrorDialogs]() {
        m_refreshServersBtn->setEnabled(true);
        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            if (showErrorDialogs) {
                QMessageBox::warning(this, Datagate::tr("DataGate"), err);
            } else {
                appendLog(Datagate::tr("Silent server refresh failed: %1").arg(err));
            }
            return;
        }
        m_serverTable->setRowCount(0);
        m_accessTotalClientsCount = -1;
        if (m_accessTotalClientsLabel) {
            m_accessTotalClientsLabel->setText(Datagate::tr("Total clients: —"));
        }
        const QByteArray raw = reply->readAll();
        reply->deleteLater();
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            if (showErrorDialogs) {
                QMessageBox::warning(this, Datagate::tr("DataGate"), Datagate::tr("Invalid JSON."));
            }
            return;
        }
        const QJsonArray arr = doc.object()
            .value(QStringLiteral("data"))
            .toObject()
            .value(QStringLiteral("openVpnServerWithStatuses"))
            .toArray();
        int totalClients = 0;
        for (const QJsonValue& v : arr) {
            const QJsonObject item = v.toObject();
            const QJsonObject serverObj = item.value(QStringLiteral("openVpnServerResponses"))
                .toObject()
                .value(QStringLiteral("openVpnServer"))
                .toObject();
            const QString name = serverObj.value(QStringLiteral("serverName")).toString();
            const bool online = serverObj.value(QStringLiteral("isOnline")).toBool();
            const int clients = item.value(QStringLiteral("countConnectedClients")).toInt();
            totalClients += clients;

            const int row = m_serverTable->rowCount();
            m_serverTable->insertRow(row);
            m_serverTable->setItem(row, 0,
                new QTableWidgetItem(name.isEmpty() ? Datagate::tr("—") : name));
            m_serverTable->setItem(row, 1,
                new QTableWidgetItem(online ? Datagate::tr("yes") : Datagate::tr("no")));
            m_serverTable->setItem(row, 2, new QTableWidgetItem(QString::number(clients)));
        }
        m_accessTotalClientsCount = totalClients;
        if (m_accessTotalClientsLabel) {
            m_accessTotalClientsLabel->setText(
                Datagate::tr("Total clients: %1").arg(totalClients));
        }
        applyWssServerList(raw);
    });
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        if (m_statisticsPanel) {
            m_statisticsPanel->retranslateUi();
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi()
{
    setWindowTitle(Datagate::tr("DataGate OpenVPN 3"));
    if (m_appTitleLabel) {
        m_appTitleLabel->setText(Datagate::tr("DataGate OpenVPN 3"));
    }
    if (m_nav && m_nav->count() >= 4) {
        m_nav->item(0)->setText(Datagate::tr("Home"));
        m_nav->item(1)->setText(Datagate::tr("Access"));
        m_nav->item(2)->setText(Datagate::tr("Statistics"));
        m_nav->item(3)->setText(Datagate::tr("Settings"));
    }
    if (m_welcomeLabel) {
        m_welcomeLabel->setText(Datagate::tr("Welcome to DataGate OpenVPN 3"));
    }
    if (m_connTitleLabel) {
        m_connTitleLabel->setText(Datagate::tr("Connection status"));
    }
    if (m_vpnServerLabel) {
        m_vpnServerLabel->setText(Datagate::tr("VPN server:"));
    }
    if (m_manualServerLabel) {
        m_manualServerLabel->setText(Datagate::tr("Server:"));
    }
    if (m_serverModeCombo) {
        m_serverModeCombo->setItemText(0, Datagate::tr("Automatic (best server)"));
        m_serverModeCombo->setItemText(1, Datagate::tr("Choose server…"));
    }
    if (m_logSectionTitle) {
        m_logSectionTitle->setText(Datagate::tr("Engine logs"));
    }
    if (m_accessPageTitle) {
        m_accessPageTitle->setText(Datagate::tr("Access"));
    }
    if (m_refreshServersBtn) {
        m_refreshServersBtn->setText(Datagate::tr("Refresh server list"));
    }
    if (m_serverTable) {
        m_serverTable->setHorizontalHeaderLabels({
            Datagate::tr("Server"),
            Datagate::tr("Online"),
            Datagate::tr("Clients"),
        });
    }
    if (m_accessTotalClientsLabel) {
        if (m_accessTotalClientsCount < 0) {
            m_accessTotalClientsLabel->setText(Datagate::tr("Total clients: —"));
        } else {
            m_accessTotalClientsLabel->setText(
                Datagate::tr("Total clients: %1").arg(m_accessTotalClientsCount));
        }
    }
    if (m_statPageTitle) {
        m_statPageTitle->setText(Datagate::tr("Statistics"));
    }
    if (m_settingsPageTitle) {
        m_settingsPageTitle->setText(Datagate::tr("Settings"));
    }
    if (m_langHeading) {
        m_langHeading->setText(Datagate::tr("Language"));
    }
    if (m_langHint) {
        m_langHint->setText(Datagate::tr(
            "Choosing a language updates the interface immediately; it is saved automatically. "
            "Use Save settings for VPN path, theme, and server options."));
    }
    if (m_appearHeading) {
        m_appearHeading->setText(Datagate::tr("Appearance"));
    }
    if (m_appearHint) {
        m_appearHint->setText(Datagate::tr("Choose the application theme."));
    }
    if (m_themeLabel) {
        m_themeLabel->setText(Datagate::tr("Theme"));
    }
    if (m_themeDark) {
        m_themeDark->setText(Datagate::tr("Dark mode"));
    }
    if (m_downloadHeading) {
        m_downloadHeading->setText(Datagate::tr("More apps"));
    }
    if (m_downloadHint) {
        m_downloadHint->setText(Datagate::tr(
            "You can download other DataGate apps for your devices from the website:"));
    }
    if (m_downloadPlatformWin) {
        m_downloadPlatformWin->setText(Datagate::tr("Windows"));
    }
    if (m_downloadPlatformLinux) {
        m_downloadPlatformLinux->setText(Datagate::tr("Linux"));
    }
    if (m_downloadPlatformAndroid) {
        m_downloadPlatformAndroid->setText(Datagate::tr("Android"));
    }
    if (m_downloadPlatformIos) {
        m_downloadPlatformIos->setText(Datagate::tr("iOS"));
    }
    if (m_downloadLinkLabel) {
        m_downloadLinkLabel->setText(
            QStringLiteral("<a href=\"https://datagateapp.com/download\">https://datagateapp.com/download</a>"));
    }
    if (m_openVpnPath) {
        m_openVpnPath->setPlaceholderText(Datagate::tr("openvpn"));
    }
    if (m_settingsVpnForm && m_openVpnPath) {
        if (QLabel* lb = qobject_cast<QLabel*>(m_settingsVpnForm->labelForField(m_openVpnPath))) {
            lb->setText(Datagate::tr("OpenVPN command"));
        }
    }
    if (m_tunHintLabel) {
#if defined(DATAGATE_EMBEDDED_OPENVPN3)
        if (!AppConfig::openVpnUseSystemBinary()) {
#if defined(DATAGATE_EMBEDDED_OPENVPN3_USE_EXTERNAL_HELPER)
            m_tunHintLabel->setText(Datagate::tr(
                "Tunnel (TUN): embedded OpenVPN uses DataGateOvpn3Helper (next to this app). Grant CAP_NET_ADMIN on "
                "that binary (Grant TUN or sudo setcap cap_net_admin+ep …), then fully restart. Re-run Grant after each "
                "rebuild."));
#else
            m_tunHintLabel->setText(Datagate::tr(
                "Tunnel (TUN): embedded OpenVPN runs in-process — Linux needs CAP_NET_ADMIN on this application "
                "binary. Use Grant TUN or sudo setcap, then fully restart."));
#endif
        } else
#endif
        {
            m_tunHintLabel->setText(Datagate::tr(
                "Tunnel (TUN): Linux allows TUN only with CAP_NET_ADMIN on the openvpn binary. "
                "Use Grant TUN capability or: sudo setcap cap_net_admin+ep $(command -v openvpn). "
                "Avoid running the whole app as sudo if possible."));
        }
    }
    if (m_grantTunCapBtn) {
        m_grantTunCapBtn->setText(Datagate::tr("Grant TUN capability…"));
    }
    if (m_saveSettingsBtn) {
        m_saveSettingsBtn->setText(Datagate::tr("Save settings"));
    }
    if (m_accountHeading) {
        m_accountHeading->setText(Datagate::tr("Account"));
    }
    if (m_accountHint) {
        m_accountHint->setText(Datagate::tr("Sign out from the application."));
    }
    if (m_logoutBtn) {
        m_logoutBtn->setText(Datagate::tr("Logout"));
    }
    if (!m_vpnTunnelUp && m_status) {
        const QString who = m_session ? m_session->displayName() : QString();
        if (!who.isEmpty()) {
            m_status->setText(Datagate::tr("Signed in as %1.").arg(who));
        } else {
            m_status->setText(Datagate::tr("Idle"));
        }
    }
    if (m_connectedToLabel) {
        if (m_vpnTunnelUp && m_vpn) {
            const QString n = m_vpn->activeServerName();
            m_connectedToLabel->setText(
                Datagate::tr("Connected to: %1").arg(n.isEmpty() ? Datagate::tr("—") : n));
        } else {
            m_connectedToLabel->setText(Datagate::tr("Connected to: —"));
        }
    }
    if (m_connectBtn && m_connectBtn->isEnabled()) {
        updateConnectButtonUi(m_vpnTunnelUp);
    }
}
