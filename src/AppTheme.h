#pragma once

class QApplication;
class QWidget;

namespace AppTheme {

void apply(bool dark);
/// Center on the available screen geometry (first show).
void centerWidgetOnScreen(QWidget* w);

} // namespace AppTheme
