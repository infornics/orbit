#include <QtTest/QtTest>
#include "editor/CodeEditor.h"
#include "editor/SyntaxHighlighter.h"
#include "app/MainWindow.h"
#include "explorer/ExplorerPanel.h"
#include "ui/Theme.h"
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTreeView>
#include <QFileSystemModel>
#include <QLabel>

using namespace Orbit;

class OrbitTests : public QObject {
    Q_OBJECT

private slots:
    void testCodeEditorTabAndIndent();
    void testCodeEditorAutoIndent();
    void testMainWindowOpenAndSave();
    void testDirtyStateAndScreenshot();
    void testExplorerFolderSingleClickExpand();
    void testSyntaxHighlighter();
    void testAutoSave();
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

void OrbitTests::testExplorerFolderSingleClickExpand() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir dir(tempDir.path());
    QVERIFY(dir.mkdir("subfolder"));
    {
        QFile file(dir.filePath("subfolder/child.txt"));
        QVERIFY(file.open(QFile::WriteOnly));
        file.write("hello");
    }

    ExplorerPanel panel;
    panel.setStyleSheet(Theme::applicationStyleSheet());
    panel.resize(300, 500);
    panel.show();
    panel.setRootFolder(tempDir.path());

    auto *treeView = panel.findChild<QTreeView*>();
    QVERIFY(treeView != nullptr);
    auto *model = panel.findChild<QFileSystemModel*>();
    QVERIFY(model != nullptr);

    // Give the file system model a moment to discover directory contents
    QTest::qWait(150);

    QModelIndex folderIndex = model->index(dir.filePath("subfolder"));
    QVERIFY(folderIndex.isValid());

    // Initially not expanded
    QVERIFY(!treeView->isExpanded(folderIndex));

    // Simulate single click on folder
    QMetaObject::invokeMethod(&panel, "onItemClicked", Q_ARG(QModelIndex, folderIndex));
    QTest::qWait(50);

    // Verify it expanded on single click
    QVERIFY(treeView->isExpanded(folderIndex));

