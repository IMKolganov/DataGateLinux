#include "AppConfig.h"
#include "AppLogging.h"
#include "GitHubReleaseChecker.h"
#include "DatagateUtils.h"
#include "AuthSession.h"
#include "DatagateTr.h"
#include "DatagateTranslator.h"
#include "LoginDialog.h"
#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QTimer>
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

#if defined(__linux__) && defined(DATAGATE_EMBEDDED_OPENVPN3) && !defined(DATAGATE_EMBEDDED_OPENVPN3_USE_EXTERNAL_HELPER)
    DatagateUtils::linuxTryRaiseEffectiveCapNetAdmin();
    {
        const int tracer = DatagateUtils::linuxProcSelfTracerPid();
        if (tracer > 0) {
            qCWarning(lcUi,
                "TracerPid=%lld: ptrace active — setcap on this binary is ignored. Use a system terminal outside the "
                "IDE, or OpenVpn.UseSystemBinary=true + setcap on openvpn (see appsettings.example.json).",
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

        if (AppConfig::updateCheckOnStartup()) {
            const QString ghOwner = AppConfig::updateGithubOwner();
            const QString ghRepo = AppConfig::updateGithubRepo();
            if (!ghOwner.isEmpty() && !ghRepo.isEmpty()) {
                MainWindow* mw = mainWin.get();
                QTimer::singleShot(1500, mw, [mw, &nam, ghOwner, ghRepo]() {
                    auto* checker = new GitHubReleaseChecker(&nam, mw);
                    QObject::connect(
                        checker,
                        &GitHubReleaseChecker::finished,
                        mw,
                        [checker, mw](bool newer, const QString& tag, const QUrl& url) {
                            checker->deleteLater();
                            if (!newer || !url.isValid()) {
                                return;
                            }
                            const int r = QMessageBox::question(
                                mw,
                                Datagate::tr("Update available"),
                                Datagate::tr("A new version (%1) is available. Open the release page?").arg(tag),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::Yes);
                            if (r == QMessageBox::Yes) {
                                QDesktopServices::openUrl(url);
                            }
                        });
                    checker->start(ghOwner, ghRepo);
                });
            }
        }
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
