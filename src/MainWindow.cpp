#include "MainWindow.h"
#include "ClientIdentity.h"

#include "AppConfig.h"
#include "AppLogging.h"
#include "AppTheme.h"
#include "AuthSession.h"
#include "DatagateAuthLogin.h"
#include "DatagateTr.h"
#include "DatagateTranslator.h"
#include "DatagateUtils.h"
#include "LocalProfileStore.h"
#include "StatisticsPanel.h"
#include "VpnSession.h"
#include "XrayClientLinksApi.h"
#include "XrayConfigBuilder.h"
#include "XraySession.h"

#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QListWidget>
#include <QMessageBox>
#include <QPair>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QLocale>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
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
    , m_nam(new DatagateNetworkAccessManager(this))
    , m_vpn(new VpnSession(this))
    , m_xray(new XraySession(this))
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
        iconFromThemeOr({QStringLiteral("document-open"), QStringLiteral("folder"), QStringLiteral("text-x-generic")},
            st, QStyle::SP_FileIcon),
        Datagate::tr("Profiles")));
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
    m_serverModeCombo->addItem(Datagate::tr("Local profile…"), 2);
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

    m_localProfileWrap = new QWidget(this);
    auto* locRow = new QHBoxLayout(m_localProfileWrap);
    locRow->setContentsMargins(0, 0, 0, 0);
    m_localProfileLabel = new QLabel(Datagate::tr("Profile:"), this);
    locRow->addWidget(m_localProfileLabel);
    m_localProfileCombo = new QComboBox(this);
    m_localProfileCombo->setMinimumWidth(200);
    locRow->addWidget(m_localProfileCombo, 1);
    connLay->addWidget(m_localProfileWrap);

    auto* btnRow = new QHBoxLayout();
    m_connectBtn = new QPushButton(Datagate::tr("Connect"), this);
    m_connectBtn->setProperty("primary", true);
    m_connectBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
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

    m_accessSubscriptionCard = new QFrame(this);
    m_accessSubscriptionCard->setObjectName(QStringLiteral("card"));
    auto* subLay = new QVBoxLayout(m_accessSubscriptionCard);
    subLay->setContentsMargins(18, 18, 18, 18);
    subLay->setSpacing(8);
    m_accessSubscriptionHeading = new QLabel(Datagate::tr("Your plan & quotas"), this);
    m_accessSubscriptionHeading->setObjectName(QStringLiteral("titleH2"));
    subLay->addWidget(m_accessSubscriptionHeading);
    m_accessPlanLabel = new QLabel(Datagate::tr("Plan: —"), this);
    m_accessPlanLabel->setObjectName(QStringLiteral("muted"));
    m_accessPlanLabel->setWordWrap(true);
    subLay->addWidget(m_accessPlanLabel);
    m_accessTrafficTitleLabel = new QLabel(Datagate::tr("Traffic quota"), this);
    m_accessTrafficTitleLabel->setObjectName(QStringLiteral("accessTrafficTitle"));
    subLay->addWidget(m_accessTrafficTitleLabel);
    m_accessQuotaMetaLabel = new QLabel(this);
    m_accessQuotaMetaLabel->setObjectName(QStringLiteral("muted"));
    m_accessQuotaMetaLabel->setWordWrap(true);
    subLay->addWidget(m_accessQuotaMetaLabel);
    m_accessQuotaBar = new QProgressBar(this);
    m_accessQuotaBar->setObjectName(QStringLiteral("accessQuotaProgress"));
    m_accessQuotaBar->setRange(0, 100);
    m_accessQuotaBar->setTextVisible(false);
    m_accessQuotaBar->setFixedHeight(12);
    subLay->addWidget(m_accessQuotaBar);
    m_accessQuotaStatsLabel = new QLabel(this);
    m_accessQuotaStatsLabel->setObjectName(QStringLiteral("muted"));
    m_accessQuotaStatsLabel->setWordWrap(true);
    subLay->addWidget(m_accessQuotaStatsLabel);
    m_accessValidLabel = new QLabel(Datagate::tr("Valid until: —"), this);
    m_accessValidLabel->setObjectName(QStringLiteral("muted"));
    m_accessValidLabel->setWordWrap(true);
    subLay->addWidget(m_accessValidLabel);
    accLay->addWidget(m_accessSubscriptionCard);

    auto* accRow = new QHBoxLayout();
    m_refreshServersBtn = new QPushButton(Datagate::tr("Refresh server list"), this);
    m_refreshServersBtn->setProperty("primary", true);
    m_refreshServersBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    accRow->addWidget(m_refreshServersBtn);
    m_connectAccessXrayBtn = new QPushButton(Datagate::tr("Connect VLESS (Xray)…"), this);
    m_connectAccessXrayBtn->setProperty("secondary", true);
    m_connectAccessXrayBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    accRow->addWidget(m_connectAccessXrayBtn);
    accRow->addStretch();
    accLay->addLayout(accRow);
    m_serverTable = new QTableWidget(this);
    m_serverTable->setObjectName(QStringLiteral("accessServerTable"));
    m_serverTable->setColumnCount(5);
    m_serverTable->setHorizontalHeaderLabels({
        Datagate::tr("Server"),
        Datagate::tr("Type"),
        Datagate::tr("Online"),
        Datagate::tr("Clients"),
        Datagate::tr("Plan access"),
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

    // ----- Page: Profiles
    auto* profilesPage = new QWidget(this);
    auto* profLay = new QVBoxLayout(profilesPage);
    profLay->setContentsMargins(20, 20, 20, 20);
    profLay->setSpacing(12);
    m_profilesPageTitle = new QLabel(Datagate::tr("Profiles"), this);
    m_profilesPageTitle->setObjectName(QStringLiteral("titleH1"));
    profLay->addWidget(m_profilesPageTitle);
    m_profilesHint = new QLabel(
        Datagate::tr(
            "Import OpenVPN (.ovpn) or Xray (vless:// / JSON). Xray uses TUN (+ local SOCKS). "
            "After connect, approve pkexec once to install TUN routes. "
            "Grant TUN capability on DataGateXray after each rebuild."),
        this);
    m_profilesHint->setObjectName(QStringLiteral("muted"));
    m_profilesHint->setWordWrap(true);
    profLay->addWidget(m_profilesHint);

    auto* profCard = new QFrame(this);
    profCard->setObjectName(QStringLiteral("card"));
    auto* profCardLay = new QVBoxLayout(profCard);
    profCardLay->setContentsMargins(18, 18, 18, 18);
    m_profilesList = new QListWidget(this);
    m_profilesList->setSelectionMode(QAbstractItemView::SingleSelection);
    profCardLay->addWidget(m_profilesList, 1);

    auto shrinkBtn = [](QPushButton* b) {
        b->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    };
    auto* profBtns = new QVBoxLayout();
    profBtns->setSpacing(8);
    auto* profImportRow = new QHBoxLayout();
    m_importOvpnBtn = new QPushButton(Datagate::tr("Import .ovpn…"), this);
    m_importOvpnBtn->setProperty("primary", true);
    shrinkBtn(m_importOvpnBtn);
    m_importXrayBtn = new QPushButton(Datagate::tr("Import VLESS / JSON…"), this);
    m_importXrayBtn->setProperty("secondary", true);
    shrinkBtn(m_importXrayBtn);
    profImportRow->addWidget(m_importOvpnBtn);
    profImportRow->addWidget(m_importXrayBtn);
    profImportRow->addStretch();
    auto* profActionRow = new QHBoxLayout();
    m_renameProfileBtn = new QPushButton(Datagate::tr("Rename"), this);
    m_renameProfileBtn->setProperty("secondary", true);
    shrinkBtn(m_renameProfileBtn);
    m_deleteProfileBtn = new QPushButton(Datagate::tr("Delete"), this);
    m_deleteProfileBtn->setProperty("secondary", true);
    shrinkBtn(m_deleteProfileBtn);
    m_connectProfileBtn = new QPushButton(Datagate::tr("Connect"), this);
    m_connectProfileBtn->setProperty("primary", true);
    shrinkBtn(m_connectProfileBtn);
    profActionRow->addWidget(m_renameProfileBtn);
    profActionRow->addWidget(m_deleteProfileBtn);
    profActionRow->addStretch();
    profActionRow->addWidget(m_connectProfileBtn);
    profBtns->addLayout(profImportRow);
    profBtns->addLayout(profActionRow);
    profCardLay->addLayout(profBtns);
    profLay->addWidget(profCard, 1);

    m_stack->addWidget(profilesPage);

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
    m_grantTunCapBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    vpnLay->addRow(m_grantTunCapBtn);
    m_saveSettingsBtn = new QPushButton(Datagate::tr("Save settings"), this);
    m_saveSettingsBtn->setProperty("secondary", true);
    m_saveSettingsBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
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
    m_logoutBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    auto* logoutRow = new QHBoxLayout();
    logoutRow->addWidget(m_logoutBtn);
    logoutRow->addStretch();
    accCardLay->addLayout(logoutRow);
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
        if (m_freeTierNavChecksEnabled && (row == 0 || row == 1)) {
            checkFreeTierOnboarding();
        }
        if (row == 2) {
            refreshLocalProfilesUi();
        }
        if (row == 1) {
            refreshServers(true);
        }
    });
    m_nav->setCurrentRow(0);
    m_freeTierNavChecksEnabled = true;

    connect(m_serverModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        updateServerModeUi();
    });
    if (m_profilesList) {
        connect(m_profilesList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem*, QListWidgetItem*) {
            updateLocalProfileActionsEnabled();
        });
    }

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
    connect(m_importOvpnBtn, &QPushButton::clicked, this, &MainWindow::importLocalOvpnProfile);
    connect(m_importXrayBtn, &QPushButton::clicked, this, &MainWindow::importLocalXrayProfile);
    connect(m_renameProfileBtn, &QPushButton::clicked, this, &MainWindow::renameSelectedLocalProfile);
    connect(m_deleteProfileBtn, &QPushButton::clicked, this, &MainWindow::deleteSelectedLocalProfile);
    connect(m_connectProfileBtn, &QPushButton::clicked, this, &MainWindow::connectSelectedLocalProfile);
    connect(m_connectAccessXrayBtn, &QPushButton::clicked, this, &MainWindow::connectAccessXrayVless);
    connect(m_connectBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        if (!m_connectBtn->isEnabled()) {
            return;
        }
        const QString btnText = m_connectBtn->text();
        if (btnText == Datagate::tr("Connecting…") || btnText == Datagate::tr("Disconnecting…")) {
            return;
        }
        if (btnText == Datagate::tr("Disconnect")) {
            m_connectBtn->setEnabled(false);
            m_connectBtn->setText(Datagate::tr("Disconnecting…"));
            appendLog(Datagate::tr("Disconnecting…"));
            disconnectAllTunnels("Disconnect button");
            return;
        }

        const int mode = m_serverModeCombo->currentData().toInt();
        if (mode == 2) {
            const QString profileId = m_localProfileCombo ? m_localProfileCombo->currentData().toString() : QString();
            if (profileId.isEmpty()) {
                QMessageBox::warning(this, Datagate::tr("DataGate"),
                    Datagate::tr("Choose a local profile or import an .ovpn / VLESS config."));
                return;
            }
            const auto meta = LocalProfileStore::find(profileId);
            if (!meta.has_value()) {
                QMessageBox::warning(this, Datagate::tr("DataGate"), Datagate::tr("Profile not found."));
                return;
            }
            disconnectAllTunnels("switch profile");
            if (meta->type == LocalProfileStore::Type::Xray) {
                QString err;
                const QString json = LocalProfileStore::readXrayConfigText(profileId, &err);
                if (json.isEmpty()) {
                    QMessageBox::warning(this, Datagate::tr("DataGate"), err);
                    return;
                }
                m_connectBtn->setEnabled(false);
                m_connectBtn->setText(Datagate::tr("Connecting…"));
                m_xray->connectFromConfigText(json, meta->name, profileId);
                return;
            }
            QString err;
            const QString ovpn = LocalProfileStore::readOvpnText(profileId, &err);
            if (ovpn.isEmpty()) {
                QMessageBox::warning(this, Datagate::tr("DataGate"), err);
                return;
            }
            m_connectBtn->setEnabled(false);
            m_connectBtn->setText(Datagate::tr("Connecting…"));
            m_vpn->connectLocalOvpn(ovpn, meta->name, m_openVpnPath->text(), profileId);
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
            const QString hint = m_session->lastEnsureFailure();
            QMessageBox::warning(
                this,
                Datagate::tr("DataGate"),
                hint.isEmpty()
                    ? Datagate::tr("Could not obtain an access token. Sign in again from Settings → Logout.")
                    : hint);
            return;
        }
        const QString tok = m_session->accessToken();
        QString licErr;
        if (!DatagateUtils::fetchUserVpnAccessInfoSync(m_nam, url, tok, &m_lastVpnAccessInfo, &licErr)) {
            qCWarning(lcUi, "Connect: quota info failed: %s (connect allowed)", qPrintable(licErr));
        } else {
            applyVpnAccessInfoToAccessPage();
        }
        const bool autoPick = mode == 0;
        int manualId = 0;
        int manualType = 0;
        if (!autoPick) {
            manualId = m_manualServerCombo->currentData().toInt();
            manualType = m_manualServerCombo->currentData(Qt::UserRole + 1).toInt();
            if (manualId <= 0) {
                QMessageBox::warning(
                    this, Datagate::tr("DataGate"), Datagate::tr("Choose a VPN server or refresh the list."));
                return;
            }
        }
        if (!autoPick && manualType == 1) {
            const QString serverName = m_manualServerCombo->currentText();
            QString cleanName = serverName;
            const QString xraySuffix = Datagate::tr(" (Xray)");
            const QString ovpnSuffix = Datagate::tr(" (OpenVPN)");
            if (cleanName.endsWith(xraySuffix)) {
                cleanName.chop(xraySuffix.size());
            } else if (cleanName.endsWith(ovpnSuffix)) {
                cleanName.chop(ovpnSuffix.size());
            }
            connectXrayForServer(manualId, cleanName);
            return;
        }
        disconnectAllTunnels("switch to cloud OpenVPN");
        m_connectBtn->setEnabled(false);
        m_connectBtn->setText(Datagate::tr("Connecting…"));
        m_vpn->connectVpn(url, tok, m_openVpnPath->text(), autoPick, manualId);
    });
    connect(m_refreshServersBtn, &QPushButton::clicked, this, [this]() { refreshServers(true); });

    connect(m_logoutBtn, &QPushButton::clicked, this, [this]() {
        emit logoutRequested();
    });

    wireSessionSignals();

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
    disconnectAllTunnels("application quit");
}

