#pragma once

#include <QCoreApplication>
#include <QString>

/// UI strings for lupdate/lrelease (context "Datagate").
namespace Datagate {
inline QString tr(const char* sourceUtf8)
{
    return QCoreApplication::translate("Datagate", sourceUtf8, nullptr);
}
} // namespace Datagate
