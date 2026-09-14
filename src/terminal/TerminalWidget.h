#pragma once

#include <QPlainTextEdit>
#include <QColor>
#include <QTextCharFormat>
#include <QList>

namespace Orbit {

class PtyProcess;

struct TerminalCell {
    QChar ch = ' ';
    QTextCharFormat format;
};

class TerminalWidget : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

    void setPtyProcess(PtyProcess *pty);
    PtyProcess *ptyProcess() const { return m_pty; }

    void appendData(const QByteArray &data);
    void clearTerminal();

    void zoomInFont(int range = 1);
    void zoomOutFont(int range = 1);
    void resetZoomFont();

signals:
    void toggleTerminalRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    enum ParserState {
        STATE_NORMAL,
        STATE_ESC,
        STATE_CSI,
        STATE_OSC,
        STATE_CHARSET
    };

    void processByteStream(const QString &text);
    void putChar(QChar ch);
    void newLine();
    void carriageReturn();
    void backspace();
    void tab();

    void handleSgrSequence(const QStringList &params);
    void handleCsiCommand(QChar cmd, const QString &params);
    void handleOscCommand(const QString &oscStr);
    static QColor parseAnsi256Color(int index);

    void updatePtySize();
    void renderScreenToWidget();
    void initGrid(int rows, int cols);

    PtyProcess *m_pty = nullptr;
    QTextCharFormat m_currentFormat;
    QTextCharFormat m_defaultFormat;

    ParserState m_parserState = STATE_NORMAL;
    QString m_paramBuffer;
    QString m_oscBuffer;

    int m_rows = 24;
    int m_cols = 80;
    int m_cursorRow = 0;
    int m_cursorCol = 0;

    QList<QList<TerminalCell>> m_history;
    QList<QList<TerminalCell>> m_grid;
    int m_maxHistoryLines = 1000;

    int m_savedRow = 0;
    int m_savedCol = 0;
};

} // namespace Orbit
