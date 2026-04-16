#include "AppConfig.h"
#include "AppLogging.h"
#include "DatagateUtils.h"
#include "AuthSession.h"
#include "DatagateTr.h"
#include "DatagateTranslator.h"
#include "LoginDialog.h"
#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QTranslator>

#include <functional>
#include <memory>

#ifndef DATAGATE_APP_VERSION_STR
#define DATAGATE_APP_VERSION_STR "0.0.0-dev"
#endif

int main(int argc, char* argv[])
{
    initDatagateLogging();

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("DataGate"));
    QCoreApplication::setApplicationName(QStringLiteral("DataGateLinux"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(DATAGATE_APP_VERSION_STR));

    DatagateTranslator::installFromSettings();

    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale::system(), QStringLiteral("qt"), QStringLiteral("_"),
            QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    qCInfo(lcUi, "DataGateLinux starting");

#if defined(__linux__) && defined(DATAGATE_EMBEDDED_OPENVPN3)
    DatagateUtils::linuxTryRaiseEffectiveCapNetAdmin();
    {
        const int tracer = DatagateUtils::linuxProcSelfTracerPid();
        if (tracer > 0) {
            qCWarning(lcUi,
                "TracerPid=%lld: process is ptraced (debugger). File capabilities (setcap) usually do not apply — "
                "run without debugging or from a terminal.",
                static_cast<long long>(tracer));
        }
    }
#endif

    if (!AppConfig::load()) {
        QMessageBox::critical(
            nullptr,
            Datagate::tr("DataGate"),
            Datagate::tr("appsettings.json not found (cwd, folder with exe, or AppConfig path), "
                         "or Api:BaseUrl / GoogleAuth:ClientId are empty.\n"
                         "See stderr log line \"AppConfig: loaded …\" when DATAGATE_LOG=1 or Debug build."));
        return 1;
    }

    QNetworkAccessManager nam;
    AuthSession session(&nam);
    session.initialize();

    std::unique_ptr<MainWindow> mainWin;

    std::function<void()> showMain;
    showMain = [&]() {
        mainWin = std::make_unique<MainWindow>(&session);
        QObject::connect(mainWin.get(), &MainWindow::logoutRequested, [&]() {
            session.logout();
            mainWin->close();
            mainWin.reset();
            LoginDialog dlg(&session, &nam);
            if (dlg.exec() != QDialog::Accepted) {
                QCoreApplication::quit();
                return;
            }
            showMain();
        });
        mainWin->show();
    };

    if (!session.ensureValidAccessToken()) {
        LoginDialog dlg(&session, &nam);
        if (dlg.exec() != QDialog::Accepted) {
            return 0;
        }
    }

    showMain();

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&mainWin]() {
        if (mainWin) {
            mainWin->shutdownVpn();
        }
    });

    return app.exec();
}
