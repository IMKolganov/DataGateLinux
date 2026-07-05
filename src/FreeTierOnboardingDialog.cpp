#include "FreeTierOnboardingDialog.h"
#include "AppTheme.h"
#include "DatagateTr.h"

#include <QDesktopServices>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString normalizeApiBase(QString base)
{
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    return base;
}

} // namespace

FreeTierOnboardingDialog::FreeTierOnboardingDialog(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    const DatagateAuth::FreeTierAccessStatus& initialStatus,
    QWidget* parent)
    : QDialog(parent)
    , m_nam(nam)
    , m_apiBase(normalizeApiBase(apiBaseUrl))
    , m_bearer(bearerToken.trimmed())
    , m_status(initialStatus)
{
    setModal(true);
    resize(560, 620);
    setMinimumSize(520, 520);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(12);

    m_title = new QLabel(this);
    QFont tf = m_title->font();
    tf.setPointSize(12);
    tf.setWeight(QFont::DemiBold);
    m_title->setFont(tf);
    root->addWidget(m_title);

    m_lead = new QLabel(this);
    m_lead->setWordWrap(true);
    root->addWidget(m_lead);

    m_planLabel = new QLabel(this);
    m_planLabel->setWordWrap(true);
    m_planLabel->setObjectName(QStringLiteral("muted"));
    root->addWidget(m_planLabel);

    m_channelLabel = new QLabel(this);
    m_channelLabel->setWordWrap(true);
    root->addWidget(m_channelLabel);

    m_openChannelBtn = new QPushButton(this);
    m_openChannelBtn->setProperty("secondary", true);
    root->addWidget(m_openChannelBtn);

    m_linkHeading = new QLabel(this);
    m_linkHeading->setObjectName(QStringLiteral("titleH2"));
    root->addWidget(m_linkHeading);

    m_linkHint = new QLabel(this);
    m_linkHint->setWordWrap(true);
    m_linkHint->setObjectName(QStringLiteral("muted"));
    root->addWidget(m_linkHint);

    m_telegramIdEdit = new QLineEdit(this);
    m_telegramIdEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{0,20}")), this));
    m_telegramIdEdit->setPlaceholderText(QStringLiteral("123456789"));
    root->addWidget(m_telegramIdEdit);

    m_requestCodeBtn = new QPushButton(this);
    m_requestCodeBtn->setProperty("primary", true);
    root->addWidget(m_requestCodeBtn);

    m_codeLabel = new QLabel(this);
    m_codeLabel->setVisible(false);
    m_codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont cf = m_codeLabel->font();
    cf.setPointSize(16);
    cf.setWeight(QFont::DemiBold);
    cf.setFamily(QStringLiteral("monospace"));
    m_codeLabel->setFont(cf);
    m_codeLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_codeLabel);

    m_codeExpiryLabel = new QLabel(this);
    m_codeExpiryLabel->setVisible(false);
    m_codeExpiryLabel->setObjectName(QStringLiteral("muted"));
    m_codeExpiryLabel->setWordWrap(true);
    root->addWidget(m_codeExpiryLabel);

    m_codeInstructions = new QLabel(this);
    m_codeInstructions->setVisible(false);
    m_codeInstructions->setWordWrap(true);
    m_codeInstructions->setObjectName(QStringLiteral("muted"));
    root->addWidget(m_codeInstructions);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setObjectName(QStringLiteral("muted"));
    root->addWidget(m_statusLabel);

    auto* btnRow = new QHBoxLayout();
    m_closeBtn = new QPushButton(this);
    m_closeBtn->setProperty("secondary", true);
    m_refreshBtn = new QPushButton(this);
    m_refreshBtn->setProperty("primary", true);
    btnRow->addWidget(m_closeBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_refreshBtn);
    root->addLayout(btnRow);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(30000);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() { refreshStatus(); });

    m_codeExpiryTimer = new QTimer(this);
    m_codeExpiryTimer->setInterval(1000);
    connect(m_codeExpiryTimer, &QTimer::timeout, this, [this]() {
        if (m_codeSecondsLeft > 0) {
            --m_codeSecondsLeft;
        }
        updateLinkCodeExpiryLabel();
        if (m_codeSecondsLeft <= 0) {
            m_codeExpiryTimer->stop();
            clearLinkCodeDisplay();
        }
    });

    connect(m_openChannelBtn, &QPushButton::clicked, this, [this]() {
        const QUrl url = channelUrlFromHandle(m_status.requiredChannel);
        if (url.isValid()) {
            QDesktopServices::openUrl(url);
        }
    });
    connect(m_requestCodeBtn, &QPushButton::clicked, this, [this]() { requestLinkCode(); });
    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() { refreshStatus(); });
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    retranslateUi();
    applyStatus(m_status);
    m_pollTimer->start();
    AppTheme::centerWidgetOnScreen(this);
}

QUrl FreeTierOnboardingDialog::channelUrlFromHandle(const QString& requiredChannel)
{
    QString handle = requiredChannel.trimmed();
    if (handle.startsWith(QLatin1Char('@'))) {
        handle = handle.mid(1);
    }
    if (handle.isEmpty()) {
        return {};
    }
    return QUrl(QStringLiteral("https://t.me/") + handle);
}

