#pragma once

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcAuth)
Q_DECLARE_LOGGING_CATEGORY(lcOAuth)
Q_DECLARE_LOGGING_CATEGORY(lcUi)

/// Console logging (stderr) when running from CLion/Qt Creator. Enabled when:
/// - Debug / RelWithDebInfo build (DATAGATE_DEBUG_LOG), or
/// - environment variable DATAGATE_LOG=1
void initDatagateLogging();
