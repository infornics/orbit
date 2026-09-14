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
    QColor("#71717a"), // 8 Bright Black / Gray
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
    setReadOnly(true);
    setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    setMaximumBlockCount(4000);
    setCenterOnScroll(false);

    QFont font("JetBrains Mono", 10);
    font.setStyleHint(QFont::Monospace);
    font.setFamilies({"JetBrains Mono", "Fira Code", "Cascadia Code", "DejaVu Sans Mono", "Monaco", "Consolas", "monospace"});
    setFont(font);

    setStyleSheet(R"(
        QPlainTextEdit#TerminalWidget {
            background-color: #0d0d11;
            color: #d4d4d8;
            border: none;
            selection-background-color: #272738;
            selection-color: #ffffff;
            padding: 8px;
        }
        QScrollBar:vertical {
            background: #0d0d11;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #272732;
            min-height: 16px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #3f3f50;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
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
    processByteStream(text);
}

void TerminalWidget::clearTerminal() {
    clear();
    m_currentFormat = m_defaultFormat;
    m_parserState = STATE_NORMAL;
    m_paramBuffer.clear();
    m_oscBuffer.clear();
}

void TerminalWidget::zoomInFont(int range) {
    QFont f = font();
    f.setPointSizeF(qMin(32.0, f.pointSizeF() + range));
    setFont(f);
    m_defaultFormat.setFont(f);
    updatePtySize();
}

void TerminalWidget::zoomOutFont(int range) {
    QFont f = font();
    f.setPointSizeF(qMax(6.0, f.pointSizeF() - range));
    setFont(f);
    m_defaultFormat.setFont(f);
    updatePtySize();
}

void TerminalWidget::resetZoomFont() {
    QFont f = font();
    f.setPointSizeF(10.0);
    setFont(f);
    m_defaultFormat.setFont(f);
    updatePtySize();
}

void TerminalWidget::keyPressEvent(QKeyEvent *event) {
    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();

    // Toggle shortcuts (Ctrl+~ or Ctrl+J) -> emit signal to parent
    if ((mods == Qt::ControlModifier && (key == Qt::Key_AsciiTilde || key == Qt::Key_QuoteLeft || key == Qt::Key_J))) {
        event->ignore();
        emit toggleTerminalRequested();
        return;
    }

    // Ctrl+Shift+C -> Copy text selection
    if (mods == (Qt::ControlModifier | Qt::ShiftModifier) && key == Qt::Key_C) {
        copy();
        return;
    }

    // Ctrl+Shift+V -> Paste clipboard text to PTY
    if (mods == (Qt::ControlModifier | Qt::ShiftModifier) && key == Qt::Key_V) {
        if (m_pty) {
            QString clipText = QGuiApplication::clipboard()->text();
            if (!clipText.isEmpty()) {
                m_pty->writeInput(clipText.toUtf8());
            }
        }
        return;
    }

    // Ctrl+Plus / Ctrl+Minus -> Zoom font
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
        int cols = qMax(10, (width() - 16) / charWidth);
        int rows = qMax(3, (height() - 16) / charHeight);
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
        if (code == 0) {
            m_currentFormat = m_defaultFormat;
        } else if (code == 1) {
            m_currentFormat.setFontWeight(QFont::Bold);
        } else if (code == 4) {
            m_currentFormat.setFontUnderline(true);
        } else if (code == 22) {
            m_currentFormat.setFontWeight(QFont::Normal);
        } else if (code == 24) {
            m_currentFormat.setFontUnderline(false);
        } else if (code >= 30 && code <= 37) {
            m_currentFormat.setForeground(s_ansi16[code - 30]);
        } else if (code == 39) {
            m_currentFormat.setForeground(m_defaultFormat.foreground());
        } else if (code >= 40 && code <= 47) {
            m_currentFormat.setBackground(s_ansi16[code - 40]);
        } else if (code == 49) {
            m_currentFormat.setBackground(m_defaultFormat.background());
        } else if (code >= 90 && code <= 97) {
            m_currentFormat.setForeground(s_ansi16[code - 90 + 8]);
        } else if (code >= 100 && code <= 107) {
            m_currentFormat.setBackground(s_ansi16[code - 100 + 8]);
        } else if (code == 38 || code == 48) {
            bool isFg = (code == 38);
            if (i + 1 < params.size()) {
                int mode = params[i + 1].toInt();
                if (mode == 5 && i + 2 < params.size()) {
                    int colorIdx = params[i + 2].toInt();
                    QColor col = parseAnsi256Color(colorIdx);
                    if (isFg) m_currentFormat.setForeground(col);
                    else m_currentFormat.setBackground(col);
                    i += 2;
                } else if (mode == 2 && i + 4 < params.size()) {
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

void TerminalWidget::handleCsiCommand(QChar cmd, const QString &params, QTextCursor &cursor) {
    QStringList pList = params.split(';');

    auto getParam = [&pList](int idx, int defVal) -> int {
        if (idx < pList.size() && !pList[idx].isEmpty()) {
            bool ok = false;
            int v = pList[idx].toInt(&ok);
            if (ok) return v;
        }
        return defVal;
    };

    if (cmd == 'm') {
        handleSgrSequence(pList);
    } else if (cmd == 'K') {
        // Erase in line
        int mode = getParam(0, 0);
        if (mode == 0) {
            // Erase from cursor to end of line
            cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
        } else if (mode == 1) {
            // Erase from start of line to cursor
            cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
        } else if (mode == 2) {
            // Erase entire line
            cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
        }
    } else if (cmd == 'J') {
        // Erase in display
        int mode = getParam(0, 0);
        if (mode == 2 || mode == 3) {
            clearTerminal();
            cursor = textCursor();
        } else if (mode == 0) {
            cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
        }
    } else if (cmd == 'A') {
        int count = getParam(0, 1);
        cursor.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor, count);
    } else if (cmd == 'B') {
        int count = getParam(0, 1);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, count);
    } else if (cmd == 'C') {
        int count = getParam(0, 1);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, count);
    } else if (cmd == 'D') {
        int count = getParam(0, 1);
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, count);
    } else if (cmd == 'G' || cmd == '`') {
        int col = getParam(0, 1);
        cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
        if (col > 1) {
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, col - 1);
        }
    } else if (cmd == 'P') {
        // Delete characters
        int count = getParam(0, 1);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, count);
        cursor.removeSelectedText();
    }
}

