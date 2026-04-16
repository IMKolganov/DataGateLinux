#include "DatagateTr.h"

#include "DatagateTranslator.h"

namespace Datagate {

QString tr(const char* sourceUtf8)
{
    return DatagateTranslator::translateUi(sourceUtf8);
}

} // namespace Datagate
