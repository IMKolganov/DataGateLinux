#include "MainWindow.h"

#include "AppConfig.h"
#include "AppLogging.h"
#include "AppTheme.h"
#include "AuthSession.h"
#include "VpnSession.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QShowEvent>

MainWindow::MainWindow(AuthSession* session, QWidget* parent)
    : QMainWindow(parent)
    , m_session(session)
    , m_nam(new QNetworkAccessManager(this))
    , m_vpn(new VpnSession(this))
{
    setWindowTitle(QStringLiteral("DataGate OpenVPN 3"));
    resize(980, 620);
    setMinimumSize(640, 480);

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralRoot"));
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(16, 12, 16, 8);
    auto* appTitle = new QLabel(QStringLiteral("DataGate OpenVPN 3"), this);
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
    m_nav->addItem(new QListWidgetItem(st->standardIcon(QStyle::SP_DirHomeIcon), QStringLiteral("Home")));
    m_nav->addItem(new QListWidgetItem(st->standardIcon(QStyle::SP_DriveNetIcon), QStringLiteral("Access")));
    m_nav->addItem(new QListWidgetItem(st->standardIcon(QStyle::SP_FileDialogContentsView), QStringLiteral("Statistics")));
    m_nav->addItem(new QListWidgetItem(st->standardIcon(QStyle::SP_FileDialogDetailedView), QStringLiteral("Settings")));

    m_stack = new QStackedWidget(this);

    // ----- Page: Home
    auto* homePage = new QWidget(this);
    auto* homeOuter = new QVBoxLayout(homePage);
    homeOuter->setContentsMargins(20, 20, 20, 20);
    homeOuter->setSpacing(16);

    auto* welcome = new QLabel(QStringLiteral("Welcome to DataGate OpenVPN 3"), this);
    welcome->setObjectName(QStringLiteral("titleH1"));
    homeOuter->addWidget(welcome);

    auto* connCard = new QFrame(this);
    connCard->setObjectName(QStringLiteral("card"));
    auto* connLay = new QVBoxLayout(connCard);
    connLay->setContentsMargins(18, 18, 18, 18);
    auto* connTitle = new QLabel(QStringLiteral("Connection status"), this);
    connTitle->setObjectName(QStringLiteral("titleH2"));
    connLay->addWidget(connTitle);
    m_status = new QLabel(QStringLiteral("Idle"), this);
    m_status->setObjectName(QStringLiteral("muted"));
    m_status->setWordWrap(true);
    connLay->addWidget(m_status);
    auto* btnRow = new QHBoxLayout();
    m_connectBtn = new QPushButton(QStringLiteral("Connect"), this);
    m_connectBtn->setProperty("primary", true);
    m_disconnectBtn = new QPushButton(QStringLiteral("Disconnect"), this);
    m_disconnectBtn->setProperty("secondary", true);
    m_disconnectBtn->setEnabled(false);
    btnRow->addWidget(m_connectBtn);
    btnRow->addWidget(m_disconnectBtn);
    btnRow->addStretch();
    connLay->addLayout(btnRow);
    homeOuter->addWidget(connCard);

    auto* logCard = new QFrame(this);
    logCard->setObjectName(QStringLiteral("card"));
    auto* logLay = new QVBoxLayout(logCard);
    logLay->setContentsMargins(18, 18, 18, 18);
    auto* logTitle = new QLabel(QStringLiteral("Engine logs"), this);
    logTitle->setObjectName(QStringLiteral("titleH2"));
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
    auto* accLay = new QVBoxLayout(accessPage);
    accLay->setContentsMargins(20, 20, 20, 20);
    accLay->setSpacing(12);
    auto* accTitle = new QLabel(QStringLiteral("Access"), this);
    accTitle->setObjectName(QStringLiteral("titleH1"));
    accLay->addWidget(accTitle);
    auto* accRow = new QHBoxLayout();
    m_refreshServersBtn = new QPushButton(QStringLiteral("Refresh server list"), this);
    m_refreshServersBtn->setProperty("primary", true);
    accRow->addWidget(m_refreshServersBtn);
    accRow->addStretch();
    accLay->addLayout(accRow);
    m_serverList = new QListWidget(this);
    accLay->addWidget(m_serverList, 1);
    m_stack->addWidget(accessPage);

    // ----- Page: Statistics
    auto* statPage = new QWidget(this);
    auto* statLay = new QVBoxLayout(statPage);
    statLay->setContentsMargins(20, 20, 20, 20);
    statLay->setSpacing(12);
    auto* statTitle = new QLabel(QStringLiteral("Statistics"), this);
    statTitle->setObjectName(QStringLiteral("titleH1"));
    statLay->addWidget(statTitle);
    auto* statHint = new QLabel(
        QStringLiteral("Usage charts and overview will appear here (same role as DataGateWin Statistics)."),
        this);
    statHint->setObjectName(QStringLiteral("muted"));
    statHint->setWordWrap(true);
    statLay->addWidget(statHint);
    statLay->addStretch();
    m_stack->addWidget(statPage);

    // ----- Page: Settings
    auto* settingsPage = new QWidget(this);
    auto* setOuter = new QVBoxLayout(settingsPage);
    setOuter->setContentsMargins(20, 20, 20, 20);
    setOuter->setSpacing(16);
    auto* setTitle = new QLabel(QStringLiteral("Settings"), this);
    setTitle->setObjectName(QStringLiteral("titleH1"));
    setOuter->addWidget(setTitle);

    auto* appearCard = new QFrame(this);
    appearCard->setObjectName(QStringLiteral("card"));
    auto* appearLay = new QVBoxLayout(appearCard);
    appearLay->setContentsMargins(18, 18, 18, 18);
    auto* appearH = new QLabel(QStringLiteral("Appearance"), this);
    appearH->setObjectName(QStringLiteral("titleH2"));
    appearLay->addWidget(appearH);
    auto* appearSub = new QLabel(QStringLiteral("Choose the application theme."), this);
    appearSub->setObjectName(QStringLiteral("muted"));
    appearLay->addWidget(appearSub);
    auto* themeRow = new QHBoxLayout();
    auto* themeLbl = new QLabel(QStringLiteral("Theme"), this);
    themeLbl->setObjectName(QStringLiteral("titleH2"));
    m_themeDark = new QCheckBox(QStringLiteral("Dark mode"), this);
    m_themeDark->setChecked(true);
    themeRow->addWidget(themeLbl);
    themeRow->addStretch();
    themeRow->addWidget(m_themeDark);
    appearLay->addLayout(themeRow);
    setOuter->addWidget(appearCard);

    auto* vpnCard = new QFrame(this);
    vpnCard->setObjectName(QStringLiteral("card"));
    auto* vpnLay = new QFormLayout(vpnCard);
    vpnLay->setContentsMargins(18, 18, 18, 18);
    vpnLay->setSpacing(12);
    m_openVpnPath = new QLineEdit(this);
    m_openVpnPath->setPlaceholderText(QStringLiteral("openvpn"));
    vpnLay->addRow(QStringLiteral("OpenVPN command"), m_openVpnPath);
    m_saveSettingsBtn = new QPushButton(QStringLiteral("Save settings"), this);
    m_saveSettingsBtn->setProperty("secondary", true);
    vpnLay->addRow(m_saveSettingsBtn);
    setOuter->addWidget(vpnCard);

    auto* accountCard = new QFrame(this);
    accountCard->setObjectName(QStringLiteral("card"));
    auto* accCardLay = new QVBoxLayout(accountCard);
    accCardLay->setContentsMargins(18, 18, 18, 18);
    auto* accH = new QLabel(QStringLiteral("Account"), this);
    accH->setObjectName(QStringLiteral("titleH2"));
    accCardLay->addWidget(accH);
    auto* accSub = new QLabel(QStringLiteral("Sign out from the application."), this);
    accSub->setObjectName(QStringLiteral("muted"));
    accCardLay->addWidget(accSub);
    m_logoutBtn = new QPushButton(QStringLiteral("Logout"), this);
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
    });
    m_nav->setCurrentRow(0);

    connect(m_themeDark, &QCheckBox::toggled, this, [this](bool dark) {
        AppTheme::apply(dark);
        QSettings s;
        s.setValue(QStringLiteral("themeDark"), dark);
    });

    connect(m_saveSettingsBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        appendLog(QStringLiteral("Settings saved."));
    });
    connect(m_connectBtn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        const QString url = AppConfig::apiBaseUrl().trimmed();
        if (url.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("DataGate"), QStringLiteral("Api:BaseUrl is missing in appsettings.json."));
            return;
        }
        qCInfo(lcUi, "Connect: ensureValidAccessToken");
        if (!m_session->ensureValidAccessToken()) {
            qCWarning(lcUi, "Connect: ensureValidAccessToken failed");
            QMessageBox::warning(
                this,
                QStringLiteral("DataGate"),
                QStringLiteral("Could not obtain a valid access token (same check as DataGateWin). "
                               "Use Settings → Logout, then sign in again, or check the network / API."));
            return;
        }
        const QString tok = m_session->accessToken();
        m_connectBtn->setEnabled(false);
        m_vpn->connectVpn(url, tok, m_openVpnPath->text());
    });
    connect(m_disconnectBtn, &QPushButton::clicked, this, [this]() {
        appendLog(QStringLiteral("Disconnecting…"));
        m_vpn->disconnectVpn("Disconnect button");
    });
    connect(m_refreshServersBtn, &QPushButton::clicked, this, &MainWindow::refreshServers);

    connect(m_logoutBtn, &QPushButton::clicked, this, [this]() {
        emit logoutRequested();
    });

    connect(m_vpn, &VpnSession::statusMessage, this, [this](const QString& t) {
        m_status->setText(t);
        appendLog(t);
    });
    connect(m_vpn, &VpnSession::errorMessage, this, [this](const QString& t) {
        m_status->setText(t);
        appendLog(QStringLiteral("Error: ") + t);
        QMessageBox::warning(this, QStringLiteral("DataGate"), t);
        m_connectBtn->setEnabled(true);
        m_disconnectBtn->setEnabled(false);
    });
    connect(m_vpn, &VpnSession::vpnUp, this, [this]() {
        m_connectBtn->setEnabled(false);
        m_disconnectBtn->setEnabled(true);
    });
    connect(m_vpn, &VpnSession::vpnDown, this, [this]() {
        m_connectBtn->setEnabled(true);
        m_disconnectBtn->setEnabled(false);
    });

    loadSettings();

    const QString who = m_session->displayName();
    if (!who.isEmpty()) {
        m_status->setText(QStringLiteral("Signed in as %1.").arg(who));
    }
}

