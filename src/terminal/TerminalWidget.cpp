#include "terminal/TerminalWidget.h"
#include "terminal/PtyProcess.h"

#include <QKeyEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QTextCursor>
#include <QScrollBar>
#include <QGuiApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QRegularExpression>
#include <QDebug>

namespace Orbit {

static const QColor s_ansi16[16] = {
    QColor("#18181c"), // 0 Black
    QColor("#f87171"), // 1 Red
    QColor("#4ade80"), // 2 Green
    QColor("#facc15"), // 3 Yellow
    QColor("#60a5fa"), // 4 Blue
    QColor("#c084fc"), // 5 Magenta
    QColor("#38bdf8"), // 6 Cyan
    QColor("#e4e4e7"), // 7 White
    QColor("#71717a"), // 8 Bright Black
    QColor("#fca5a5"), // 9 Bright Red
    QColor("#86efac"), // 10 Bright Green
    QColor("#fde047"), // 11 Bright Yellow
    QColor("#93c5fd"), // 12 Bright Blue
    QColor("#e9d5ff"), // 13 Bright Magenta
    QColor("#7dd3fc"), // 14 Bright Cyan
    QColor("#ffffff")  // 15 Bright White
};

TerminalWidget::TerminalWidget(QWidget *parent)
    : QPlainTextEdit(parent) {
    setObjectName("TerminalWidget");
    setReadOnly(true); // Terminal handles input via keyPressEvent
    setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    setMaximumBlockCount(5000); // Prevent infinite scroll memory buffer leak

    // Fixed width font for terminal
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setFamily("JetBrains Mono, Fira Code, DejaVu Sans Mono, Consolas, monospace");
    font.setPointSizeF(10.0);
    font.setStyleHint(QFont::Monospace);
    setFont(font);

    // Dark terminal palette
    setStyleSheet(R"(
        QPlainTextEdit#TerminalWidget {
            background-color: #121215;
            color: #d4d4d8;
            border: none;
            selection-background-color: #272730;
            selection-color: #ffffff;
            padding: 4px;
        }
    )");

    m_defaultFormat.setForeground(QColor("#d4d4d8"));
    m_defaultFormat.setFont(font);
    m_currentFormat = m_defaultFormat;
}

TerminalWidget::~TerminalWidget() = default;

void TerminalWidget::setPtyProcess(PtyProcess *pty) {
    m_pty = pty;
    if (m_pty) {
        connect(m_pty, &PtyProcess::readyRead, this, &TerminalWidget::appendData);
        updatePtySize();
    }
}