bool MainWindow::anyTunnelUp() const
{
    return m_vpnTunnelUp;
}

void MainWindow::disconnectAllTunnels(const char* reason)
{
    if (m_vpn) {
        m_vpn->disconnectVpn(reason);
    }
    if (m_xray) {
        m_xray->disconnectXray(reason);
    }
    // OpenVPN→Xray switch leaves host/def1 routes if openvpn exited uncleanly; scrub so Xray
    // does not inherit a blackhole and the UI does not look "dead" after Disconnect.
    DatagateUtils::scrubStaleLinuxVpnRoutes();
}

void MainWindow::wireSessionSignals()
{
    const auto onUp = [this](const QString& name) {
        updateConnectButtonUi(true);
        if (m_connectedToLabel) {
            m_connectedToLabel->setText(Datagate::tr("Connected to: %1").arg(name.isEmpty() ? Datagate::tr("—") : name));
        }
    };
    const auto onDown = [this]() {
        // Another engine may still be up (should not happen, but keep UI honest).
        if (m_xray && m_xray->isRunning()) {
            updateConnectButtonUi(true);
            if (m_connectedToLabel) {
                m_connectedToLabel->setText(
                    Datagate::tr("Connected to: %1").arg(m_xray->activeDisplayName()));
            }
            return;
        }
        // Do not use activeServerName(): it used to linger after disconnect and flip the button
        // back to «Disconnect», which caused an «Отключение…» spam loop on every click.
        if (m_vpn && m_vpn->isRunning()) {
            updateConnectButtonUi(true);
            if (m_connectedToLabel) {
                m_connectedToLabel->setText(
                    Datagate::tr("Connected to: %1").arg(m_vpn->activeServerName()));
            }
            return;
        }
        updateConnectButtonUi(false);
        if (m_connectedToLabel) {
            m_connectedToLabel->setText(Datagate::tr("Connected to: —"));
        }
    };

    connect(m_vpn, &VpnSession::statusMessage, this, [this](const QString& t) {
        m_status->setText(t);
        appendLog(t);
    });
    connect(m_vpn, &VpnSession::engineLog, this, [this](const QString& t) {
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
    connect(m_vpn, &VpnSession::vpnUp, this, [this, onUp]() { onUp(m_vpn->activeServerName()); });
    connect(m_vpn, &VpnSession::vpnDown, this, onDown);

    connect(m_xray, &XraySession::statusMessage, this, [this](const QString& t) {
        m_status->setText(t);
        appendLog(t);
    });
    connect(m_xray, &XraySession::engineLog, this, [this](const QString& t) {
        appendLog(t);
    });
    connect(m_xray, &XraySession::errorMessage, this, [this](const QString& t) {
        m_status->setText(t);
        appendLog(Datagate::tr("Error: ") + t);
        QMessageBox::warning(this, Datagate::tr("DataGate"), t);
        updateConnectButtonUi(false);
    });
    connect(m_xray, &XraySession::openVpnCapabilitySetupRecommended, this,
        &MainWindow::onOpenVpnCapabilityRecommended);
    connect(m_xray, &XraySession::vpnUp, this, [this, onUp]() {
        QString label = m_xray->activeDisplayName();
        if (AppConfig::xrayUseTun()) {
            label += Datagate::tr(" (TUN + SOCKS :%1)").arg(AppConfig::xraySocksPort());
        } else {
            label += Datagate::tr(" (SOCKS :%1)").arg(AppConfig::xraySocksPort());
        }
        onUp(label);
    });
    connect(m_xray, &XraySession::vpnDown, this, onDown);
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

    const int savedMode = s.value(QStringLiteral("connectServerMode"), -1).toInt();
    int modeIndex = 0;
    if (savedMode == 0 || savedMode == 1 || savedMode == 2) {
        modeIndex = m_serverModeCombo->findData(savedMode);
    } else {
        const bool autoMode = s.value(QStringLiteral("connectServerModeAuto"), true).toBool();
        modeIndex = autoMode ? 0 : 1;
    }
    if (modeIndex < 0) {
        modeIndex = 0;
    }
    m_serverModeCombo->blockSignals(true);
    m_serverModeCombo->setCurrentIndex(modeIndex);
    m_serverModeCombo->blockSignals(false);

    const int savedId = s.value(QStringLiteral("manualVpnServerId"), 0).toInt();
    if (savedId > 0) {
        const int ix = m_manualServerCombo->findData(savedId);
        if (ix >= 0) {
            m_manualServerCombo->setCurrentIndex(ix);
        }
    }

    refreshLocalProfilesUi();
    const QString savedProfileId = s.value(QStringLiteral("localVpnProfileId")).toString();
    if (!savedProfileId.isEmpty() && m_localProfileCombo) {
        const int pix = m_localProfileCombo->findData(savedProfileId);
        if (pix >= 0) {
            m_localProfileCombo->setCurrentIndex(pix);
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
    const int mode = m_serverModeCombo->currentData().toInt();
    s.setValue(QStringLiteral("connectServerMode"), mode);
    s.setValue(QStringLiteral("connectServerModeAuto"), mode == 0);
    const int mid = m_manualServerCombo->currentData().toInt();
    if (mid > 0) {
        s.setValue(QStringLiteral("manualVpnServerId"), mid);
    }
    if (m_localProfileCombo) {
        const QString pid = m_localProfileCombo->currentData().toString();
        if (!pid.isEmpty()) {
            s.setValue(QStringLiteral("localVpnProfileId"), pid);
        }
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
    const int mode = m_serverModeCombo ? m_serverModeCombo->currentData().toInt() : 0;
    const bool manual = mode == 1;
    const bool local = mode == 2;
    if (m_manualServerWrap) {
        m_manualServerWrap->setVisible(manual);
    }
    if (m_manualServerCombo) {
        m_manualServerCombo->setEnabled(manual);
    }
    if (m_localProfileWrap) {
        m_localProfileWrap->setVisible(local);
    }
    if (m_localProfileCombo) {
        m_localProfileCombo->setEnabled(local);
    }
    if (local) {
        refreshLocalProfilesUi();
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
    const auto runSetcap = [this](const QString& path) -> QPair<int, QString> {
        QProcess p;
        p.start(QStringLiteral("pkexec"),
            QStringList{QStringLiteral("setcap"), QStringLiteral("cap_net_admin+eip"), path});
        p.waitForFinished(-1);
        const QString errOut = QString::fromUtf8(p.readAllStandardError()).trimmed();
        const QString stdOut = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        const QString combined = stdOut.isEmpty() ? errOut : (stdOut + Datagate::tr("\n") + errOut);
        return {p.exitCode(), combined};
    };

    QStringList targets;
    targets << fi.canonicalFilePath();
    const QString xrayBeside =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("DataGateXray"));
    const QFileInfo xrayFi(xrayBeside);
    if (xrayFi.exists() && xrayFi.isExecutable()) {
        const QString xrayCanon = xrayFi.canonicalFilePath();
        if (!targets.contains(xrayCanon)) {
            targets << xrayCanon;
        }
    }

    QString report;
    int failCount = 0;
    for (const QString& target : targets) {
        const auto [code, combined] = runSetcap(target);
        const QString capVerify = DatagateUtils::linuxFileGetcapLine(target);
        report += Datagate::tr("\n— %1 —\n").arg(target);
        if (code == 0) {
            appendLog(Datagate::tr("setcap cap_net_admin+eip on ") + target);
            report += Datagate::tr("setcap OK.\ngetcap: %1\n")
                          .arg(capVerify.isEmpty() ? Datagate::tr("(empty)") : capVerify);
        } else {
            ++failCount;
            report += Datagate::tr("pkexec/setcap failed (exit %1).\n%2\n")
                          .arg(code)
                          .arg(combined.isEmpty() ? Datagate::tr("(no output)") : combined);
        }
    }

    if (failCount == 0) {
#if defined(DATAGATE_EMBEDDED_OPENVPN3) && defined(DATAGATE_EMBEDDED_OPENVPN3_USE_EXTERNAL_HELPER)
        const bool helperOnly = !AppConfig::openVpnUseSystemBinary();
#else
        const bool helperOnly = false;
#endif
        if (helperOnly) {
            QMessageBox::information(this, Datagate::tr("DataGate"),
                Datagate::tr(
                    "TUN capability granted.%1\n\n"
                    "Press Connect again — DataGateOvpn3Helper is a separate process and picks up "
                    "capabilities on the next start (no need to quit the app).\n\n"
                    "After each rebuild/copy of the helper, run Grant once more.")
                    .arg(report));
            appendLog(Datagate::tr("Grant TUN OK — connect again."));
        } else {
            QMessageBox::information(this, Datagate::tr("DataGate"),
                Datagate::tr(
                    "setcap finished for OpenVPN/helper and DataGateXray (when present).%1\n\n"
                    "You must quit DataGate completely and start it again — only a new process picks up file "
                    "capabilities.\n\n"
                    "After each CMake rebuild, run Grant again.")
                    .arg(report));
            appendLog(Datagate::tr("Restart DataGate (exit app, then launch again) for TUN to work."));
        }
    } else {
        QMessageBox::warning(this, Datagate::tr("DataGate"),
            Datagate::tr("Some setcap calls failed.%1\n\nManual example:\nsudo setcap cap_net_admin+ep %2")
                .arg(report, fi.canonicalFilePath()));
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
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(grantBtn);
    box.exec();
    if (box.clickedButton() == grantBtn) {
        grantOpenVpnCapNetAdmin();
    } else if (box.clickedButton() == settingsBtn) {
        navigateTo(4);
    }
}

void MainWindow::applyWssServerList(const QByteArray& statusJsonBody)
{
    const QVector<BestServer> servers = DatagateUtils::listManualConnectServersFromStatusJson(statusJsonBody);
    m_manualServerCombo->clear();
    for (const BestServer& s : servers) {
        QString label = s.name;
        if (s.isXray()) {
            label += Datagate::tr(" (Xray)");
        } else {
            label += Datagate::tr(" (OpenVPN)");
        }
        m_manualServerCombo->addItem(label, s.serverId);
        const int ix = m_manualServerCombo->count() - 1;
        m_manualServerCombo->setItemData(ix, s.serverType, Qt::UserRole + 1);
    }
    if (m_statisticsPanel) {
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
            navigateTo(4);
        }
        return;
    }
    if (!m_session->ensureValidAccessToken()) {
        if (showErrorDialogs) {
            const QString hint = m_session->lastEnsureFailure();
            QMessageBox::warning(
                this,
                Datagate::tr("DataGate"),
                hint.isEmpty()
                    ? Datagate::tr("Could not refresh the session. Sign in again.")
                    : hint);
        }
        return;
    }
    const QString tok = m_session->accessToken();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    const QUrl url(base + QStringLiteral("/api/v3/open-vpn-servers/get-all-with-status"));
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + tok).toUtf8());

    int keepServerId = 0;
    if (m_serverTable && m_serverTable->currentRow() >= 0) {
        if (QTableWidgetItem* it = m_serverTable->item(m_serverTable->currentRow(), 0)) {
            keepServerId = it->data(Qt::UserRole).toInt();
        }
    }

    m_refreshServersBtn->setEnabled(false);
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, showErrorDialogs, base, tok, keepServerId]() {
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
            const int serverId = serverObj.value(QStringLiteral("id")).toInt(
                serverObj.value(QStringLiteral("Id")).toInt(0));
            const int serverType = serverObj.contains(QStringLiteral("serverType"))
                ? serverObj.value(QStringLiteral("serverType")).toInt(0)
                : serverObj.value(QStringLiteral("ServerType")).toInt(0);
            const bool online = serverObj.value(QStringLiteral("isOnline")).toBool();
            const int clients = item.value(QStringLiteral("countConnectedClients")).toInt();
            const QJsonValue acc1 = serverObj.value(QStringLiteral("isAccessibleForUserQuotaPlan"));
            const QJsonValue acc2 = serverObj.value(QStringLiteral("IsAccessibleForUserQuotaPlan"));
            bool inPlan = true;
            if (!acc1.isUndefined()) {
                inPlan = acc1.toBool(true);
            } else if (!acc2.isUndefined()) {
                inPlan = acc2.toBool(true);
            }
            totalClients += clients;

            const int row = m_serverTable->rowCount();
            m_serverTable->insertRow(row);
            auto* nameCell = new QTableWidgetItem(name.isEmpty() ? Datagate::tr("—") : name);
            nameCell->setData(Qt::UserRole, serverId);
            nameCell->setData(Qt::UserRole + 1, serverType);
            m_serverTable->setItem(row, 0, nameCell);
            m_serverTable->setItem(row, 1,
                new QTableWidgetItem(serverType == 1 ? Datagate::tr("Xray") : Datagate::tr("OpenVPN")));
            m_serverTable->setItem(row, 2,
                new QTableWidgetItem(online ? Datagate::tr("yes") : Datagate::tr("no")));
            m_serverTable->setItem(row, 3, new QTableWidgetItem(QString::number(clients)));
            m_serverTable->setItem(row, 4,
                new QTableWidgetItem(inPlan ? Datagate::tr("yes") : Datagate::tr("no")));
        }
        m_accessTotalClientsCount = totalClients;
        if (m_accessTotalClientsLabel) {
            m_accessTotalClientsLabel->setText(
                Datagate::tr("Total clients: %1").arg(totalClients));
        }
        applyWssServerList(raw);

        int restoreRow = -1;
        if (keepServerId > 0) {
            for (int r = 0; r < m_serverTable->rowCount(); ++r) {
                if (QTableWidgetItem* it = m_serverTable->item(r, 0)) {
                    if (it->data(Qt::UserRole).toInt() == keepServerId) {
                        restoreRow = r;
                        break;
                    }
                }
            }
        }
        if (restoreRow < 0 && m_serverTable->rowCount() > 0) {
            restoreRow = 0;
        }
        if (restoreRow >= 0) {
            m_serverTable->setCurrentCell(restoreRow, 0);
            m_serverTable->selectRow(restoreRow);
        }

        QString accErr;
        if (DatagateUtils::fetchUserVpnAccessInfoSync(m_nam, base, tok, &m_lastVpnAccessInfo, &accErr)) {
            applyVpnAccessInfoToAccessPage();
        } else {
            qCWarning(lcUi, "Subscription / license info: %s", qPrintable(accErr));
        }
    });
}