void FreeTierOnboardingDialog::retranslateUi()
{
    setWindowTitle(Datagate::tr("Telegram setup"));
    if (m_title) {
        m_title->setText(Datagate::tr("Complete Telegram setup"));
    }
    if (m_lead) {
        m_lead->setText(Datagate::tr(
            "On the Free and Default plans, VPN configs from the Telegram bot require either a subscription "
            "to the official channel or linking your Telegram account to this sign-in."));
    }
    if (m_linkHeading) {
        m_linkHeading->setText(Datagate::tr("Or link your Telegram account"));
    }
    if (m_linkHint) {
        m_linkHint->setText(Datagate::tr(
            "Open the DataGate Telegram bot (/register if needed), enter your numeric Telegram user ID below, "
            "then enter the link code in the bot with /link_account CODE or by sending the code in private chat."));
    }
    if (m_openChannelBtn) {
        m_openChannelBtn->setText(Datagate::tr("Open Telegram channel"));
    }
    if (m_telegramIdEdit) {
        m_telegramIdEdit->setPlaceholderText(Datagate::tr("Telegram user ID"));
    }
    if (m_requestCodeBtn) {
        m_requestCodeBtn->setText(Datagate::tr("Request link code"));
    }
    if (m_codeInstructions) {
        m_codeInstructions->setText(Datagate::tr(
            "Enter this code in the Telegram bot within the time shown. The code works only for the Telegram ID you entered."));
    }
    if (m_refreshBtn) {
        m_refreshBtn->setText(Datagate::tr("Check again"));
    }
    if (m_closeBtn) {
        m_closeBtn->setText(Datagate::tr("Close"));
    }
}

void FreeTierOnboardingDialog::applyStatus(const DatagateAuth::FreeTierAccessStatus& status)
{
    m_status = status;

    const QString plan = status.activePlanName.trimmed();
    if (plan.isEmpty()) {
        m_planLabel->setText(Datagate::tr("Your plan requires Telegram setup before using VPN configs from the bot."));
    } else {
        m_planLabel->setText(Datagate::tr("Plan: %1 — complete one of the steps below.").arg(plan));
    }

    const QString channel = status.requiredChannel.trimmed();
    if (channel.isEmpty()) {
        m_channelLabel->setText(Datagate::tr("Subscribe to the required Telegram channel, then tap Check again."));
    } else {
        m_channelLabel->setText(Datagate::tr("Required channel: %1").arg(channel));
    }
    m_openChannelBtn->setEnabled(channelUrlFromHandle(channel).isValid());

    const bool canLink = status.canRequestAccountLinkCode;
    m_telegramIdEdit->setEnabled(canLink);
    m_requestCodeBtn->setEnabled(canLink);
    if (!canLink) {
        m_linkHint->setText(Datagate::tr(
            "Account linking is not available for this sign-in. Subscribe to the channel above, or sign in with "
            "Google or email/password and try again."));
    }

    if (status.isGracePeriod) {
        m_statusLabel->setText(Datagate::tr(
            "A grace period may be active in the Telegram bot. Channel subscription or account linking is still recommended."));
    } else {
        m_statusLabel->clear();
    }
}

void FreeTierOnboardingDialog::refreshStatus()
{
    m_statusLabel->clear();
    QString err;
    const DatagateAuth::FreeTierAccessStatus st =
        DatagateAuth::fetchFreeTierAccessStatusSync(m_nam, m_apiBase, m_bearer, &err);
    if (!st.ok) {
        m_statusLabel->setText(err);
        return;
    }
    if (!st.isApplicable || st.isCompliant) {
        accept();
        return;
    }
    applyStatus(st);
}

void FreeTierOnboardingDialog::requestLinkCode()
{
    m_statusLabel->clear();
    const QString rawId = m_telegramIdEdit->text().trimmed();
    if (rawId.isEmpty()) {
        m_statusLabel->setText(Datagate::tr("Enter your numeric Telegram user ID."));
        return;
    }
    bool ok = false;
    const qint64 telegramId = rawId.toLongLong(&ok);
    if (!ok || telegramId <= 0) {
        m_statusLabel->setText(Datagate::tr("Telegram user ID must be a positive number."));
        return;
    }

    QString err;
    const std::optional<DatagateAuth::TelegramAccountLinkCode> link =
        DatagateAuth::postRequestTelegramAccountLinkCodeSync(m_nam, m_apiBase, m_bearer, telegramId, &err);
    if (!link.has_value()) {
        m_statusLabel->setText(err);
        clearLinkCodeDisplay();
        return;
    }

    m_codeLabel->setText(link->code);
    m_codeLabel->setVisible(true);
    m_codeInstructions->setVisible(true);
    m_codeSecondsLeft = link->expiresInSeconds > 0 ? link->expiresInSeconds : 900;
    m_codeExpiryLabel->setVisible(true);
    updateLinkCodeExpiryLabel();
    m_codeExpiryTimer->start();
    m_statusLabel->setText(Datagate::tr("Link code issued. Enter it in the Telegram bot, then tap Check again."));
}

void FreeTierOnboardingDialog::clearLinkCodeDisplay()
{
    m_codeLabel->clear();
    m_codeLabel->setVisible(false);
    m_codeExpiryLabel->clear();
    m_codeExpiryLabel->setVisible(false);
    m_codeInstructions->setVisible(false);
    m_codeSecondsLeft = 0;
    if (m_codeExpiryTimer) {
        m_codeExpiryTimer->stop();
    }
}

void FreeTierOnboardingDialog::updateLinkCodeExpiryLabel()
{
    if (m_codeSecondsLeft <= 0) {
        m_codeExpiryLabel->setText(Datagate::tr("Link code expired. Request a new code."));
        return;
    }
    const int minutes = m_codeSecondsLeft / 60;
    const int seconds = m_codeSecondsLeft % 60;
    m_codeExpiryLabel->setText(
        Datagate::tr("Expires in %1:%2").arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0')));
}

void FreeTierOnboardingDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        applyStatus(m_status);
        updateLinkCodeExpiryLabel();
    }
    QDialog::changeEvent(event);
}
