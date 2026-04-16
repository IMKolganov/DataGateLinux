#pragma once

#include <QString>

namespace DatagateTranslator {

/// en | ru | fr | el — persisted as General/language in QSettings.
QString currentLanguageCode();

void setLanguageCode(const QString& code);

/// Load datagate_<code>.qm from applicationDir/i18n/ (call after QApplication exists).
void installFromSettings();

void installForLanguage(const QString& code);

} // namespace DatagateTranslator
