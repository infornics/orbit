#include "CodeEditor.h"
#include "LineNumberArea.h"
#include "Theme.h"

#include <QPainter>
#include <QTextBlock>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QRegularExpression>

namespace Orbit {

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
    , m_lineNumberArea(new LineNumberArea(this))
    , m_baseFontSize(11)
    , m_currentFontSize(11) {

    // Typography
    setFont(Theme::editorFont(m_currentFontSize));
    setTabStopDistance(4 * fontMetrics().horizontalAdvance(' '));
    setLineWrapMode(QPlainTextEdit::NoWrap);

    // Modern styling
    setStyleSheet(QString(R"(
        QPlainTextEdit {
            background-color: #1e1e24;
            color: #e6e6ee;
            border: none;
            selection-background-color: #2b3e64;
            selection-color: #ffffff;
            padding-top: 4px;
        }
    )"));

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::onCursorPositionChanged);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

int CodeEditor::lineNumberAreaWidth() const {
    int digits = 1;
    int maxLines = qMax(1, blockCount());
    while (maxLines >= 10) {
        maxLines /= 10;
        ++digits;
    }
    // Minimum 3 digits width for visual stability
    digits = qMax(3, digits);
    const int padding = 20;
    return padding + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy != 0) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void CodeEditor::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);

    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(Theme::CurrentLineBg);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void CodeEditor::onCursorPositionChanged() {
    emit cursorLocationChanged(currentLineNumber(), currentColumnNumber());
    m_lineNumberArea->update();
}

int CodeEditor::currentLineNumber() const {
    return textCursor().blockNumber() + 1;
}

int CodeEditor::currentColumnNumber() const {
    return textCursor().positionInBlock() + 1;
}

void CodeEditor::setEditorFontSize(int pointSize) {
    pointSize = qBound(8, pointSize, 32);
    if (m_currentFontSize != pointSize) {
        m_currentFontSize = pointSize;
        setFont(Theme::editorFont(m_currentFontSize));
        setTabStopDistance(4 * fontMetrics().horizontalAdvance(' '));
        updateLineNumberAreaWidth(0);
        m_lineNumberArea->update();
    }
}

int CodeEditor::editorFontSize() const {
    return m_currentFontSize;
}

void CodeEditor::resetEditorFontSize() {
    setEditorFontSize(m_baseFontSize);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), Theme::GutterBg);

    // Subtle right border
    painter.setPen(Theme::BorderSubtle);
    painter.drawLine(event->rect().topRight(), event->rect().bottomRight());

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    const int currentLine = textCursor().blockNumber();

    painter.setFont(font());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const QString number = QString::number(blockNumber + 1);
            const bool isCurrent = (blockNumber == currentLine);

            painter.setPen(isCurrent ? Theme::GutterActiveFg : Theme::GutterFg);
            if (isCurrent) {
                QFont boldFont = font();
                boldFont.setBold(true);
                painter.setFont(boldFont);
            } else {
                painter.setFont(font());
            }

            // Draw line number with right-aligned margin
            painter.drawText(0, top, m_lineNumberArea->width() - 10, fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditor::keyPressEvent(QKeyEvent *event) {
    // Zoom shortcuts: Ctrl + '=', Ctrl + '+', Ctrl + '-', Ctrl + '0'
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
            setEditorFontSize(m_currentFontSize + 1);
            event->accept();
            return;
        } else if (event->key() == Qt::Key_Minus) {
            setEditorFontSize(m_currentFontSize - 1);
            event->accept();
            return;
        } else if (event->key() == Qt::Key_0) {
            resetEditorFontSize();
            event->accept();
            return;
        }
    }

    // Tab key -> 4 spaces or indent selection
    if (event->key() == Qt::Key_Tab) {
        if (textCursor().hasSelection()) {
            indentSelection();
        } else {
            textCursor().insertText("    ");
        }
        event->accept();
        return;
    }

    // Shift + Tab (Backtab) -> unindent
    if (event->key() == Qt::Key_Backtab) {
        unindentSelection();
        event->accept();
        return;
    }

    // Return / Enter -> Auto indentation
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        handleReturnKey();
        event->accept();
        return;
    }

    // Backspace: intelligent 4-space deletion if indentation is spaces
    if (event->key() == Qt::Key_Backspace && !textCursor().hasSelection()) {
        QTextCursor cursor = textCursor();
        int posInBlock = cursor.positionInBlock();
        if (posInBlock >= 4) {
            QString blockText = cursor.block().text().left(posInBlock);
            if (blockText.endsWith("    ") && blockText.trimmed().isEmpty()) {
                cursor.beginEditBlock();
                for (int i = 0; i < 4; ++i) {
                    cursor.deletePreviousChar();
                }
                cursor.endEditBlock();
                setTextCursor(cursor);
                event->accept();
                return;
            }
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}

void CodeEditor::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        const int delta = event->angleDelta().y();
        if (delta > 0) {
            setEditorFontSize(m_currentFontSize + 1);
        } else if (delta < 0) {
            setEditorFontSize(m_currentFontSize - 1);
        }
        event->accept();
        return;
    }

    QPlainTextEdit::wheelEvent(event);
}

void CodeEditor::indentSelection() {
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();

    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();

    QTextBlock startBlock = document()->findBlock(start);
    QTextBlock endBlock = document()->findBlock(end);

    QTextBlock block = startBlock;
    while (block.isValid()) {
        QTextCursor blockCursor(block);
        blockCursor.movePosition(QTextCursor::StartOfBlock);
        blockCursor.insertText("    ");

        if (block == endBlock) break;
        block = block.next();
    }

    cursor.endEditBlock();
}

void CodeEditor::unindentSelection() {
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();

    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();

    QTextBlock startBlock = document()->findBlock(start);
    QTextBlock endBlock = document()->findBlock(end);

    QTextBlock block = startBlock;
    while (block.isValid()) {
        QString text = block.text();
        int spacesToRemove = 0;
        while (spacesToRemove < 4 && spacesToRemove < text.length() && text.at(spacesToRemove) == ' ') {
            ++spacesToRemove;
        }

        if (spacesToRemove > 0) {
            QTextCursor blockCursor(block);
            blockCursor.movePosition(QTextCursor::StartOfBlock);
            for (int i = 0; i < spacesToRemove; ++i) {
                blockCursor.deleteChar();
            }
        }

        if (block == endBlock) break;
        block = block.next();
    }

    cursor.endEditBlock();
}

void CodeEditor::handleReturnKey() {
    QTextCursor cursor = textCursor();
    QString lineText = cursor.block().text();
    int posInBlock = cursor.positionInBlock();
    QString prefix = lineText.left(posInBlock);

    // Extract leading whitespace
    int indentCount = 0;
    while (indentCount < prefix.length() && (prefix.at(indentCount) == ' ' || prefix.at(indentCount) == '\t')) {
        ++indentCount;
    }
    QString indent = prefix.left(indentCount);

    // If line ended with '{', add an extra 4 spaces of indentation
    if (prefix.trimmed().endsWith('{')) {
        indent += "    ";
    }

    cursor.beginEditBlock();
    cursor.insertText("\n" + indent);
    cursor.endEditBlock();
    setTextCursor(cursor);
    ensureCursorVisible();
}

} // namespace Orbit