    treeView->setCurrentIndex(folderIndex);
    if (treeView->selectionModel()) {
        treeView->selectionModel()->select(folderIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    QTest::qWait(50);
    QPixmap pixmap = panel.grab();
    pixmap.save("/home/rachit/.gemini/antigravity-ide/brain/b746bfe9-4557-459b-a6f6-33d305091097/explorer_unified.png");

    // Simulate second single click
    QMetaObject::invokeMethod(&panel, "onItemClicked", Q_ARG(QModelIndex, folderIndex));
    QTest::qWait(50);

    // Verify it collapsed
    QVERIFY(!treeView->isExpanded(folderIndex));
}

void OrbitTests::testSyntaxHighlighter() {
    // 1. Test language detection
    QCOMPARE(SyntaxHighlighter::detectLanguage("index.ts"), Language::TypeScript);
    QCOMPARE(SyntaxHighlighter::detectLanguage("Component.tsx"), Language::TypeScript);
    QCOMPARE(SyntaxHighlighter::detectLanguage("server.js"), Language::JavaScript);
    QCOMPARE(SyntaxHighlighter::detectLanguage("main.cpp"), Language::Cpp);
    QCOMPARE(SyntaxHighlighter::detectLanguage("header.hpp"), Language::Cpp);
    QCOMPARE(SyntaxHighlighter::detectLanguage("script.py"), Language::Python);
    QCOMPARE(SyntaxHighlighter::detectLanguage("package.json"), Language::Json);
    QCOMPARE(SyntaxHighlighter::detectLanguage("index.html"), Language::Xml);
    QCOMPARE(SyntaxHighlighter::detectLanguage("resources.qrc"), Language::Xml);
    QCOMPARE(SyntaxHighlighter::detectLanguage("styles.css"), Language::Css);
    QCOMPARE(SyntaxHighlighter::detectLanguage("README.md"), Language::Markdown);
    QCOMPARE(SyntaxHighlighter::detectLanguage("deploy.yaml"), Language::Yaml);
    QCOMPARE(SyntaxHighlighter::detectLanguage("build.sh"), Language::Bash);
    QCOMPARE(SyntaxHighlighter::detectLanguage("CMakeLists.txt"), Language::CMake);
    QCOMPARE(SyntaxHighlighter::detectLanguage("notes.txt"), Language::None);

    // 2. Test language names
    QCOMPARE(SyntaxHighlighter::languageName(Language::TypeScript), QString("TypeScript"));
    QCOMPARE(SyntaxHighlighter::languageName(Language::Cpp), QString("C++"));
    QCOMPARE(SyntaxHighlighter::languageName(Language::Python), QString("Python"));
    QCOMPARE(SyntaxHighlighter::languageName(Language::None), QString("Plain Text"));

    // 3. Test CodeEditor highlighting
    CodeEditor editor;
    editor.setFilePath("index.ts");
    QCOMPARE(editor.currentLanguageName(), QString("TypeScript"));

    editor.setPlainText("import dns from \"dns\";\n// Comment\nconst port = 8000;");
    QTextBlock block0 = editor.document()->firstBlock();
    QList<QTextLayout::FormatRange> formats0 = block0.layout()->formats();
    QVERIFY(!formats0.isEmpty());

    // 4. Test MainWindow integration with CMake
    MainWindow window;
    window.resize(1100, 720);
    window.show();
    QVERIFY(window.openFile("/home/rachit/Documents/Code/Infornics/Marketplace/Orbit/CMakeLists.txt"));
    QTest::qWait(100);

    QPixmap pixmap = window.grab();
    pixmap.save("/home/rachit/.gemini/antigravity-ide/brain/b746bfe9-4557-459b-a6f6-33d305091097/syntax_highlighting_demo.png");

    // 5. Test TypeScript file highlighting
    QTemporaryFile tsFile(QDir::temp().filePath("index.XXXXXX.ts"));
    QVERIFY(tsFile.open());
    {
        QTextStream out(&tsFile);
        out << "import dns from \"dns\";\n"
            << "dns.setServers([\"8.8.8.8\", \"8.8.4.4\"]);\n\n"
            << "import express from 'express';\n"
            << "import dotenv from 'dotenv';\n"
            << "import cors from 'cors';\n\n"
            << "// Server Port\n"
            << "const port: number = 8000;\n"
            << "app.get('/health', (req, res) => {\n"
            << "    res.status(200).json({ success: true, message: 'Server is running' });\n"
            << "});\n";
    }
    tsFile.close();

    QVERIFY(window.openFile(tsFile.fileName()));
    QTest::qWait(100);
    QPixmap tsPixmap = window.grab();
    tsPixmap.save("/home/rachit/.gemini/antigravity-ide/brain/b746bfe9-4557-459b-a6f6-33d305091097/typescript_syntax_demo.png");
}

void OrbitTests::testAutoSave() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString filePath = tempDir.filePath("autosave_test.txt");
    {
        QFile file(filePath);
        QVERIFY(file.open(QFile::WriteOnly | QFile::Text));
        QTextStream out(&file);
        out << "Version 1";
    }

    MainWindow window;
    QVERIFY(window.openFile(filePath));

    // Enable Auto Save
    QMetaObject::invokeMethod(&window, "onToggleAutoSave", Q_ARG(bool, true));

    auto *editor = window.findChild<CodeEditor*>();
    QVERIFY(editor != nullptr);
    editor->setPlainText("Version 2 Auto Saved");

    // Wait for auto save debounce (1000ms + margin)
    QTest::qWait(1300);

    // Verify file content on disk has updated
    QFile verifyFile(filePath);
    QVERIFY(verifyFile.open(QFile::ReadOnly | QFile::Text));
    QTextStream in(&verifyFile);
    QCOMPARE(in.readAll(), QString("Version 2 Auto Saved"));

    // Verify window dirty status is false
    QVERIFY(!window.windowTitle().contains("•"));
}

QTEST_MAIN(OrbitTests)
#include "test_editor.moc"