void TerminalWidget::appendData(const QByteArray &data) {
    if (data.isEmpty()) return;
    QString text = QString::fromUtf8(data);
    processAnsiStream(text);

    // Auto-scroll to bottom
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void TerminalWidget::clearTerminal() {
    clear();
    m_currentFormat = m_defaultFormat;
}

void TerminalWidget::zoomInFont(int range) {
    QFont f = font();
    f.setPointSizeF(qMin(32.0, f.pointSizeF() + range));
    setFont(f);
    updatePtySize();
}

void TerminalWidget::zoomOutFont(int range) {
    QFont f = font();
    f.setPointSizeF(qMax(6.0, f.pointSizeF() - range));
    setFont(f);
    updatePtySize();
}

void TerminalWidget::resetZoomFont() {
    QFont f = font();
    f.setPointSizeF(10.0);
    setFont(f);
    updatePtySize();
}

void TerminalWidget::keyPressEvent(QKeyEvent *event) {
    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();

    // Check toggle shortcuts (Ctrl+~ or Ctrl+J) -> let parent window handle
    if ((mods == Qt::ControlModifier && (key == Qt::Key_AsciiTilde || key == Qt::Key_QuoteLeft || key == Qt::Key_J))) {
        event->ignore();
        emit toggleTerminalRequested();
        return;
    }

    // Ctrl+Shift+C -> Copy selection
    if (mods == (Qt::ControlModifier | Qt::ShiftModifier) && key == Qt::Key_C) {
        copy();
        return;
    }

    // Ctrl+Shift+V -> Paste clipboard
    if (mods == (Qt::ControlModifier | Qt::ShiftModifier) && key == Qt::Key_V) {
        if (m_pty) {
            QString clipText = QGuiApplication::clipboard()->text();
            if (!clipText.isEmpty()) {
                m_pty->writeInput(clipText.toUtf8());
            }
        }
        return;
    }

    // Ctrl+Plus / Ctrl+Minus -> Zoom terminal font
    if (mods & Qt::ControlModifier) {
        if (key == Qt::Key_Plus || key == Qt::Key_Equal) {
            zoomInFont();
            return;
        }
        if (key == Qt::Key_Minus) {
            zoomOutFont();
            return;
        }
        if (key == Qt::Key_0) {
            resetZoomFont();
            return;
        }
    }

    if (!m_pty || !m_pty->isRunning()) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    QByteArray bytesToSend;

    if (mods & Qt::ControlModifier && !(mods & Qt::AltModifier) && !(mods & Qt::ShiftModifier)) {
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            char ctrlByte = static_cast<char>(1 + (key - Qt::Key_A));
            bytesToSend.append(ctrlByte);
        }
    } else {
        switch (key) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            bytesToSend.append('\r');
            break;
        case Qt::Key_Backspace:
            bytesToSend.append('\x7f');
            break;
        case Qt::Key_Tab:
            bytesToSend.append('\t');
            break;
        case Qt::Key_Escape:
            bytesToSend.append('\x1b');
            break;
        case Qt::Key_Up:
            bytesToSend.append("\x1b[A");
            break;
        case Qt::Key_Down:
            bytesToSend.append("\x1b[B");
            break;
        case Qt::Key_Right:
            bytesToSend.append("\x1b[C");
            break;
        case Qt::Key_Left:
            bytesToSend.append("\x1b[D");
            break;
        case Qt::Key_Home:
            bytesToSend.append("\x1b[H");
            break;
        case Qt::Key_End:
            bytesToSend.append("\x1b[F");
            break;
        case Qt::Key_PageUp:
            bytesToSend.append("\x1b[5~");
            break;
        case Qt::Key_PageDown:
            bytesToSend.append("\x1b[6~");
            break;
        case Qt::Key_Delete:
            bytesToSend.append("\x1b[3~");
            break;
        default:
            QString inputStr = event->text();
            if (!inputStr.isEmpty()) {
                bytesToSend = inputStr.toUtf8();
            }
            break;
        }
    }

    if (!bytesToSend.isEmpty()) {
        m_pty->writeInput(bytesToSend);
    }
}

void TerminalWidget::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    updatePtySize();
}

void TerminalWidget::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            zoomInFont();
        } else if (event->angleDelta().y() < 0) {
            zoomOutFont();
        }
        event->accept();
        return;
    }
    QPlainTextEdit::wheelEvent(event);
}

void TerminalWidget::updatePtySize() {
    if (!m_pty || !m_pty->isRunning()) return;

    int charWidth = fontMetrics().horizontalAdvance('M');
    int charHeight = fontMetrics().height();

    if (charWidth > 0 && charHeight > 0) {
        int cols = qMax(10, (width() - 10) / charWidth);
        int rows = qMax(3, (height() - 10) / charHeight);
        m_pty->resizePty(rows, cols);
    }
}

QColor TerminalWidget::parseAnsi256Color(int index) {
    if (index >= 0 && index < 16) {
        return s_ansi16[index];
    }
    if (index >= 16 && index <= 231) {
        int idx = index - 16;
        int r = (idx / 36) % 6;
        int g = (idx / 6) % 6;
        int b = idx % 6;
        return QColor(r ? (r * 40 + 55) : 0,
                      g ? (g * 40 + 55) : 0,
                      b ? (b * 40 + 55) : 0);
    }
    if (index >= 232 && index <= 255) {
        int gray = 8 + (index - 232) * 10;
        return QColor(gray, gray, gray);
    }
    return QColor("#d4d4d8");
}

