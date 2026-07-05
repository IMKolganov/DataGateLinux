#include "AdminTotpSetupDialog.h"
#include "DatagateAuthLogin.h"
#include "DatagateTr.h"
#include "AppTheme.h"

#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QTextEdit>
#include <QVBoxLayout>

AdminTotpSetupDialog::AdminTotpSetupDialog(QNetworkAccessManager* nam,
    const QString& apiBaseUrl,
    const QString& bearerToken,
    QWidget* parent)
    : QDialog(parent)
    , m_nam(nam)
    , m_apiBase(apiBaseUrl)
    , m_bearer(bearerToken)
{
    setModal(true);
    setWindowFlag(Qt::WindowCloseButtonHint, false);
    resize(520, 520);
    setMinimumSize(480, 420);

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

    m_required = new QLabel(this);
    m_required->setWordWrap(true);
    m_required->setObjectName(QStringLiteral("muted"));
    root->addWidget(m_required);

    m_secretView = new QTextEdit(this);
    m_secretView->setReadOnly(true);
    m_secretView->setVisible(false);
    m_secretView->setMaximumHeight(120);
    root->addWidget(m_secretView);

    m_confirmCode = new QLineEdit(this);
    m_confirmCode->setMaxLength(8);
    m_confirmCode->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{0,8}")), this));
    m_confirmCode->setPlaceholderText(QStringLiteral("000000"));
    m_confirmCode->setVisible(false);
    root->addWidget(m_confirmCode);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setObjectName(QStringLiteral("muted"));
    root->addWidget(m_status);

    auto* btnRow = new QHBoxLayout();
    m_logoutBtn = new QPushButton(this);
    m_logoutBtn->setProperty("secondary", true);
    m_beginBtn = new QPushButton(this);
    m_beginBtn->setProperty("primary", true);
    m_confirmBtn = new QPushButton(this);
    m_confirmBtn->setProperty("primary", true);
    m_confirmBtn->setVisible(false);
    btnRow->addWidget(m_logoutBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_beginBtn);
    btnRow->addWidget(m_confirmBtn);
    root->addLayout(btnRow);

    connect(m_logoutBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_beginBtn, &QPushButton::clicked, this, [this]() {
        m_status->clear();
        QString err;
        const std::optional<DatagateAuth::TotpSetupInfo> setup =
            DatagateAuth::postTotpSetupSync(m_nam, m_apiBase, m_bearer, &err);
        if (!setup.has_value()) {
            m_status->setText(err);
            return;
        }
        showSetupInfo(setup->sharedSecret, setup->otpAuthUri);
        m_status->setText(Datagate::tr("Scan the URI or enter the secret in your authenticator app, then enter the 6-digit code."));
    });
    connect(m_confirmBtn, &QPushButton::clicked, this, [this]() {
        const QString code = m_confirmCode->text().trimmed();
        if (code.size() < 6) {
            m_status->setText(Datagate::tr("Enter the 6-digit code from your authenticator app."));
            return;
        }
        m_status->clear();
        QString err;
        if (!DatagateAuth::postTotpConfirmSync(m_nam, m_apiBase, m_bearer, code, &err)) {
            m_status->setText(err);
            return;
        }
        accept();
    });

    retranslateUi();
    AppTheme::centerWidgetOnScreen(this);
}

void AdminTotpSetupDialog::retranslateUi()
{
    setWindowTitle(Datagate::tr("Admin security"));
    if (m_title) {
        m_title->setText(Datagate::tr("Two-factor authentication"));
    }
    if (m_lead) {
        m_lead->setText(
            Datagate::tr("Administrators must use an authenticator app (Google Authenticator, Authy, 1Password, etc.) "
                         "for sign-in. Set it up now to continue."));
    }
    if (m_required) {
        m_required->setText(Datagate::tr("You must enable two-factor authentication before using DataGate."));
    }
    if (m_logoutBtn) {
        m_logoutBtn->setText(Datagate::tr("Sign out"));
    }
    if (m_beginBtn) {
        m_beginBtn->setText(Datagate::tr("Begin setup"));
    }
    if (m_confirmBtn) {
        m_confirmBtn->setText(Datagate::tr("Confirm and continue"));
    }
}

void AdminTotpSetupDialog::showSetupInfo(const QString& sharedSecret, const QString& otpAuthUri)
{
    QString text;
    if (!otpAuthUri.isEmpty()) {
        text += Datagate::tr("Setup URI:") + QLatin1Char('\n') + otpAuthUri + QLatin1Char('\n') + QLatin1Char('\n');
    }
    if (!sharedSecret.isEmpty()) {
        text += Datagate::tr("Manual secret:") + QLatin1Char('\n') + sharedSecret;
    }
    m_secretView->setPlainText(text);
    m_secretView->setVisible(true);
    m_confirmCode->setVisible(true);
    m_confirmBtn->setVisible(true);
    m_beginBtn->setVisible(false);
    m_confirmCode->setFocus();
}

void AdminTotpSetupDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}
