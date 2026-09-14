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
#include <QTextDocument>
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

    initGrid(24, 80);
}

TerminalWidget::~TerminalWidget() = default;

void TerminalWidget::initGrid(int rows, int cols) {
    m_rows = qMax(3, rows);
    m_cols = qMax(10, cols);
    m_grid.clear();
    for (int r = 0; r < m_rows; ++r) {
        m_grid.append(QList<TerminalCell>(m_cols, {' ', m_defaultFormat}));
    }
    m_cursorRow = 0;
    m_cursorCol = 0;
}

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
    renderScreenToWidget();
}

void TerminalWidget::clearTerminal() {
    clear();
    m_history.clear();
    initGrid(m_rows, m_cols);
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
    int charWidth = fontMetrics().horizontalAdvance('M');
    int charHeight = fontMetrics().height();

    if (charWidth > 0 && charHeight > 0) {
        int cols = qMax(10, (width() - 16) / charWidth);
        int rows = qMax(3, (height() - 16) / charHeight);

        if (cols != m_cols || rows != m_rows) {
            m_cols = cols;
            m_rows = rows;
            while (m_grid.size() < m_rows) {
                m_grid.append(QList<TerminalCell>(m_cols, {' ', m_defaultFormat}));
            }
            while (m_grid.size() > m_rows) {
                m_grid.removeLast();
            }
            for (int r = 0; r < m_grid.size(); ++r) {
                while (m_grid[r].size() < m_cols) {
                    m_grid[r].append({' ', m_defaultFormat});
                }
                if (m_grid[r].size() > m_cols) {
                    m_grid[r] = m_grid[r].mid(0, m_cols);
                }
            }
            m_cursorRow = qBound(0, m_cursorRow, m_rows - 1);
            m_cursorCol = qBound(0, m_cursorCol, m_cols - 1);

            if (m_pty && m_pty->isRunning()) {
                m_pty->resizePty(m_rows, m_cols);
            }
            renderScreenToWidget();
        }
    }
}

void TerminalWidget::putChar(QChar ch) {
    if (m_cursorCol >= m_cols) {
        m_cursorCol = 0;
        newLine();
    }
    if (m_cursorRow >= 0 && m_cursorRow < m_grid.size() && m_cursorCol >= 0 && m_cursorCol < m_cols) {
        m_grid[m_cursorRow][m_cursorCol] = {ch, m_currentFormat};
    }
    m_cursorCol++;
}

void TerminalWidget::newLine() {
    m_cursorRow++;
    if (m_cursorRow >= m_rows) {
        if (!m_grid.isEmpty()) {
            m_history.append(m_grid.takeFirst());
            if (m_history.size() > m_maxHistoryLines) {
                m_history.removeFirst();
            }
        }
        m_grid.append(QList<TerminalCell>(m_cols, {' ', m_defaultFormat}));
        m_cursorRow = m_rows - 1;
    }
}

void TerminalWidget::carriageReturn() {
    m_cursorCol = 0;
}

void TerminalWidget::backspace() {
    m_cursorCol = qMax(0, m_cursorCol - 1);
}

