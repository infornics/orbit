#include <QtTest/QtTest>
#include "CodeEditor.h"
#include "MainWindow.h"
#include "ExplorerPanel.h"
#include <QTemporaryDir>
#include <QTemporaryFile>

using namespace Orbit;

class OrbitTests : public QObject {
    Q_OBJECT

private slots:
    void testCodeEditorTabAndIndent();
    void testCodeEditorAutoIndent();
    void testMainWindowOpenAndSave();
    void testDirtyStateAndScreenshot();
};

void OrbitTests::testCodeEditorTabAndIndent() {
    CodeEditor editor;
    editor.setPlainText("test line");

    // Move cursor to start of line
    QTextCursor cursor = editor.textCursor();
    cursor.movePosition(QTextCursor::Start);
    editor.setTextCursor(cursor);

    // Press Tab
    QKeyEvent tabEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
    QApplication::sendEvent(&editor, &tabEvent);

    QCOMPARE(editor.toPlainText(), QString("    test line"));

    // Press Shift+Tab (Backtab)
    QKeyEvent backtabEvent(QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier);
    QApplication::sendEvent(&editor, &backtabEvent);

    QCOMPARE(editor.toPlainText(), QString("test line"));
}

void OrbitTests::testCodeEditorAutoIndent() {
    CodeEditor editor;
    editor.setPlainText("    function demo() {");

    // Move cursor to end of line
    QTextCursor cursor = editor.textCursor();
    cursor.movePosition(QTextCursor::End);
    editor.setTextCursor(cursor);

    // Press Return
    QKeyEvent enterEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(&editor, &enterEvent);

    // Should have auto-indented with previous indent + 4 spaces for '{'
    QString expected = "    function demo() {\n        ";
    QCOMPARE(editor.toPlainText(), expected);
}

void OrbitTests::testMainWindowOpenAndSave() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString filePath = tempDir.filePath("sample.txt");
    {
        QFile file(filePath);
        QVERIFY(file.open(QFile::WriteOnly | QFile::Text));
        QTextStream out(&file);
        out << "Initial Content";
    }

    MainWindow window;
    QVERIFY(window.openFile(filePath));

    // Modify file
    auto *editor = window.findChild<CodeEditor*>();
    QVERIFY(editor != nullptr);
    QCOMPARE(editor->toPlainText(), QString("Initial Content"));

    editor->setPlainText("Updated Content");

    // Verify saving
    QMetaObject::invokeMethod(&window, "onSaveFile");

    // Read back file
    QFile verifyFile(filePath);
    QVERIFY(verifyFile.open(QFile::ReadOnly | QFile::Text));
    QTextStream in(&verifyFile);
    QCOMPARE(in.readAll(), QString("Updated Content"));
}

void OrbitTests::testDirtyStateAndScreenshot() {
    MainWindow window;
    window.resize(1100, 720);
    window.show();
    window.openFolder("/home/rachit/Documents/Code/Infornics/Marketplace/Orbit");
    window.openFile("/home/rachit/Documents/Code/Infornics/Marketplace/Orbit/CMakeLists.txt");

    auto *editor = window.findChild<CodeEditor*>();
    QVERIFY(editor != nullptr);

    // Make an edit
    QTextCursor cursor = editor->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.insertText("# Orbit Project\n");

    QTest::qWait(100);

    // Verify window title contains dirty bullet •
    QVERIFY(window.windowTitle().contains("•"));

    QPixmap pixmap = window.grab();
    pixmap.save("/home/rachit/.gemini/antigravity-ide/brain/bea66796-e0e5-469a-9a79-cff795234df2/orbit_dirty.png");
}

QTEST_MAIN(OrbitTests)
#include "test_editor.moc"
