#include "DatagateTranslator.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QTranslator>

namespace {

QTranslator* g_translator = nullptr;

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

    if (g_translator) {
        QCoreApplication::removeTranslator(g_translator);
        delete g_translator;
        g_translator = nullptr;
    }

    if (lang == QStringLiteral("en")) {
        return;
    }

    auto* t = new QTranslator(QCoreApplication::instance());
    const QString base = QCoreApplication::applicationDirPath();
    const QString name = QStringLiteral("datagate_%1").arg(lang);
    const QString pathQm = QDir(base).filePath(QStringLiteral("i18n/%1.qm").arg(name));
    if (t->load(pathQm)) {
        QCoreApplication::installTranslator(t);
        g_translator = t;
    } else {
        delete t;
    }
}

void installFromSettings()
{
    installForLanguage(currentLanguageCode());
}

} // namespace DatagateTranslator
