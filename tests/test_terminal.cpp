#include <QtTest/QtTest>
#include "terminal/PtyProcess.h"
#include "terminal/TerminalWidget.h"
#include "terminal/TerminalPanel.h"

using namespace Orbit;

class TestTerminal : public QObject {
    Q_OBJECT

private slots:
    void testPtyProcessStartStop();
    void testPtyProcessEcho();
    void testTerminalWidgetAnsiParsing();
    void testTerminalPanelAddCloseTab();
};

void TestTerminal::testPtyProcessStartStop() {
    PtyProcess pty;
    QVERIFY(!pty.isRunning());
    bool started = pty.start(QDir::homePath(), 24, 80);
    QVERIFY(started);
    QVERIFY(pty.isRunning());
    QVERIFY(pty.masterFd() >= 0);
    QVERIFY(pty.processId() > 0);

    pty.stop();
    QVERIFY(!pty.isRunning());
}

void TestTerminal::testPtyProcessEcho() {
    PtyProcess pty;
    QSignalSpy readSpy(&pty, &PtyProcess::readyRead);

    QVERIFY(pty.start(QDir::homePath(), 24, 80));

    // Wait briefly for shell prompt
    QTest::qWait(300);
    QVERIFY(readSpy.count() > 0);

    readSpy.clear();
    pty.writeInput("echo ORBIT_TERMINAL_TEST\n");

    // Wait for output
    QTest::qWait(300);

    bool foundTestOutput = false;
    for (int i = 0; i < readSpy.count(); ++i) {
        QByteArray chunk = readSpy.at(i).at(0).toByteArray();
        if (chunk.contains("ORBIT_TERMINAL_TEST")) {
            foundTestOutput = true;
            break;
        }
    }
    QVERIFY(foundTestOutput);

    pty.stop();
}

void TestTerminal::testTerminalWidgetAnsiParsing() {
    TerminalWidget widget;
    widget.appendData("\033[31mRed Text\033[0m Standard Text");

    QString plain = widget.toPlainText();
    QVERIFY(plain.contains("Red Text Standard Text"));

    widget.clearTerminal();
    QVERIFY(widget.toPlainText().isEmpty());
}

void TestTerminal::testTerminalPanelAddCloseTab() {
    TerminalPanel panel;
    panel.setWorkingDir(QDir::homePath());

    TerminalWidget *w1 = panel.addTerminalTab();
    QVERIFY(w1 != nullptr);
    QVERIFY(panel.currentTerminal() == w1);

    TerminalWidget *w2 = panel.addTerminalTab();
    QVERIFY(w2 != nullptr);
    QVERIFY(panel.currentTerminal() == w2);

    panel.closeTerminalTab(1);
    QVERIFY(panel.currentTerminal() == w1);

    panel.closeTerminalTab(0);
    QVERIFY(panel.currentTerminal() == nullptr);
}

QTEST_MAIN(TestTerminal)
#include "test_terminal.moc"