void MainWindow::applyVpnAccessInfoToAccessPage()
{
    const DatagateUtils::UserVpnAccessInfo& i = m_lastVpnAccessInfo;
    if (m_accessPlanLabel) {
        if (!i.quotaApiError.isEmpty()) {
            m_accessPlanLabel->setText(Datagate::tr("Quota: %1").arg(i.quotaApiError));
        } else {
            const QString p = i.planName.isEmpty() ? QStringLiteral("—") : i.planName;
            m_accessPlanLabel->setText(Datagate::tr("Plan: %1").arg(p));
        }
    }
    if (m_accessTrafficTitleLabel) {
        m_accessTrafficTitleLabel->setVisible(i.quotaApiError.isEmpty());
    }
    if (m_accessQuotaMetaLabel) {
        if (!i.quotaApiError.isEmpty()) {
            m_accessQuotaMetaLabel->setVisible(false);
        } else {
            QString meta;
            if (!i.planName.isEmpty()) {
                meta = i.planName;
            }
            if (i.quotaPeriodIsMonthly && i.quotaLimitBytes > 0) {
                meta.append(meta.isEmpty() ? QString() : QStringLiteral(" · "));
                meta.append(Datagate::tr("This calendar month"));
            } else if (!i.quotaPeriodIsMonthly && i.quotaLimitBytes > 0) {
                meta.append(meta.isEmpty() ? QString() : QStringLiteral(" · "));
                meta.append(Datagate::tr("Today"));
            }
            m_accessQuotaMetaLabel->setText(meta);
            m_accessQuotaMetaLabel->setVisible(!meta.isEmpty());
        }
    }
    if (m_accessQuotaBar && m_accessQuotaStatsLabel) {
        if (!i.quotaApiError.isEmpty()) {
            m_accessQuotaBar->setVisible(false);
            m_accessQuotaStatsLabel->setVisible(false);
        } else if (i.trafficUsageNeedsExternalId) {
            m_accessQuotaBar->setVisible(false);
            m_accessQuotaStatsLabel->setVisible(true);
            m_accessQuotaStatsLabel->setText(Datagate::tr(
                "Traffic usage needs an OpenVPN client ID (external ID) on your account."));
        } else if (i.quotaLimitBytes <= 0) {
            m_accessQuotaBar->setVisible(false);
            m_accessQuotaStatsLabel->setVisible(true);
            m_accessQuotaStatsLabel->setText(Datagate::tr(
                "No daily or monthly traffic limit on the active quota plan for today, or no plan is active."));
        } else if (i.trafficUsedBytesForPeriod < 0) {
            m_accessQuotaBar->setVisible(false);
            m_accessQuotaStatsLabel->setVisible(true);
            m_accessQuotaStatsLabel->setText(Datagate::tr("Usage data unavailable."));
        } else {
            const qint64 used = i.trafficUsedBytesForPeriod;
            const qint64 lim = i.quotaLimitBytes;
            const double pct = lim > 0 ? qMin(100.0, 100.0 * static_cast<double>(used) / static_cast<double>(lim)) : 0;
            const bool over = used > lim;
            m_accessQuotaBar->setVisible(true);
            m_accessQuotaStatsLabel->setVisible(true);
            m_accessQuotaBar->setRange(0, 100);
            m_accessQuotaBar->setValue(static_cast<int>(qRound(pct)));
            m_accessQuotaBar->setProperty("overquota", over);
            m_accessQuotaBar->style()->unpolish(m_accessQuotaBar);
            m_accessQuotaBar->style()->polish(m_accessQuotaBar);
            const QString uStr = DatagateUtils::formatDataSizeBytes(used);
            const QString lStr = DatagateUtils::formatDataSizeBytes(lim);
            QString stats = Datagate::tr("Used %1 / %2 (%3%)")
                                .arg(uStr, lStr, QString::number(pct, 'f', 1));
            stats += QLatin1Char('\n');
            if (over) {
                stats += Datagate::tr("Over by %1").arg(DatagateUtils::formatDataSizeBytes(used - lim));
            } else {
                stats += Datagate::tr("Remaining %1").arg(DatagateUtils::formatDataSizeBytes(lim - used));
            }
            m_accessQuotaStatsLabel->setText(stats);
        }
    }
    if (m_accessValidLabel) {
        QStringList parts;
        if (!i.effectiveFrom.isEmpty()) {
            const QString d = DatagateUtils::formatIsoDateTimeForLocale(i.effectiveFrom);
            parts << Datagate::tr("Effective from: %1").arg(d.isEmpty() ? i.effectiveFrom : d);
        }
        if (!i.assignmentNote.isEmpty()) {
            parts << Datagate::tr("Note: %1").arg(i.assignmentNote);
        }
        if (i.validUntil.isValid()) {
            parts << Datagate::tr("Valid until: %1")
                         .arg(QLocale().toString(i.validUntil.toLocalTime(), QLocale::ShortFormat));
        }
        if (parts.isEmpty()) {
            m_accessValidLabel->setText(Datagate::tr("—"));
        } else {
            m_accessValidLabel->setText(parts.join(QLatin1Char('\n')));
        }
    }
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
    if (m_nav && m_nav->count() >= 5) {
        m_nav->item(0)->setText(Datagate::tr("Home"));
        m_nav->item(1)->setText(Datagate::tr("Access"));
        m_nav->item(2)->setText(Datagate::tr("Profiles"));
        m_nav->item(3)->setText(Datagate::tr("Statistics"));
        m_nav->item(4)->setText(Datagate::tr("Settings"));
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
    if (m_localProfileLabel) {
        m_localProfileLabel->setText(Datagate::tr("Profile:"));
    }
    if (m_serverModeCombo && m_serverModeCombo->count() >= 3) {
        m_serverModeCombo->setItemText(0, Datagate::tr("Automatic (best server)"));
        m_serverModeCombo->setItemText(1, Datagate::tr("Choose server…"));
        m_serverModeCombo->setItemText(2, Datagate::tr("Local profile…"));
    }
    if (m_profilesPageTitle) {
        m_profilesPageTitle->setText(Datagate::tr("Profiles"));
    }
    if (m_profilesHint) {
        m_profilesHint->setText(Datagate::tr(
            "Import OpenVPN (.ovpn) or Xray (vless:// / JSON). Xray uses TUN (+ local SOCKS). "
            "After connect, approve pkexec once to install TUN routes. "
            "Grant TUN capability on DataGateXray after each rebuild."));
    }
    if (m_importOvpnBtn) {
        m_importOvpnBtn->setText(Datagate::tr("Import .ovpn…"));
    }
    if (m_importXrayBtn) {
        m_importXrayBtn->setText(Datagate::tr("Import VLESS / JSON…"));
    }
    if (m_renameProfileBtn) {
        m_renameProfileBtn->setText(Datagate::tr("Rename"));
    }
    if (m_deleteProfileBtn) {
        m_deleteProfileBtn->setText(Datagate::tr("Delete"));
    }
    if (m_connectProfileBtn) {
        m_connectProfileBtn->setText(Datagate::tr("Connect"));
    }
    if (m_connectAccessXrayBtn) {
        m_connectAccessXrayBtn->setText(Datagate::tr("Connect VLESS (Xray)…"));
    }
    if (m_logSectionTitle) {
        m_logSectionTitle->setText(Datagate::tr("Engine logs"));
    }
    if (m_accessPageTitle) {
        m_accessPageTitle->setText(Datagate::tr("Access"));
    }
    if (m_accessSubscriptionHeading) {
        m_accessSubscriptionHeading->setText(Datagate::tr("Your plan & quotas"));
    }
    if (m_accessTrafficTitleLabel) {
        m_accessTrafficTitleLabel->setText(Datagate::tr("Traffic quota"));
    }
    applyVpnAccessInfoToAccessPage();
    if (m_refreshServersBtn) {
        m_refreshServersBtn->setText(Datagate::tr("Refresh server list"));
    }
    if (m_serverTable) {
        m_serverTable->setHorizontalHeaderLabels({
            Datagate::tr("Server"),
            Datagate::tr("Type"),
            Datagate::tr("Online"),
            Datagate::tr("Clients"),
            Datagate::tr("Plan access"),
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
        if (m_vpnTunnelUp) {
            QString n;
            if (m_xray && m_xray->isRunning()) {
                n = m_xray->activeDisplayName();
            } else if (m_vpn) {
                n = m_vpn->activeServerName();
            }
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

QString MainWindow::selectedLocalProfileId() const
{
    if (!m_profilesList) {
        return {};
    }
    QListWidgetItem* item = m_profilesList->currentItem();
    if (!item) {
        return {};
    }
    return item->data(Qt::UserRole).toString();
}

void MainWindow::updateLocalProfileActionsEnabled()
{
    const bool has = !selectedLocalProfileId().isEmpty();
    if (m_renameProfileBtn) {
        m_renameProfileBtn->setEnabled(has);
    }
    if (m_deleteProfileBtn) {
        m_deleteProfileBtn->setEnabled(has);
    }
    if (m_connectProfileBtn) {
        m_connectProfileBtn->setEnabled(has);
    }
}

void MainWindow::selectLocalProfileInUi(const QString& profileId)
{
    if (profileId.isEmpty()) {
        return;
    }
    if (m_localProfileCombo) {
        const int ix = m_localProfileCombo->findData(profileId);
        if (ix >= 0) {
            m_localProfileCombo->setCurrentIndex(ix);
        }
    }
    if (m_profilesList) {
        for (int i = 0; i < m_profilesList->count(); ++i) {
            if (m_profilesList->item(i)->data(Qt::UserRole).toString() == profileId) {
                m_profilesList->setCurrentRow(i);
                break;
            }
        }
    }
    updateLocalProfileActionsEnabled();
}

void MainWindow::refreshLocalProfilesUi()
{
    const QString keepHome = m_localProfileCombo ? m_localProfileCombo->currentData().toString() : QString();
    const QString keepList = selectedLocalProfileId();

    if (m_localProfileCombo) {
        m_localProfileCombo->blockSignals(true);
        m_localProfileCombo->clear();
    }
    if (m_profilesList) {
        m_profilesList->clear();
    }

    const auto profiles = LocalProfileStore::list();
    for (const LocalProfileStore::Profile& p : profiles) {
        QString label = p.name;
        if (p.type == LocalProfileStore::Type::Xray) {
            label += Datagate::tr(" (Xray)");
        } else {
            label += Datagate::tr(" (OpenVPN)");
        }
        if (m_localProfileCombo) {
            m_localProfileCombo->addItem(label, p.id);
        }
        if (m_profilesList) {
            auto* item = new QListWidgetItem(label, m_profilesList);
            item->setData(Qt::UserRole, p.id);
        }
    }

    if (m_localProfileCombo) {
        if (!keepHome.isEmpty()) {
            const int ix = m_localProfileCombo->findData(keepHome);
            if (ix >= 0) {
                m_localProfileCombo->setCurrentIndex(ix);
            }
        }
        m_localProfileCombo->blockSignals(false);
    }
    if (m_profilesList && !keepList.isEmpty()) {
        for (int i = 0; i < m_profilesList->count(); ++i) {
            if (m_profilesList->item(i)->data(Qt::UserRole).toString() == keepList) {
                m_profilesList->setCurrentRow(i);
                break;
            }
        }
    }
    updateLocalProfileActionsEnabled();
}

void MainWindow::importLocalOvpnProfile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        Datagate::tr("Import OpenVPN profile"),
        QString(),
        Datagate::tr("OpenVPN configs (*.ovpn *.conf);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    bool ok = false;
    const QString suggested = QFileInfo(path).completeBaseName();
    const QString name = QInputDialog::getText(
        this,
        Datagate::tr("Profile name"),
        Datagate::tr("Display name:"),
        QLineEdit::Normal,
        suggested,
        &ok)
                             .trimmed();
    if (!ok) {
        return;
    }
    QString err;
    const auto created = LocalProfileStore::importOvpn(path, name, &err);
    if (!created.has_value()) {
        QMessageBox::warning(this, Datagate::tr("Profiles"), err);
        return;
    }
    refreshLocalProfilesUi();
    selectLocalProfileInUi(created->id);
    appendLog(Datagate::tr("Imported local profile: %1").arg(created->name));
}

void MainWindow::importLocalXrayProfile()
{
    QMessageBox box(this);
    box.setWindowTitle(Datagate::tr("Import Xray"));
    box.setText(Datagate::tr("Import a vless:// link or an Xray JSON config file."));
    auto* uriBtn = box.addButton(Datagate::tr("Paste vless://…"), QMessageBox::AcceptRole);
    auto* fileBtn = box.addButton(Datagate::tr("Choose JSON file…"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == uriBtn) {
        bool ok = false;
        const QString uri = QInputDialog::getMultiLineText(
            this, Datagate::tr("Import VLESS"), Datagate::tr("vless:// link:"), QString(), &ok)
                                .trimmed();
        if (!ok || uri.isEmpty()) {
            return;
        }
        bool nameOk = false;
        const QString name = QInputDialog::getText(
            this, Datagate::tr("Profile name"), Datagate::tr("Display name:"), QLineEdit::Normal, QString(), &nameOk)
                                 .trimmed();
        if (!nameOk) {
            return;
        }
        QString err;
        const auto created = LocalProfileStore::importXrayUri(uri, name, &err);
        if (!created.has_value()) {
            QMessageBox::warning(this, Datagate::tr("Profiles"), err);
            return;
        }
        refreshLocalProfilesUi();
        selectLocalProfileInUi(created->id);
        appendLog(Datagate::tr("Imported Xray profile: %1").arg(created->name));
        return;
    }
    if (box.clickedButton() != fileBtn) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this,
        Datagate::tr("Import Xray JSON"),
        QString(),
        Datagate::tr("JSON configs (*.json);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    bool ok = false;
    const QString suggested = QFileInfo(path).completeBaseName();
    const QString name = QInputDialog::getText(
        this, Datagate::tr("Profile name"), Datagate::tr("Display name:"), QLineEdit::Normal, suggested, &ok)
                             .trimmed();
    if (!ok) {
        return;
    }
    QString err;
    const auto created = LocalProfileStore::importXrayJsonFile(path, name, &err);
    if (!created.has_value()) {
        QMessageBox::warning(this, Datagate::tr("Profiles"), err);
        return;
    }
    refreshLocalProfilesUi();
    selectLocalProfileInUi(created->id);
    appendLog(Datagate::tr("Imported Xray profile: %1").arg(created->name));
}

void MainWindow::renameSelectedLocalProfile()
{
    const QString id = selectedLocalProfileId();
    if (id.isEmpty()) {
        QMessageBox::information(this, Datagate::tr("Profiles"), Datagate::tr("Select a profile first."));
        return;
    }
    const auto meta = LocalProfileStore::find(id);
    bool ok = false;
    const QString name = QInputDialog::getText(
        this,
        Datagate::tr("Rename profile"),
        Datagate::tr("Display name:"),
        QLineEdit::Normal,
        meta.has_value() ? meta->name : QString(),
        &ok)
                             .trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    QString err;
    if (!LocalProfileStore::rename(id, name, &err)) {
        QMessageBox::warning(this, Datagate::tr("Profiles"), err);
        return;
    }
    refreshLocalProfilesUi();
}

void MainWindow::deleteSelectedLocalProfile()
{
    const QString id = selectedLocalProfileId();
    if (id.isEmpty()) {
        QMessageBox::information(this, Datagate::tr("Profiles"), Datagate::tr("Select a profile first."));
        return;
    }
    if ((m_vpn && !m_vpn->activeLocalProfileId().isEmpty() && m_vpn->activeLocalProfileId() == id && m_vpnTunnelUp)
        || (m_xray && !m_xray->activeLocalProfileId().isEmpty() && m_xray->activeLocalProfileId() == id
            && m_xray->isRunning())) {
        QMessageBox::warning(
            this,
            Datagate::tr("Profiles"),
            Datagate::tr("Disconnect from this profile before deleting it."));
        return;
    }
    const auto meta = LocalProfileStore::find(id);
    const QString label = meta.has_value() ? meta->name : id;
    const auto answer = QMessageBox::question(
        this,
        Datagate::tr("Profiles"),
        Datagate::tr("Delete profile \"%1\"?").arg(label));
    if (answer != QMessageBox::Yes) {
        return;
    }
    QString err;
    if (!LocalProfileStore::remove(id, &err)) {
        QMessageBox::warning(this, Datagate::tr("Profiles"), err);
        return;
    }
    refreshLocalProfilesUi();
    appendLog(Datagate::tr("Deleted local profile: %1").arg(label));
}

void MainWindow::connectSelectedLocalProfile()
{
    const QString id = selectedLocalProfileId();
    if (id.isEmpty()) {
        QMessageBox::information(this, Datagate::tr("Profiles"), Datagate::tr("Select a profile first."));
        return;
    }
    if (m_serverModeCombo) {
        const int ix = m_serverModeCombo->findData(2);
        if (ix >= 0) {
            m_serverModeCombo->setCurrentIndex(ix);
        }
    }
    if (m_localProfileCombo) {
        const int ix = m_localProfileCombo->findData(id);
        if (ix >= 0) {
            m_localProfileCombo->setCurrentIndex(ix);
        }
    }
    updateServerModeUi();
    navigateTo(0);
    if (m_connectBtn) {
        m_connectBtn->click();
    }
}

void MainWindow::connectAccessXrayVless()
{
    if (!m_serverTable || m_serverTable->currentRow() < 0) {
        QMessageBox::information(
            this, Datagate::tr("Access"), Datagate::tr("Select a server row first."));
        return;
    }
    const int row = m_serverTable->currentRow();
    QTableWidgetItem* nameItem = m_serverTable->item(row, 0);
    if (!nameItem) {
        return;
    }
    const int serverId = nameItem->data(Qt::UserRole).toInt();
    const int serverType = nameItem->data(Qt::UserRole + 1).toInt();
    if (serverId <= 0) {
        QMessageBox::warning(this, Datagate::tr("Access"), Datagate::tr("Invalid server id."));
        return;
    }
    if (serverType != 1) {
        QMessageBox::information(
            this,
            Datagate::tr("Access"),
            Datagate::tr("Selected server is OpenVPN. Pick a row with Type = Xray, or use Home → Connect for OpenVPN."));
        return;
    }
    connectXrayForServer(serverId, nameItem->text());
}

void MainWindow::connectXrayForServer(int serverId, const QString& serverName)
{
    const QString url = AppConfig::apiBaseUrl().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, Datagate::tr("DataGate"), Datagate::tr("Api:BaseUrl is missing in appsettings.json."));
        return;
    }
    if (!m_session->ensureValidAccessToken()) {
        const QString hint = m_session->lastEnsureFailure();
        QMessageBox::warning(
            this,
            Datagate::tr("DataGate"),
            hint.isEmpty()
                ? Datagate::tr("Could not obtain an access token. Sign in again.")
                : hint);
        return;
    }
    const QString tok = m_session->accessToken();
    DatagateUtils::ensureInstallationId();
    const QString ext = DatagateUtils::externalIdFromJwt(tok);
    if (ext.isEmpty()) {
        QMessageBox::warning(this, Datagate::tr("Access"),
            Datagate::tr("Could not read externalId from JWT (need sub, externalId, or nameid)."));
        return;
    }
    const QString installId = DatagateUtils::installationIdFromSettings();
    // Stable CN (one link per user+server). Also try legacy device-scoped CN for older links.
    const QString stableCn = QStringLiteral("ldg-xray-%1-%2").arg(serverId).arg(ext);
    const QString legacyCn = QStringLiteral("ldg-xray-%1-%2-%3").arg(serverId).arg(ext).arg(installId);
    const QString appVer = QCoreApplication::applicationVersion().trimmed();
    const QString issuedTo = QStringLiteral("datagate linux app %1 user %2 device %3")
                                 .arg(appVer.isEmpty() ? QStringLiteral("unknown") : appVer, ext, installId);

    appendLog(Datagate::tr("Requesting Xray VLESS link for %1…").arg(serverName));
    QString err;
    const auto link = XrayClientLinksApi::ensureAndDownloadByCn(
        m_nam, url, tok, serverId, QStringList{stableCn, legacyCn}, ext, issuedTo, &err);
    if (!link.has_value()) {
        QMessageBox::warning(this, Datagate::tr("Access"), err);
        return;
    }

    QString buildErr;
    const QString configJson = XrayConfigBuilder::clientConfigFromOpaqueText(
        link->contentUtf8, AppConfig::xraySocksPort(), AppConfig::xrayUseTun(), &buildErr);
    if (configJson.isEmpty()) {
        QMessageBox::warning(this, Datagate::tr("Access"), buildErr);
        return;
    }

    const QString profileName = serverName + Datagate::tr(" (Xray)");
    QString importErr;
    const auto saved = LocalProfileStore::upsertXrayJsonText(configJson, profileName, &importErr);
    if (!saved.has_value()) {
        QMessageBox::warning(this, Datagate::tr("Access"), importErr);
        return;
    }
    refreshLocalProfilesUi();
    selectLocalProfileInUi(saved->id);
    if (m_serverModeCombo) {
        const int ix = m_serverModeCombo->findData(2);
        if (ix >= 0) {
            m_serverModeCombo->setCurrentIndex(ix);
        }
    }
    updateServerModeUi();

    disconnectAllTunnels("Access Xray connect");
    navigateTo(0);
    m_connectBtn->setEnabled(false);
    m_connectBtn->setText(Datagate::tr("Connecting…"));
    m_xray->connectFromConfigText(configJson, profileName, saved->id);
}

void MainWindow::checkFreeTierOnboarding()
{
    if (!m_session || !m_session->ensureValidAccessToken()) {
        return;
    }
    const QString base = AppConfig::apiBaseUrl().trimmed();
    if (base.isEmpty()) {
        return;
    }
    DatagateAuth::showFreeTierOnboardingIfRequired(m_nam, base, m_session, this);
}
