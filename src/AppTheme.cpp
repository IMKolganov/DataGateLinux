#include "AppTheme.h"
#include "BrandColors.h"

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
        using namespace BrandColors::Dark;
        pal.setColor(QPalette::Window, QColor(QString::fromUtf8(kBgDefault)));
        pal.setColor(QPalette::WindowText, QColor(QString::fromUtf8(kTextDefault)));
        pal.setColor(QPalette::Base, QColor(QString::fromUtf8(kBgSubtle)));
        pal.setColor(QPalette::AlternateBase, QColor(QString::fromUtf8(kBgMuted)));
        pal.setColor(QPalette::Text, QColor(QString::fromUtf8(kTextDefault)));
        pal.setColor(QPalette::Button, QColor(QString::fromUtf8(kBgSubtle)));
        pal.setColor(QPalette::ButtonText, QColor(QString::fromUtf8(kTextDefault)));
        pal.setColor(QPalette::Highlight, QColor(QString::fromUtf8(kAccentEmphasis)));
        pal.setColor(QPalette::HighlightedText, QColor(QString::fromUtf8(kWhite)));
        pal.setColor(QPalette::Link, QColor(QString::fromUtf8(kAccentFg)));
        pal.setColor(QPalette::PlaceholderText, QColor(QString::fromUtf8(kTextMuted)));
    } else {
        using namespace BrandColors::Light;
        pal.setColor(QPalette::Window, QColor(QString::fromUtf8(kBgDefault)));
        pal.setColor(QPalette::WindowText, QColor(QString::fromUtf8(kTextDefault)));
        pal.setColor(QPalette::Base, QColor(QString::fromUtf8(kBgMuted)));
        pal.setColor(QPalette::AlternateBase, QColor(QString::fromUtf8(kBgSubtle)));
        pal.setColor(QPalette::Text, QColor(QString::fromUtf8(kTextDefault)));
        pal.setColor(QPalette::Button, QColor(QString::fromUtf8(kBgSubtle)));
        pal.setColor(QPalette::ButtonText, QColor(QString::fromUtf8(kTextDefault)));
        pal.setColor(QPalette::Highlight, QColor(QString::fromUtf8(kAccentEmphasis)));
        pal.setColor(QPalette::HighlightedText, QColor(QString::fromUtf8(kWhite)));
        pal.setColor(QPalette::Link, QColor(QString::fromUtf8(kAccentFg)));
        pal.setColor(QPalette::PlaceholderText, QColor(QString::fromUtf8(kTextMuted)));
    }
    qApp->setPalette(pal);

    QString qss;
    if (dark) {
        using namespace BrandColors::Dark;
        qss = QStringLiteral(
                  R"(
QMainWindow, QWidget#centralRoot { background-color: %1; }
QFrame#card {
  background-color: %2;
  border: 1px solid %3;
  border-radius: 8px;
}
QLabel#titleH1 { color: %4; font-size: 20px; font-weight: 600; }
QLabel#titleH2 { color: %4; font-size: 14px; font-weight: 600; }
QLabel#muted { color: %5; font-size: 13px; }
QLabel#accessTrafficTitle { color: %4; font-size: 14px; font-weight: 600; }
QProgressBar#accessQuotaProgress {
  border: 1px solid %3;
  border-radius: 6px;
  background-color: %8;
  min-height: 12px;
  max-height: 12px;
}
QProgressBar#accessQuotaProgress::chunk {
  border-radius: 5px;
  background-color: %9;
}
QProgressBar#accessQuotaProgress[overquota="true"]::chunk {
  background-color: #dc2626;
}
QListWidget#navPane {
  background-color: %6;
  border: none;
  border-right: 1px solid %3;
  outline: none;
  padding: 8px 0;
  font-size: 14px;
}
QListWidget#navPane::viewport {
  background-color: %6;
}
QListWidget#navPane::item {
  padding: 10px 16px;
  margin: 2px 8px;
  border-radius: 6px;
  color: %4;
}
QListWidget#navPane::item:selected {
  background-color: %7;
}
QListWidget#navPane::item:hover:!selected {
  background-color: %8;
}
QPushButton[primary="true"] {
  background-color: %9;
  color: %10;
  border: none;
  border-radius: 6px;
  padding: 8px 20px;
  min-width: 140px;
  font-weight: 600;
}
QPushButton[primary="true"]:hover { background-color: %11; }
QPushButton[primary="true"]:pressed { background-color: %9; }
QPushButton[secondary="true"] {
  background-color: %8;
  color: %4;
  border: 1px solid %3;
  border-radius: 6px;
  padding: 8px 20px;
  min-width: 140px;
}
QPushButton[secondary="true"]:hover { background-color: %7; }
QLineEdit, QPlainTextEdit, QSpinBox {
  background-color: %8;
  border: 1px solid %3;
  border-radius: 6px;
  padding: 6px 8px;
  selection-background-color: %9;
}
QPlainTextEdit#engineLog {
  font-family: "Cascadia Mono", "Consolas", "DejaVu Sans Mono", monospace;
  font-size: 12px;
}
QTableWidget#accessServerTable {
  background-color: %8;
  border: 1px solid %3;
  border-radius: 6px;
  gridline-color: %3;
}
QTableWidget#accessServerTable QHeaderView::section {
  background-color: %2;
  color: %4;
  padding: 6px 8px;
  border: none;
  border-bottom: 1px solid %3;
}
QScrollBar:vertical { background: %6; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: %12; min-height: 24px; border-radius: 4px; }
QScrollBar:horizontal { background: %6; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: %12; min-width: 24px; border-radius: 4px; }
)")
                  .arg(QString::fromUtf8(kBgDefault),
                      QString::fromUtf8(kBgMuted),
                      QString::fromUtf8(kBorderDefault),
                      QString::fromUtf8(kTextDefault),
                      QString::fromUtf8(kTextMuted),
                      QString::fromUtf8(kBgDefault),
                      QString::fromUtf8(kBgSubtle),
                      QString::fromUtf8(kBgSubtle),
                      QString::fromUtf8(kAccentEmphasis),
                      QString::fromUtf8(kWhite),
                      QString::fromUtf8(kAccentHover),
                      QString::fromUtf8(kTextFaint));
    } else {
        using namespace BrandColors::Light;
        qss = QStringLiteral(
                  R"(
QMainWindow, QWidget#centralRoot { background-color: %1; }
QFrame#card {
  background-color: %2;
  border: 1px solid %3;
  border-radius: 8px;
}
QLabel#titleH1 { color: %4; font-size: 20px; font-weight: 600; }
QLabel#titleH2 { color: %4; font-size: 14px; font-weight: 600; }
QLabel#muted { color: %5; font-size: 13px; }
QLabel#accessTrafficTitle { color: %4; font-size: 14px; font-weight: 600; }
QProgressBar#accessQuotaProgress {
  border: 1px solid %3;
  border-radius: 6px;
  background-color: %8;
  min-height: 12px;
  max-height: 12px;
}
QProgressBar#accessQuotaProgress::chunk {
  border-radius: 5px;
  background-color: %9;
}
QProgressBar#accessQuotaProgress[overquota="true"]::chunk {
  background-color: #dc2626;
}
QListWidget#navPane {
  background-color: %6;
  border: none;
  border-right: 1px solid %3;
  outline: none;
  padding: 8px 0;
  font-size: 14px;
}
QListWidget#navPane::viewport {
  background-color: %6;
}
QListWidget#navPane::item {
  padding: 10px 16px;
  margin: 2px 8px;
  border-radius: 6px;
  color: %4;
}
QListWidget#navPane::item:selected {
  background-color: %7;
}
QListWidget#navPane::item:hover:!selected {
  background-color: %8;
}
QPushButton[primary="true"] {
  background-color: %9;
  color: %10;
  border: none;
  border-radius: 6px;
  padding: 8px 20px;
  min-width: 140px;
  font-weight: 600;
}
QPushButton[primary="true"]:hover { background-color: %11; }
QPushButton[primary="true"]:pressed { background-color: %9; }
QPushButton[secondary="true"] {
  background-color: %8;
  color: %4;
  border: 1px solid %3;
  border-radius: 6px;
  padding: 8px 20px;
  min-width: 140px;
}
QPushButton[secondary="true"]:hover { background-color: %7; }
QLineEdit, QPlainTextEdit, QSpinBox {
  background-color: %2;
  border: 1px solid %3;
  border-radius: 6px;
  padding: 6px 8px;
  selection-background-color: %9;
}
QPlainTextEdit#engineLog {
  font-family: "Cascadia Mono", "Consolas", "DejaVu Sans Mono", monospace;
  font-size: 12px;
}
QTableWidget#accessServerTable {
  background-color: %2;
  border: 1px solid %3;
  border-radius: 6px;
  gridline-color: %12;
}
QTableWidget#accessServerTable QHeaderView::section {
  background-color: %8;
  color: %4;
  padding: 6px 8px;
  border: none;
  border-bottom: 1px solid %3;
}
QScrollBar:vertical { background: %1; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: %13; min-height: 24px; border-radius: 4px; }
QScrollBar:horizontal { background: %1; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: %13; min-width: 24px; border-radius: 4px; }
)")
                  .arg(QString::fromUtf8(kBgDefault),
                      QString::fromUtf8(kBgMuted),
                      QString::fromUtf8(kBorderDefault),
                      QString::fromUtf8(kTextDefault),
                      QString::fromUtf8(kTextMuted),
                      QString::fromUtf8(kBgDefault),
                      QString::fromUtf8(kBorderMuted),
                      QString::fromUtf8(kBgSubtle),
                      QString::fromUtf8(kAccentEmphasis),
                      QString::fromUtf8(kWhite),
                      QString::fromUtf8(kAccentHover),
                      QString::fromUtf8(kBorderMuted),
                      QString::fromUtf8(kTextFaint));
    }

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