void TerminalWidget::tab() {
    int nextTab = qMin(m_cols, (m_cursorCol / 8 + 1) * 8);
    while (m_cursorCol < nextTab) {
        putChar(' ');
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

void TerminalWidget::handleOscCommand(const QString &oscStr) {
    Q_UNUSED(oscStr);
}

void TerminalWidget::handleCsiCommand(QChar cmd, const QString &params) {
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
        if (m_cursorRow >= 0 && m_cursorRow < m_grid.size()) {
            if (mode == 0) {
                for (int c = m_cursorCol; c < m_cols; ++c) {
                    m_grid[m_cursorRow][c] = {' ', m_defaultFormat};
                }
            } else if (mode == 1) {
                for (int c = 0; c <= qMin(m_cursorCol, m_cols - 1); ++c) {
                    m_grid[m_cursorRow][c] = {' ', m_defaultFormat};
                }
            } else if (mode == 2) {
                for (int c = 0; c < m_cols; ++c) {
                    m_grid[m_cursorRow][c] = {' ', m_defaultFormat};
                }
            }
        }
    } else if (cmd == 'J') {
        // Erase in display
        int mode = getParam(0, 0);
        if (mode == 2 || mode == 3) {
            m_history.clear();
            initGrid(m_rows, m_cols);
        } else if (mode == 0) {
            for (int r = m_cursorRow; r < m_rows; ++r) {
                int startC = (r == m_cursorRow) ? m_cursorCol : 0;
                for (int c = startC; c < m_cols; ++c) {
                    m_grid[r][c] = {' ', m_defaultFormat};
                }
            }
        }
    } else if (cmd == 'A') {
        int count = getParam(0, 1);
        m_cursorRow = qMax(0, m_cursorRow - count);
    } else if (cmd == 'B') {
        int count = getParam(0, 1);
        m_cursorRow = qMin(m_rows - 1, m_cursorRow + count);
    } else if (cmd == 'C') {
        int count = getParam(0, 1);
        m_cursorCol = qMin(m_cols - 1, m_cursorCol + count);
    } else if (cmd == 'D') {
        int count = getParam(0, 1);
        m_cursorCol = qMax(0, m_cursorCol - count);
    } else if (cmd == 'G' || cmd == '`') {
        int col = getParam(0, 1);
        m_cursorCol = qBound(0, col - 1, m_cols - 1);
    } else if (cmd == 'H' || cmd == 'f') {
        int row = getParam(0, 1);
        int col = getParam(1, 1);
        m_cursorRow = qBound(0, row - 1, m_rows - 1);
        m_cursorCol = qBound(0, col - 1, m_cols - 1);
    } else if (cmd == 'P') {
        int count = getParam(0, 1);
        if (m_cursorRow >= 0 && m_cursorRow < m_grid.size()) {
            for (int c = m_cursorCol; c < m_cols; ++c) {
                int src = c + count;
                if (src < m_cols) {
                    m_grid[m_cursorRow][c] = m_grid[m_cursorRow][src];
                } else {
                    m_grid[m_cursorRow][c] = {' ', m_defaultFormat};
                }
            }
        }
    } else if (cmd == '@') {
        int count = getParam(0, 1);
        if (m_cursorRow >= 0 && m_cursorRow < m_grid.size()) {
            for (int c = m_cols - 1; c >= m_cursorCol + count; --c) {
                m_grid[m_cursorRow][c] = m_grid[m_cursorRow][c - count];
            }
            for (int c = m_cursorCol; c < qMin(m_cols, m_cursorCol + count); ++c) {
                m_grid[m_cursorRow][c] = {' ', m_defaultFormat};
            }
        }
    }
}

void TerminalWidget::processByteStream(const QString &text) {
    for (int i = 0; i < text.length(); ++i) {
        QChar ch = text[i];

        switch (m_parserState) {
        case STATE_NORMAL:
            if (ch == '\x1b') {
                m_parserState = STATE_ESC;
                m_paramBuffer.clear();
            } else if (ch == '\r') {
                carriageReturn();
            } else if (ch == '\n') {
                newLine();
            } else if (ch == '\b') {
                backspace();
            } else if (ch == '\t') {
                tab();
            } else if (ch == '\a') {
                // Bell - ignore
            } else {
                putChar(ch);
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
            } else if (ch == '7') {
                m_savedRow = m_cursorRow;
                m_savedCol = m_cursorCol;
                m_parserState = STATE_NORMAL;
            } else if (ch == '8') {
                m_cursorRow = qBound(0, m_savedRow, m_rows - 1);
                m_cursorCol = qBound(0, m_savedCol, m_cols - 1);
                m_parserState = STATE_NORMAL;
            } else {
                m_parserState = STATE_NORMAL;
            }
            break;

        case STATE_CHARSET:
            m_parserState = STATE_NORMAL;
            break;

        case STATE_OSC:
            if (ch == '\a') {
                handleOscCommand(m_oscBuffer);
                m_parserState = STATE_NORMAL;
                m_oscBuffer.clear();
            } else if (ch == '\\' && m_oscBuffer.endsWith('\x1b')) {
                m_oscBuffer.chop(1);
                handleOscCommand(m_oscBuffer);
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
                handleCsiCommand(ch, m_paramBuffer);
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
}

void TerminalWidget::renderScreenToWidget() {
    QTextDocument *doc = document();
    doc->clear();
    QTextCursor cursor(doc);

    // Render history lines
    for (int h = 0; h < m_history.size(); ++h) {
        const auto &row = m_history[h];
        int lastNonSpace = row.size() - 1;
        while (lastNonSpace >= 0 && row[lastNonSpace].ch == ' ' && row[lastNonSpace].format == m_defaultFormat) {
            lastNonSpace--;
        }

        for (int c = 0; c <= lastNonSpace && c < row.size(); ++c) {
            cursor.setCharFormat(row[c].format);
            cursor.insertText(QString(row[c].ch));
        }
        cursor.insertBlock();
    }

    // Determine last active row in m_grid so we don't insert empty rows below prompt
    int lastActiveRow = m_grid.size() - 1;
    while (lastActiveRow > m_cursorRow) {
        bool hasText = false;
        for (const auto &cell : m_grid[lastActiveRow]) {
            if (cell.ch != ' ' || cell.format != m_defaultFormat) {
                hasText = true;
                break;
            }
        }
        if (hasText) break;
        lastActiveRow--;
    }

    // Render grid rows up to lastActiveRow
    for (int r = 0; r <= lastActiveRow && r < m_grid.size(); ++r) {
        const auto &row = m_grid[r];
        int lastNonSpace = row.size() - 1;
        while (lastNonSpace >= 0 && row[lastNonSpace].ch == ' ' && row[lastNonSpace].format == m_defaultFormat) {
            lastNonSpace--;
        }

        if (r == m_cursorRow) {
            lastNonSpace = qMax(lastNonSpace, m_cursorCol);
        }

        for (int c = 0; c <= lastNonSpace && c < row.size(); ++c) {
            QTextCharFormat fmt = row[c].format;

            // Electric blue cursor block highlight
            if (r == m_cursorRow && c == m_cursorCol) {
                fmt.setBackground(QColor("#4f8cf6"));
                fmt.setForeground(QColor("#ffffff"));
            }

            cursor.setCharFormat(fmt);
            cursor.insertText(QString(row[c].ch));
        }

        if (r < lastActiveRow) {
            cursor.insertBlock();
        }
    }

    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

} // namespace Orbit
