#include "LoginDialog.h"

#include "AppConfig.h"
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
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QSettings>
#include <QShowEvent>
#include <QVBoxLayout>

LoginDialog::LoginDialog(AuthSession* session, QNetworkAccessManager* nam, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_google(new GoogleAuthHelper(nam, this))
{
    setModal(true);
    resize(440, 300);
    setFixedSize(440, 300);

    {
        QSettings s;
        AppTheme::apply(s.value(QStringLiteral("themeDark"), true).toBool());
    }

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(14);

    m_title = new QLabel(this);
    QFont tf = m_title->font();
    tf.setPointSize(12);
    tf.setWeight(QFont::DemiBold);
    m_title->setFont(tf);
    root->addWidget(m_title);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    root->addWidget(m_hint);

    auto* langRow = new QHBoxLayout();
    m_langLabel = new QLabel(this);
    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_languageCombo->addItem(QStringLiteral("Русский"), QStringLiteral("ru"));
    m_languageCombo->addItem(QStringLiteral("Français"), QStringLiteral("fr"));
    m_languageCombo->addItem(QStringLiteral("Ελληνικά"), QStringLiteral("el"));
    langRow->addWidget(m_langLabel);
    langRow->addWidget(m_languageCombo, 1);
    root->addLayout(langRow);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto* row = new QHBoxLayout();
    row->addStretch();
    m_cancel = new QPushButton(this);
    m_cancel->setProperty("secondary", true);
    m_signIn = new QPushButton(this);
    m_signIn->setProperty("primary", true);
    row->addWidget(m_cancel);
    row->addWidget(m_signIn);
    root->addLayout(row);

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
        m_session->setFromLoginJson(data);
        m_signIn->setEnabled(true);
        qCInfo(lcUi, "Login: Google sign-in succeeded");
        accept();
    });
    connect(m_google, &GoogleAuthHelper::finishedError, this, [this](const QString& err) {
        m_signIn->setEnabled(true);
        const QString silent = QStringLiteral("[silent]");
        if (err.startsWith(silent)) {
            m_status->setText(err.mid(silent.length()));
            return;
        }
        m_status->setText(err);
        QMessageBox::warning(this, Datagate::tr("DataGate"), err);
    });

    retranslateUi();
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
