#include "LoginDialog.h"

#include "AppConfig.h"
#include "DatagateAuthLogin.h"
#include "DatagateTr.h"
#include "AppLogging.h"
#include "AppTheme.h"
#include "AuthSession.h"
#include "DatagateTranslator.h"
#include "GoogleAuthHelper.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSettings>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

LoginDialog::LoginDialog(AuthSession* session, QNetworkAccessManager* nam, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_google(new GoogleAuthHelper(nam, this))
    , m_nam(nam)
{
    setModal(true);
    resize(440, 360);
    setMinimumSize(440, 320);

    {
        QSettings s;
        AppTheme::apply(s.value(QStringLiteral("themeDark"), true).toBool());
    }

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(14);

    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack, 1);

    // ----- Sign-in page
    m_signInPage = new QWidget(this);
    auto* signLay = new QVBoxLayout(m_signInPage);
    signLay->setContentsMargins(0, 0, 0, 0);
    signLay->setSpacing(14);

    m_title = new QLabel(m_signInPage);
    QFont tf = m_title->font();
    tf.setPointSize(12);
    tf.setWeight(QFont::DemiBold);
    m_title->setFont(tf);
    signLay->addWidget(m_title);

    m_hint = new QLabel(m_signInPage);
    m_hint->setWordWrap(true);
    signLay->addWidget(m_hint);

    auto* langRow = new QHBoxLayout();
    m_langLabel = new QLabel(m_signInPage);
    m_languageCombo = new QComboBox(m_signInPage);
    m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_languageCombo->addItem(QStringLiteral("Русский"), QStringLiteral("ru"));
    m_languageCombo->addItem(QStringLiteral("Français"), QStringLiteral("fr"));
    m_languageCombo->addItem(QStringLiteral("Ελληνικά"), QStringLiteral("el"));
    langRow->addWidget(m_langLabel);
    langRow->addWidget(m_languageCombo, 1);
    signLay->addLayout(langRow);

    m_status = new QLabel(m_signInPage);
    m_status->setWordWrap(true);
    signLay->addWidget(m_status);

    auto* row = new QHBoxLayout();
    row->addStretch();
    m_cancel = new QPushButton(m_signInPage);
    m_cancel->setProperty("secondary", true);
    m_signIn = new QPushButton(m_signInPage);
    m_signIn->setProperty("primary", true);
    row->addWidget(m_cancel);
    row->addWidget(m_signIn);
    signLay->addLayout(row);

    m_stack->addWidget(m_signInPage);

    // ----- TOTP challenge page
    m_totpPage = new QWidget(this);
    auto* totpLay = new QVBoxLayout(m_totpPage);
    totpLay->setContentsMargins(0, 0, 0, 0);
    totpLay->setSpacing(12);

    m_totpLead = new QLabel(m_totpPage);
    m_totpLead->setWordWrap(true);
    totpLay->addWidget(m_totpLead);

    m_totpCode = new QLineEdit(m_totpPage);
    m_totpCode->setMaxLength(8);
    m_totpCode->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{0,8}")), m_totpCode));
    m_totpCode->setPlaceholderText(QStringLiteral("000000"));
    totpLay->addWidget(m_totpCode);

    m_totpStatus = new QLabel(m_totpPage);
    m_totpStatus->setWordWrap(true);
    m_totpStatus->setObjectName(QStringLiteral("muted"));
    totpLay->addWidget(m_totpStatus);

    auto* totpBtnRow = new QHBoxLayout();
    m_totpBack = new QPushButton(m_totpPage);
    m_totpBack->setProperty("secondary", true);
    m_totpVerify = new QPushButton(m_totpPage);
    m_totpVerify->setProperty("primary", true);
    totpBtnRow->addWidget(m_totpBack);
    totpBtnRow->addStretch();
    totpBtnRow->addWidget(m_totpVerify);
    totpLay->addLayout(totpBtnRow);

    m_stack->addWidget(m_totpPage);

    const QString lang = DatagateTranslator::currentLanguageCode();
    m_languageCombo->blockSignals(true);
    const int lix = m_languageCombo->findData(lang);
    m_languageCombo->setCurrentIndex(lix >= 0 ? lix : 0);
    m_languageCombo->blockSignals(false);
    DatagateTranslator::installForLanguage(lang);

    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString code = m_languageCombo->currentData().toString();
        DatagateTranslator::setLanguageCode(code);
        DatagateTranslator::installForLanguage(code);
        retranslateUi();
    });

    connect(m_cancel, &QPushButton::clicked, this, [this]() {
        m_google->cancelOAuth();
        reject();
    });
    connect(m_signIn, &QPushButton::clicked, this, [this]() {
        const QString api = AppConfig::apiBaseUrl().trimmed();
        const QString cid = AppConfig::googleClientId().trimmed();
        if (api.isEmpty() || cid.isEmpty()) {
            QMessageBox::warning(
                this,
                Datagate::tr("DataGate"),
                Datagate::tr("Configure Api:BaseUrl and GoogleAuth:ClientId in appsettings.json."));
            return;
        }
        m_signIn->setEnabled(false);
        m_status->setText(Datagate::tr("Waiting for browser sign-in…"));
        m_google->signInWithGoogle(cid, AppConfig::googleRedirectPort(), api);
    });
    connect(m_google, &GoogleAuthHelper::finishedSuccess, this, [this](const QJsonObject& data) {
        m_signIn->setEnabled(true);
        handleLoginData(data);
    });
    connect(m_google, &GoogleAuthHelper::finishedError, this, [this](const QString& err) {
        m_signIn->setEnabled(true);
        showSignInPage();
        const QString silent = QStringLiteral("[silent]");
        if (err.startsWith(silent)) {
            m_status->setText(err.mid(silent.length()));
            return;
        }
        m_status->setText(err);
        QMessageBox::warning(this, Datagate::tr("DataGate"), err);
    });

    connect(m_totpBack, &QPushButton::clicked, this, [this]() {
        m_loginChallengeId.clear();
        m_totpCode->clear();
        m_totpStatus->clear();
        showSignInPage();
    });
    connect(m_totpVerify, &QPushButton::clicked, this, [this]() {
        const QString code = m_totpCode->text().trimmed();
        if (code.size() < 6) {
            m_totpStatus->setText(Datagate::tr("Enter the 6-digit code from your authenticator app."));
            return;
        }
        if (m_loginChallengeId.isEmpty()) {
            showSignInPage();
            m_status->setText(Datagate::tr("Sign in again — the verification step expired."));
            return;
        }
        m_totpVerify->setEnabled(false);
        m_totpStatus->setText(Datagate::tr("Verifying…"));
        QString err;
        const std::optional<QJsonObject> data = DatagateAuth::postTotpVerifyLoginSync(
            m_nam, AppConfig::apiBaseUrl(), m_loginChallengeId, code, &err);
        m_totpVerify->setEnabled(true);
        if (!data.has_value()) {
            if (DatagateAuth::isLoginChallengeExpiredMessage(err)) {
                m_loginChallengeId.clear();
                showSignInPage();
                m_status->setText(
                    Datagate::tr("This verification step expired. Sign in again — you are not logged in yet."));
            } else {
                m_totpStatus->setText(err);
            }
            return;
        }
        handleLoginData(*data);
    });

    retranslateUi();
    showSignInPage();
}

