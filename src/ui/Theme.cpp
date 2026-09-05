#include "ui/Theme.h"
#include <QFontDatabase>

namespace Orbit {

QFont Theme::editorFont(int pointSize) {
    const QStringList preferredFamilies = {
        "MesloLGS Nerd Font Mono",
        "MesloLGM Nerd Font Mono",
        "Adwaita Mono",
        "JetBrains Mono",
        "Fira Code",
        "DejaVu Sans Mono",
        "Liberation Mono",
        "monospace"
    };

    const QStringList availableFamilies = QFontDatabase::families();
    QString chosenFamily = "monospace";
    for (const QString &fam : preferredFamilies) {
        if (availableFamilies.contains(fam, Qt::CaseInsensitive)) {
            chosenFamily = fam;
            break;
        }
    }

    QFont font(chosenFamily, pointSize);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    return font;
}

QFont Theme::uiFont(int pointSize) {
    const QStringList preferredFamilies = {
        "Inter",
        "Cantarell",
        "Ubuntu",
        "Segoe UI",
        "Noto Sans",
        "DejaVu Sans"
    };

    const QStringList availableFamilies = QFontDatabase::families();
    QString chosenFamily;
    for (const QString &fam : preferredFamilies) {
        if (availableFamilies.contains(fam, Qt::CaseInsensitive)) {
            chosenFamily = fam;
            break;
        }
    }

    QFont font = chosenFamily.isEmpty() ? QFont() : QFont(chosenFamily);
    font.setPointSize(pointSize);
    return font;
}

QFont Theme::monoFont(int pointSize) {
    return editorFont(pointSize);
}

QString Theme::applicationStyleSheet() {
    return QString(R"(
        /* Global Window and Base */
        QMainWindow, QWidget#centralWidget {
            background-color: #121215;
            color: #eaeaf0;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            font-size: 13px;
        }

        /* Splitter */
        QSplitter::handle {
            background-color: #1e1e24;
        }
        QSplitter::handle:horizontal {
            width: 1px;
        }
        QSplitter::handle:horizontal:hover {
            background-color: #4f8cf6;
        }

        /* Menu Bar */
        QMenuBar {
            background-color: #151518;
            color: #b0b0be;
            border-bottom: 1px solid #24242c;
            padding: 2px 6px;
            font-size: 12px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 4px 8px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: #24242e;
            color: #ffffff;
        }
        QMenuBar::item:pressed {
            background-color: #2b2b36;
        }

        /* Menus */
        QMenu {
            background-color: #1b1b20;
            color: #eaeaf0;
            border: 1px solid #2c2c36;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px 6px 26px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #262632;
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background-color: #262632;
            margin: 4px 6px;
        }

        /* Status Bar */
        QStatusBar {
            background-color: #141418;
            color: #828292;
            border-top: 1px solid #22222a;
            font-size: 11px;
            padding: 2px 10px;
        }
        QStatusBar QLabel {
            color: #828292;
            padding: 0 6px;
        }

        /* Tree View (Explorer) */
        QTreeView {
            background-color: #18181c;
            color: #c4c4d0;
            border: none;
            outline: none;
            font-size: 13px;
            padding: 4px;
        }
        QTreeView::item {
            height: 26px;
            background: transparent;
            border: none;
            padding-left: 2px;
        }
        QTreeView::item:hover,
        QTreeView::item:selected,
        QTreeView::item:selected:active {
            background: transparent;
            color: #ffffff;
        }
        QTreeView::branch,
        QTreeView::branch:hover,
        QTreeView::branch:selected,
        QTreeView::branch:selected:active,
        QTreeView::branch:selected:!active,
        QTreeView::branch:has-children:hover,
        QTreeView::branch:has-children:selected {
            background: transparent;
        }

        /* ScrollBars */
        QScrollBar:vertical {
            border: none;
            background: #18181c;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #33333e;
            min-height: 24px;
            border-radius: 5px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background: #4a4a58;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }

        QScrollBar:horizontal {
            border: none;
            background: #18181c;
            height: 10px;
            margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: #33333e;
            min-width: 24px;
            border-radius: 5px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #4a4a58;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: none;
        }

        /* Buttons */
        QPushButton {
            background-color: #24242d;
            color: #d6d6e2;
            border: 1px solid #32323f;
            border-radius: 5px;
            padding: 5px 12px;
            font-size: 12px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #2d2d38;
            border-color: #404052;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #1e1e26;
        }
        QPushButton#primaryButton {
            background-color: #3b74db;
            color: #ffffff;
            border: 1px solid #4a82e8;
        }
        QPushButton#primaryButton:hover {
            background-color: #4b84eb;
            border-color: #5c93f7;
        }
        QPushButton#primaryButton:pressed {
            background-color: #3162be;
        }

        /* Tooltips */
        QToolTip {
            background-color: #24242c;
            color: #e4e4ed;
            border: 1px solid #3a3a46;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 12px;
        }

        /* Dialogs / Message Boxes */
        QMessageBox {
            background-color: #1c1c24;
            color: #eaeaf0;
            border: 1px solid #2e2e3a;
            border-radius: 8px;
        }
        QMessageBox QLabel {
            color: #eaeaf0;
            font-size: 13px;
            background: transparent;
        }
        QMessageBox QLabel#qt_msgbox_informativelabel {
            color: #9292a4;
            font-size: 12px;
        }
        QMessageBox QPushButton {
            background-color: #262632;
            color: #eaeaf0;
            border: 1px solid #3c3c4c;
            border-radius: 5px;
            padding: 6px 18px;
            min-width: 75px;
            font-size: 12px;
            font-weight: 500;
        }
        QMessageBox QPushButton:hover {
            background-color: #323242;
            border-color: #4f8cf6;
            color: #ffffff;
        }
        QMessageBox QPushButton:pressed {
            background-color: #20202a;
        }
        QMessageBox QPushButton:default {
            background-color: #3b74db;
            border-color: #4f8cf6;
            color: #ffffff;
        }
        QMessageBox QPushButton:default:hover {
            background-color: #4f8cf6;
            border-color: #6ba0f8;
        }
    )");
}

} // namespace Orbit
