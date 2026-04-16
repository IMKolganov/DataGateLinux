#include "DatagateTranslator.h"

#include "AppLogging.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QIODevice>
#include <QSettings>
#include <QString>
#include <QTranslator>
#include <QXmlStreamReader>

namespace {

QTranslator* g_translator = nullptr;

/// When .qm is missing, map source → translation from embedded :/ts/datagate_<lang>.ts
QHash<QString, QString> g_tsFallback;
bool g_useTsFallback = false;

QString normalizeLang(const QString& code)
{
    const QString c = code.trimmed().toLower();
    if (c == QStringLiteral("ru") || c.startsWith(QStringLiteral("ru"))) {
        return QStringLiteral("ru");
    }
    if (c == QStringLiteral("fr") || c.startsWith(QStringLiteral("fr"))) {
        return QStringLiteral("fr");
    }
    if (c == QStringLiteral("el") || c.startsWith(QStringLiteral("el"))
        || c == QStringLiteral("gr")) {
        return QStringLiteral("el");
    }
    return QStringLiteral("en");
}

bool loadDatagateTsIntoMap(QIODevice* dev, QHash<QString, QString>* out)
{
    if (!dev || !out) {
        return false;
    }
    QXmlStreamReader xml(dev);
    bool inDatagateContext = false;
    QString pendingSource;
    int contextDepth = 0;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const auto n = xml.name();
            if (n == QStringLiteral("context")) {
                ++contextDepth;
                inDatagateContext = false;
            } else if (n == QStringLiteral("name") && contextDepth > 0) {
                const QString t = xml.readElementText();
                inDatagateContext = (t == QStringLiteral("Datagate"));
            } else if (n == QStringLiteral("source") && inDatagateContext) {
                pendingSource = xml.readElementText();
            } else if (n == QStringLiteral("translation") && inDatagateContext) {
                const QString trType = xml.attributes().value(QStringLiteral("type")).toString();
                const QString trText = xml.readElementText();
                if (!pendingSource.isEmpty() && trType != QStringLiteral("vanished")
                    && trType != QStringLiteral("obsolete") && !trText.isEmpty()) {
                    (*out)[pendingSource] = trText;
                }
                pendingSource.clear();
            }
        } else if (xml.isEndElement() && xml.name() == QStringLiteral("context")) {
            if (contextDepth > 0) {
                --contextDepth;
            }
            inDatagateContext = false;
            pendingSource.clear();
        }
    }
    if (xml.hasError()) {
        return false;
    }
    return !out->isEmpty();
}

bool tryLoadTsFallback(const QString& lang, QHash<QString, QString>* out)
{
    const QString res = QStringLiteral(":/ts/datagate_%1.ts").arg(lang);
    QFile f(res);
    if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    out->clear();
    if (!loadDatagateTsIntoMap(&f, out)) {
        return false;
    }
    qCInfo(lcUi, "Using embedded .ts fallback for \"%s\" (%d strings)", qPrintable(lang),
        static_cast<int>(out->size()));
    return true;
}

} // namespace

namespace DatagateTranslator {

QString currentLanguageCode()
{
    QSettings s;
    return normalizeLang(s.value(QStringLiteral("General/language"), QStringLiteral("en")).toString());
}

void setLanguageCode(const QString& code)
{
    QSettings s;
    s.setValue(QStringLiteral("General/language"), normalizeLang(code));
}

void installForLanguage(const QString& code)
{
    const QString lang = normalizeLang(code);

    g_tsFallback.clear();
    g_useTsFallback = false;

    if (g_translator) {
        QCoreApplication::removeTranslator(g_translator);
        delete g_translator;
        g_translator = nullptr;
    }

    if (lang == QStringLiteral("en")) {
        return;
    }

    auto* t = new QTranslator(QCoreApplication::instance());
    const QString fileBase = QStringLiteral("datagate_%1").arg(lang);

    const QString resPath = QStringLiteral(":/i18n/%1.qm").arg(fileBase);
    const QString diskPath
        = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("i18n/%1.qm").arg(fileBase));

    bool loaded = false;
    if (QFile::exists(resPath) && t->load(resPath)) {
        loaded = true;
        qCDebug(lcUi, "Loaded translator from resources: %s", qPrintable(resPath));
    } else if (QFile::exists(diskPath) && t->load(diskPath)) {
        loaded = true;
        qCDebug(lcUi, "Loaded translator from disk: %s", qPrintable(diskPath));
    }

    if (loaded) {
        QCoreApplication::installTranslator(t);
        g_translator = t;
        return;
    }

    delete t;

    if (tryLoadTsFallback(lang, &g_tsFallback)) {
        g_useTsFallback = true;
        return;
    }

    qCWarning(lcUi,
        "No translation for \"%s\" (.qm missing; embedded .ts missing or invalid). "
        "Install qt6-l10n-tools and rebuild, or keep i18n/datagate_*.ts in resources.",
        qPrintable(lang));
}

void installFromSettings()
{
    installForLanguage(currentLanguageCode());
}

QString translateUi(const char* sourceUtf8)
{
    const QString key = QString::fromUtf8(sourceUtf8);
    if (g_useTsFallback) {
        const auto it = g_tsFallback.constFind(key);
        if (it != g_tsFallback.constEnd()) {
            return *it;
        }
    }
    return QCoreApplication::translate("Datagate", sourceUtf8, nullptr);
}

} // namespace DatagateTranslator
