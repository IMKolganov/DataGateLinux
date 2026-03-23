#include "AppConfig.h"
#include "AppLogging.h"
#include "AuthSession.h"
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

int main(int argc, char* argv[])
{
    initDatagateLogging();

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("DataGate"));
    QCoreApplication::setApplicationName(QStringLiteral("DataGateLinux"));

    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale::system(), QStringLiteral("qt"), QStringLiteral("_"),
            QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    qCInfo(lcUi, "DataGateLinux starting");

    if (!AppConfig::load()) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("DataGate"),
            QStringLiteral("appsettings.json not found (cwd, folder with exe, or AppConfig path), "
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