void TerminalWidget::processByteStream(const QString &text) {
    QTextCursor cursor = textCursor();
    // Ensure we place cursor at end if user is scrolled to bottom
    if (verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 5) {
        cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    }

    for (int i = 0; i < text.length(); ++i) {
        QChar ch = text[i];

        switch (m_parserState) {
        case STATE_NORMAL:
            if (ch == '\x1b') {
                m_parserState = STATE_ESC;
                m_paramBuffer.clear();
            } else if (ch == '\r') {
                cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
            } else if (ch == '\n') {
                cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::MoveAnchor);
                cursor.insertBlock(m_blockFormat, m_currentFormat);
            } else if (ch == '\b') {
                if (!cursor.atBlockStart()) {
                    cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor);
                }
            } else if (ch == '\t') {
                int col = cursor.positionInBlock();
                int spaces = 4 - (col % 4);
                for (int s = 0; s < spaces; ++s) {
                    if (!cursor.atBlockEnd()) {
                        cursor.deleteChar();
                    }
                    cursor.setCharFormat(m_currentFormat);
                    cursor.insertText(" ");
                }
            } else if (ch == '\a') {
                // Bell - ignore
            } else {
                if (!cursor.atBlockEnd()) {
                    cursor.deleteChar();
                }
                cursor.setCharFormat(m_currentFormat);
                cursor.insertText(QString(ch));
            }
            break;

        case STATE_ESC:
            if (ch == '[') {
                m_parserState = STATE_CSI;
                m_paramBuffer.clear();
            } else if (ch == ']') {
                m_parserState = STATE_OSC;
                m_oscBuffer.clear();
            } else if (ch == '(' || ch == ')' || ch == '#' || ch == '%') {
                m_parserState = STATE_CHARSET;
            } else {
                m_parserState = STATE_NORMAL;
            }
            break;

        case STATE_CHARSET:
            m_parserState = STATE_NORMAL;
            break;

        case STATE_OSC:
            if (ch == '\a') {
                m_parserState = STATE_NORMAL;
                m_oscBuffer.clear();
            } else if (ch == '\\' && m_oscBuffer.endsWith('\x1b')) {
                m_parserState = STATE_NORMAL;
                m_oscBuffer.clear();
            } else {
                m_oscBuffer.append(ch);
            }
            break;

        case STATE_CSI: {
            ushort u = ch.unicode();
            if ((u >= '0' && u <= '9') || ch == ';' || ch == '?' || ch == '>' || ch == '!' || ch == ' ') {
                m_paramBuffer.append(ch);
            } else if (u >= 0x40 && u <= 0x7E) {
                handleCsiCommand(ch, m_paramBuffer, cursor);
                m_parserState = STATE_NORMAL;
                m_paramBuffer.clear();
            } else if (ch == '\x1b') {
                m_parserState = STATE_ESC;
                m_paramBuffer.clear();
            } else {
                m_parserState = STATE_NORMAL;
                m_paramBuffer.clear();
            }
            break;
        }
        }
    }

    setTextCursor(cursor);
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

} // namespace Orbit
