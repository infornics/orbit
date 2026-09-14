#pragma once

#include <QPlainTextEdit>
#include <QColor>
#include <QTextCharFormat>
#include <QTextBlockFormat>

namespace Orbit {

class PtyProcess;

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

    void processAnsiStream(const QString &text);
    void handleSgrSequence(const QStringList &params);
    void handleCsiCommand(QChar cmd, const QString &params, QTextCursor &cursor);
    void handleOscCommand(const QString &oscStr);
    static QColor parseAnsi256Color(int index);
    void updatePtySize();

    PtyProcess *m_pty = nullptr;
    QTextCharFormat m_currentFormat;
    QTextCharFormat m_defaultFormat;
    QTextBlockFormat m_blockFormat;

    ParserState m_parserState = STATE_NORMAL;
    QString m_paramBuffer;
    QString m_oscBuffer;

    int m_savedBlock = -1;
    int m_savedCol = -1;
};

} // namespace Orbit
