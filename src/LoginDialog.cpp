#include "LoginDialog.h"

#include "AppConfig.h"
#include "AppLogging.h"
#include "AppTheme.h"
#include "AuthSession.h"
#include "GoogleAuthHelper.h"

#include <QCloseEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

LoginDialog::LoginDialog(AuthSession* session, QNetworkAccessManager* nam, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_google(new GoogleAuthHelper(nam, this))
{
    setWindowTitle(QStringLiteral("DataGate — Sign in"));
    setModal(true);
    resize(420, 220);
    setFixedSize(420, 220);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("Welcome to DataGate"), this);
    QFont tf = title->font();
    tf.setPointSize(12);
    tf.setWeight(QFont::DemiBold);
    title->setFont(tf);
    root->addWidget(title);

    auto* hint = new QLabel(
        QStringLiteral("Sign in with your Google account to continue."),
        this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto* row = new QHBoxLayout();
    row->addStretch();
    m_cancel = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancel->setProperty("secondary", true);
    m_signIn = new QPushButton(QStringLiteral("Sign in with Google"), this);
    m_signIn->setProperty("primary", true);
    row->addWidget(m_cancel);
    row->addWidget(m_signIn);
    root->addLayout(row);

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
                QStringLiteral("DataGate"),
                QStringLiteral("Configure Api:BaseUrl and GoogleAuth:ClientId in appsettings.json."));
            return;
        }
        m_signIn->setEnabled(false);
        m_status->setText(QStringLiteral("Waiting for browser sign-in…"));
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
        QMessageBox::warning(this, QStringLiteral("DataGate"), err);
    });
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