MainWindow::~MainWindow() = default;

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
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.setValue(QStringLiteral("openVpnCmd"), m_openVpnPath->text().trimmed());
    s.setValue(QStringLiteral("themeDark"), m_themeDark->isChecked());
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

void MainWindow::refreshServers()
{
    QString base = AppConfig::apiBaseUrl().trimmed();
    if (base.isEmpty()) {
        QMessageBox::information(
            this,
            QStringLiteral("DataGate"),
            QStringLiteral("Configure Api:BaseUrl in appsettings.json."));
        navigateTo(3);
        return;
    }
    qCInfo(lcUi, "Refresh servers: ensureValidAccessToken");
    if (!m_session->ensureValidAccessToken()) {
        qCWarning(lcUi, "Refresh servers: ensureValidAccessToken failed");
        QMessageBox::warning(
            this,
            QStringLiteral("DataGate"),
            QStringLiteral("Could not refresh the session. Check the network or sign in again (Settings → Logout)."));
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
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_refreshServersBtn->setEnabled(true);
        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            QMessageBox::warning(this, QStringLiteral("DataGate"), err);
            return;
        }
        m_serverList->clear();
        const QByteArray raw = reply->readAll();
        reply->deleteLater();
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            QMessageBox::warning(this, QStringLiteral("DataGate"), QStringLiteral("Invalid JSON."));
            return;
        }
        const QJsonArray arr = doc.object()
            .value(QStringLiteral("data"))
            .toObject()
            .value(QStringLiteral("openVpnServerWithStatuses"))
            .toArray();
        for (const QJsonValue& v : arr) {
            const QJsonObject item = v.toObject();
            const QJsonObject serverObj = item.value(QStringLiteral("openVpnServerResponses"))
                .toObject()
                .value(QStringLiteral("openVpnServer"))
                .toObject();
            const QString name = serverObj.value(QStringLiteral("serverName")).toString();
            const int id = serverObj.value(QStringLiteral("id")).toInt();
            const bool online = serverObj.value(QStringLiteral("isOnline")).toBool();
            const QString line = QStringLiteral("%1 | id=%2 | online=%3")
                .arg(name.isEmpty() ? QStringLiteral("?") : name)
                .arg(id)
                .arg(online ? QStringLiteral("yes") : QStringLiteral("no"));
            m_serverList->addItem(line);
        }
    });
}
