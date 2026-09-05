#pragma once

#include <QPlainTextEdit>
#include <QWidget>

namespace Orbit {

class LineNumberArea;

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth() const;

    int currentLineNumber() const;
    int currentColumnNumber() const;

    void setEditorFontSize(int pointSize);
    int editorFontSize() const;
    void resetEditorFontSize();

signals:
    void cursorLocationChanged(int line, int col);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);
    void onCursorPositionChanged();

private:
    void indentSelection();
    void unindentSelection();
    void handleReturnKey();

    QWidget *m_lineNumberArea;
    int m_baseFontSize;
    int m_currentFontSize;
};

} // namespace Orbit
