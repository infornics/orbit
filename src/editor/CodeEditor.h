#pragma once

#include <QPlainTextEdit>
#include <QWidget>

namespace Orbit {

class LineNumberArea;
class SyntaxHighlighter;

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

    void setFilePath(const QString &filePath);
    QString currentLanguageName() const;
    SyntaxHighlighter* highlighter() const;

signals:
    void cursorLocationChanged(int line, int col);
    void languageChanged(const QString &languageName);

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
    SyntaxHighlighter *m_highlighter;
    int m_baseFontSize;
    int m_currentFontSize;
};

} // namespace Orbit