void TerminalWidget::handleSgrSequence(const QStringList &params) {
    if (params.isEmpty() || (params.size() == 1 && params.first().isEmpty())) {
        m_currentFormat = m_defaultFormat;
        return;
    }

    for (int i = 0; i < params.size(); ++i) {
        int code = params[i].toInt();
        if (code == 0) { // Reset
            m_currentFormat = m_defaultFormat;
        } else if (code == 1) { // Bold
            m_currentFormat.setFontWeight(QFont::Bold);
        } else if (code == 4) { // Underline
            m_currentFormat.setFontUnderline(true);
        } else if (code == 22) { // Normal weight
            m_currentFormat.setFontWeight(QFont::Normal);
        } else if (code == 24) { // Underline off
            m_currentFormat.setFontUnderline(false);
        } else if (code >= 30 && code <= 37) { // FG 8 colors
            m_currentFormat.setForeground(s_ansi16[code - 30]);
        } else if (code == 39) { // Default FG
            m_currentFormat.setForeground(m_defaultFormat.foreground());
        } else if (code >= 40 && code <= 47) { // BG 8 colors
            m_currentFormat.setBackground(s_ansi16[code - 40]);
        } else if (code == 49) { // Default BG
            m_currentFormat.setBackground(m_defaultFormat.background());
        } else if (code >= 90 && code <= 97) { // Bright FG
            m_currentFormat.setForeground(s_ansi16[code - 90 + 8]);
        } else if (code >= 100 && code <= 107) { // Bright BG
            m_currentFormat.setBackground(s_ansi16[code - 100 + 8]);
        } else if (code == 38 || code == 48) { // 256 / TrueColor FG or BG
            bool isFg = (code == 38);
            if (i + 1 < params.size()) {
                int mode = params[i + 1].toInt();
                if (mode == 5 && i + 2 < params.size()) { // 256 color
                    int colorIdx = params[i + 2].toInt();
                    QColor col = parseAnsi256Color(colorIdx);
                    if (isFg) m_currentFormat.setForeground(col);
                    else m_currentFormat.setBackground(col);
                    i += 2;
                } else if (mode == 2 && i + 4 < params.size()) { // RGB TrueColor
                    int r = params[i + 2].toInt();
                    int g = params[i + 3].toInt();
                    int b = params[i + 4].toInt();
                    QColor col(r, g, b);
                    if (isFg) m_currentFormat.setForeground(col);
                    else m_currentFormat.setBackground(col);
                    i += 4;
                }
            }
        }
    }
}

void TerminalWidget::processAnsiStream(const QString &text) {
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);

    int pos = 0;
    int len = text.length();

    while (pos < len) {
        QChar ch = text[pos];

        if (ch == '\x1b') {
            // Escape sequence start
            if (pos + 1 < len && text[pos + 1] == '[') {
                // CSI sequence \033[ ... [letter]
                int endPos = pos + 2;
                while (endPos < len && (text[endPos].isDigit() || text[endPos] == ';' || text[endPos] == '?')) {
                    endPos++;
                }
                if (endPos < len) {
                    QChar cmd = text[endPos];
                    QString seqParam = text.mid(pos + 2, endPos - (pos + 2));

                    if (cmd == 'm') {
                        // SGR sequence
                        handleSgrSequence(seqParam.split(';'));
                    } else if (cmd == 'J' && (seqParam == "2" || seqParam == "3")) {
                        // Clear screen
                        clearTerminal();
                        cursor = textCursor();
                    } else if (cmd == 'K') {
                        // Erase in line - ignore for basic display
                    }
                    pos = endPos + 1;
                    continue;
                }
            }
            pos++;
            continue;
        }

        if (ch == '\r') {
            // Carriage return - if next character is not '\n', move cursor to line start
            if (pos + 1 < len && text[pos + 1] == '\n') {
                // Handled in next iteration as newline
            } else {
                cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
            }
            pos++;
            continue;
        }

        if (ch == '\b') {
            // Backspace character
            if (!cursor.atBlockStart()) {
                cursor.deletePreviousChar();
            }
            pos++;
            continue;
        }

        if (ch == '\a') {
            // Bell - ignore
            pos++;
            continue;
        }

        // Standard printable character or \n
        cursor.setCharFormat(m_currentFormat);
        cursor.insertText(QString(ch));
        pos++;
    }

    setTextCursor(cursor);
}

} // namespace Orbit
