#include "AppLogging.h"

#include <QDateTime>
#include <QLoggingCategory>

#include <cstdio>
#include <cstdlib>

#include <QtGlobal>

Q_LOGGING_CATEGORY(lcAuth, "datagate.auth")
Q_LOGGING_CATEGORY(lcOAuth, "datagate.oauth")
Q_LOGGING_CATEGORY(lcUi, "datagate.ui")

static void datagateMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    const QByteArray local = msg.toLocal8Bit();
    const char* typeStr = "DEBUG";
    switch (type) {
    case QtDebugMsg:
        typeStr = "DEBUG";
        break;
    case QtInfoMsg:
        typeStr = "INFO";
        break;
    case QtWarningMsg:
        typeStr = "WARN";
        break;
    case QtCriticalMsg:
        typeStr = "CRITICAL";
        break;
    case QtFatalMsg:
        typeStr = "FATAL";
        break;
    default:
        break;
    }
    fprintf(stderr,
        "%s [%s] %s: %s\n",
        QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toUtf8().constData(),
        typeStr,
        ctx.category ? ctx.category : "qt",
        local.constData());
    fflush(stderr);
    if (type == QtFatalMsg) {
        abort();
    }
}

void initDatagateLogging()
{
#if defined(DATAGATE_DEBUG_LOG)
    const bool verbose = true;
#else
    const bool verbose = qEnvironmentVariableIntValue("DATAGATE_LOG") != 0;
#endif
    if (verbose) {
        QLoggingCategory::setFilterRules(QStringLiteral("datagate.*.debug=true\ndatagate.*.info=true"));
    }
    qInstallMessageHandler(datagateMessageHandler);
    if (verbose) {
        qCInfo(lcUi, "datagate verbose log: DATAGATE_DEBUG_LOG or DATAGATE_LOG=1");
    }
}