void LoginDialog::handleLoginData(const QJsonObject& data)
{
    const DatagateAuth::LoginFlow flow = DatagateAuth::resolveLoginFlow(data);
    if (flow.kind == DatagateAuth::LoginFlowKind::TotpChallenge) {
        showTotpPage(flow.displayName);
        m_loginChallengeId = flow.loginChallengeId;
        m_totpCode->clear();
        m_totpStatus->clear();
        m_totpCode->setFocus();
        return;
    }

    const QString token = flow.payload.value(QStringLiteral("token")).toString();
    if (token.isEmpty()) {
        const QString alt = flow.payload.value(QStringLiteral("Token")).toString();
        if (alt.isEmpty()) {
            showSignInPage();
            m_status->setText(Datagate::tr("No token in API response."));
            QMessageBox::warning(this, Datagate::tr("DataGate"), m_status->text());
            return;
        }
    }

    m_session->setFromLoginJson(flow.payload);
    qCInfo(lcUi, "Login: sign-in succeeded (setupRequired=%d)",
        flow.kind == DatagateAuth::LoginFlowKind::SetupRequired ? 1 : 0);
    accept();
}

void LoginDialog::showSignInPage()
{
    if (m_stack) {
        m_stack->setCurrentWidget(m_signInPage);
    }
}

void LoginDialog::showTotpPage(const QString& displayName)
{
    if (m_stack) {
        m_stack->setCurrentWidget(m_totpPage);
    }
    if (m_totpLead) {
        if (!displayName.trimmed().isEmpty()) {
            m_totpLead->setText(
                Datagate::tr("Signed in as %1. Enter the 6-digit code from your authenticator app to finish signing in.")
                    .arg(displayName.trimmed()));
        } else {
            m_totpLead->setText(
                Datagate::tr("Enter the 6-digit code from your authenticator app to finish signing in."));
        }
    }
}

void LoginDialog::retranslateUi()
{
    setWindowTitle(Datagate::tr("DataGate — Sign in"));
    if (m_title) {
        m_title->setText(Datagate::tr("Welcome to DataGate"));
    }
    if (m_hint) {
        m_hint->setText(Datagate::tr("Sign in with your Google account to continue."));
    }
    if (m_langLabel) {
        m_langLabel->setText(Datagate::tr("Language"));
    }
    if (m_cancel) {
        m_cancel->setText(Datagate::tr("Cancel"));
    }
    if (m_signIn) {
        m_signIn->setText(Datagate::tr("Sign in with Google"));
    }
    if (m_totpBack) {
        m_totpBack->setText(Datagate::tr("Back to sign in"));
    }
    if (m_totpVerify) {
        m_totpVerify->setText(Datagate::tr("Verify and sign in"));
    }
}

void LoginDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void LoginDialog::showEvent(QShowEvent* e)
{
    QDialog::showEvent(e);
    if (m_centerOnFirstShow) {
        m_centerOnFirstShow = false;
        AppTheme::centerWidgetOnScreen(this);
    }
}

void LoginDialog::closeEvent(QCloseEvent* e)
{
    m_google->cancelOAuth();
    QDialog::closeEvent(e);
}
