#include "AppTheme.h"

#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QScreen>
#include <QStyleFactory>
#include <QWidget>

namespace AppTheme {

void apply(bool dark)
{
    if (!qApp) {
        return;
    }

    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette pal;
    if (dark) {
        pal.setColor(QPalette::Window, QColor(QStringLiteral("#1E1E1E")));
        pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#E6E6E6")));
        pal.setColor(QPalette::Base, QColor(QStringLiteral("#2D2D2D")));
        pal.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#252526")));
        pal.setColor(QPalette::Text, QColor(QStringLiteral("#E6E6E6")));
        pal.setColor(QPalette::Button, QColor(QStringLiteral("#3C3C3C")));
        pal.setColor(QPalette::ButtonText, QColor(QStringLiteral("#E6E6E6")));
        pal.setColor(QPalette::Highlight, QColor(QStringLiteral("#0078D4")));
        pal.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#FFFFFF")));
        pal.setColor(QPalette::Link, QColor(QStringLiteral("#60CDFF")));
        pal.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#9E9E9E")));
    } else {
        pal.setColor(QPalette::Window, QColor(QStringLiteral("#F3F3F3")));
        pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#1A1A1A")));
        pal.setColor(QPalette::Base, QColor(QStringLiteral("#FFFFFF")));
        pal.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#FAFAFA")));
        pal.setColor(QPalette::Text, QColor(QStringLiteral("#1A1A1A")));
        pal.setColor(QPalette::Button, QColor(QStringLiteral("#E8E8E8")));
        pal.setColor(QPalette::ButtonText, QColor(QStringLiteral("#1A1A1A")));
        pal.setColor(QPalette::Highlight, QColor(QStringLiteral("#0078D4")));
        pal.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#FFFFFF")));
        pal.setColor(QPalette::Link, QColor(QStringLiteral("#0067C0")));
        pal.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#6E6E6E")));
    }
    qApp->setPalette(pal);

    const QString qss = dark ? QStringLiteral(R"(
QMainWindow, QWidget#centralRoot { background-color: #1E1E1E; }
QFrame#card {
  background-color: #2D2D2D;
  border: 1px solid #3F3F3F;
  border-radius: 8px;
}
QLabel#titleH1 { color: #E6E6E6; font-size: 20px; font-weight: 600; }
QLabel#titleH2 { color: #E6E6E6; font-size: 14px; font-weight: 600; }
QLabel#muted { color: #A0A0A0; font-size: 13px; }
QListWidget#navPane {
  background-color: #252526;
  border: none;
  border-right: 1px solid #3F3F3F;
  outline: none;
  padding: 8px 0;
  font-size: 14px;
}
QListWidget#navPane::item {
  padding: 10px 16px;
  margin: 2px 8px;
  border-radius: 4px;
  color: #E6E6E6;
}
QListWidget#navPane::item:selected {
  background-color: #3E3E42;
}
QListWidget#navPane::item:hover:!selected {
  background-color: #2D2D30;
}
QPushButton[primary="true"] {
  background-color: #0078D4;
  color: #FFFFFF;
  border: none;
  border-radius: 4px;
  padding: 8px 20px;
  min-width: 140px;
  font-weight: 600;
}
QPushButton[primary="true"]:hover { background-color: #1A86D8; }
QPushButton[primary="true"]:pressed { background-color: #006CBD; }
QPushButton[secondary="true"] {
  background-color: #3C3C3C;
  color: #E6E6E6;
  border: 1px solid #5C5C5C;
  border-radius: 4px;
  padding: 8px 20px;
  min-width: 140px;
}
QPushButton[secondary="true"]:hover { background-color: #4A4A4A; }
QLineEdit, QPlainTextEdit, QSpinBox {
  background-color: #2D2D2D;
  border: 1px solid #3F3F3F;
  border-radius: 4px;
  padding: 6px 8px;
  selection-background-color: #0078D4;
}
QPlainTextEdit#engineLog {
  font-family: "Consolas", "DejaVu Sans Mono", monospace;
  font-size: 12px;
}
QScrollBar:vertical { background: #252526; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #5C5C5C; min-height: 24px; border-radius: 4px; }
QScrollBar:horizontal { background: #252526; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: #5C5C5C; min-width: 24px; border-radius: 4px; }
)")
                             : QStringLiteral(R"(
QMainWindow, QWidget#centralRoot { background-color: #F3F3F3; }
QFrame#card {
  background-color: #FFFFFF;
  border: 1px solid #E1E1E1;
  border-radius: 8px;
}
QLabel#titleH1 { color: #1A1A1A; font-size: 20px; font-weight: 600; }
QLabel#titleH2 { color: #1A1A1A; font-size: 14px; font-weight: 600; }
QLabel#muted { color: #5C5C5C; font-size: 13px; }
QListWidget#navPane {
  background-color: #FAFAFA;
  border: none;
  border-right: 1px solid #E1E1E1;
  outline: none;
  padding: 8px 0;
  font-size: 14px;
}
QListWidget#navPane::item {
  padding: 10px 16px;
  margin: 2px 8px;
  border-radius: 4px;
  color: #1A1A1A;
}
QListWidget#navPane::item:selected {
  background-color: #E5E5E5;
}
QListWidget#navPane::item:hover:!selected {
  background-color: #F0F0F0;
}
QPushButton[primary="true"] {
  background-color: #0078D4;
  color: #FFFFFF;
  border: none;
  border-radius: 4px;
  padding: 8px 20px;
  min-width: 140px;
  font-weight: 600;
}
QPushButton[primary="true"]:hover { background-color: #1A86D8; }
QPushButton[primary="true"]:pressed { background-color: #006CBD; }
QPushButton[secondary="true"] {
  background-color: #F0F0F0;
  color: #1A1A1A;
  border: 1px solid #C4C4C4;
  border-radius: 4px;
  padding: 8px 20px;
  min-width: 140px;
}
QPushButton[secondary="true"]:hover { background-color: #E5E5E5; }
QLineEdit, QPlainTextEdit, QSpinBox {
  background-color: #FFFFFF;
  border: 1px solid #C4C4C4;
  border-radius: 4px;
  padding: 6px 8px;
  selection-background-color: #0078D4;
}
QPlainTextEdit#engineLog {
  font-family: "Consolas", "DejaVu Sans Mono", monospace;
  font-size: 12px;
}
QScrollBar:vertical { background: #F3F3F3; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #C4C4C4; min-height: 24px; border-radius: 4px; }
QScrollBar:horizontal { background: #F3F3F3; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: #C4C4C4; min-width: 24px; border-radius: 4px; }
)");

    qApp->setStyleSheet(qss);
}

void centerWidgetOnScreen(QWidget* w)
{
    if (!w) {
        return;
    }
    QScreen* scr = w->screen();
    if (!scr) {
        scr = QGuiApplication::primaryScreen();
    }
    if (!scr) {
        return;
    }
    QRect fg = w->frameGeometry();
    fg.moveCenter(scr->availableGeometry().center());
    w->move(fg.topLeft());
}

} // namespace AppTheme
