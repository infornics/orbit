#include <QtTest/QtTest>
#include "editor/CodeEditor.h"
#include "editor/SyntaxHighlighter.h"
#include "app/MainWindow.h"
#include "explorer/ExplorerPanel.h"
#include "ui/Theme.h"
#include "agent/AcpClient.h"
#include "agent/AntigravityInstaller.h"
#include "agent/AntigravityPanel.h"
#include "agent/AgentThread.h"
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTreeView>
#include <QFileSystemModel>
#include <QLabel>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>

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
    void testUnsavedChangesDialog();
    void testRevertChangesClearsModified();
    void testAntigravityInstallerRegistryParse();
    void testAcpClientExtractText();
    void testAcpClientMockHandshake();
    void testAntigravityPanelToggle();
    void testAgentThreadToolUpsertAndDiffs();
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

    // Revert edit with undo
    editor->undo();
    QTest::qWait(100);
    QVERIFY(!window.windowTitle().contains("•"));

    // Revert edit manually (typing and then deleting)
    cursor.movePosition(QTextCursor::Start);
    cursor.insertText("xyz");
    QTest::qWait(100);
    QVERIFY(window.windowTitle().contains("•"));

    cursor.movePosition(QTextCursor::Start);
    for (int i = 0; i < 3; ++i) {
        cursor.deleteChar();
    }
    QTest::qWait(100);
    QVERIFY(!window.windowTitle().contains("•"));
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

void OrbitTests::testUnsavedChangesDialog() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("package.json");
    {
        QFile file(filePath);
        QVERIFY(file.open(QFile::WriteOnly | QFile::Text));
        file.write("{\n  \"name\": \"orbit\"\n}\n");
    }

    MainWindow window;
    window.resize(900, 600);
    window.show();
    QVERIFY(window.openFile(filePath));

    auto *editor = window.findChild<CodeEditor*>();
    QVERIFY(editor != nullptr);
    editor->insertPlainText(" "); // Make dirty

    // Schedule screenshot & dismiss via Cancel
    QTimer::singleShot(300, [&]() {
        QWidget *modal = QApplication::activeModalWidget();
        if (modal) {
            QPixmap pix = modal->grab();
            pix.save("/home/rachit/.gemini/antigravity-ide/brain/b746bfe9-4557-459b-a6f6-33d305091097/unsaved_changes_clean.png");
            modal->close();
        }
    });

    window.close();
}

void OrbitTests::testRevertChangesClearsModified() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("revert_test.txt");
    {
        QFile file(filePath);
        QVERIFY(file.open(QFile::WriteOnly | QFile::Text));
        QTextStream out(&file);
        out << "Hello World";
    }

    MainWindow window;
    QVERIFY(window.openFile(filePath));
    QVERIFY(!window.windowTitle().contains("•"));

    auto *editor = window.findChild<CodeEditor*>();
    QVERIFY(editor != nullptr);

    // 1. Edit via typing
    editor->insertPlainText(" Extra");
    QVERIFY(window.windowTitle().contains("•"));

    // 2. Undo revert
    editor->undo();
    QCOMPARE(editor->toPlainText(), QString("Hello World"));
    QVERIFY(!window.windowTitle().contains("•"));

    // 3. Manual typing revert (type and then backspace)
    editor->insertPlainText("!");
    QVERIFY(window.windowTitle().contains("•"));
    QTextCursor c = editor->textCursor();
    c.deletePreviousChar();
    editor->setTextCursor(c);
    QCOMPARE(editor->toPlainText(), QString("Hello World"));
    QVERIFY(!window.windowTitle().contains("•"));
}

void OrbitTests::testAntigravityInstallerRegistryParse() {
    const QByteArray registry = R"({
      "version": 1,
      "agents": [
        {
          "id": "antigravity-acp",
          "name": "Google Antigravity",
          "distribution": {
            "binary": {
              "linux-x86_64": {
                "archive": "https://example.test/linux.zip",
                "cmd": "./agy_acp_server.par",
                "args": ["--uid="]
              },
              "linux-aarch64": {
                "archive": "https://example.test/linux-arm.zip",
                "cmd": "./agy_acp_server.par",
                "args": ["--uid="]
              },
              "darwin-aarch64": {
                "archive": "https://example.test/mac.zip",
                "cmd": "./agy_acp_server.par"
              },
              "darwin-x86_64": {
                "archive": "https://example.test/mac-x64.zip",
                "cmd": "./agy_acp_server.par"
              },
              "windows-x86_64": {
                "archive": "https://example.test/win.zip",
                "cmd": "./agy_acp_server.exe"
              },
              "windows-aarch64": {
                "archive": "https://example.test/win-arm.zip",
                "cmd": "./agy_acp_server.exe"
              }
            }
          }
        }
      ]
    })";

    const AntigravityPlatformSpec spec = AntigravityInstaller::specFromRegistry(registry);
    QVERIFY(!spec.archiveUrl.isEmpty());
    QVERIFY(spec.archiveUrl.startsWith("https://example.test/"));
    QVERIFY(spec.command.contains("agy_acp_server"));

    const AntigravityPlatformSpec fallback = AntigravityInstaller::fallbackSpec();
    QVERIFY(fallback.archiveUrl.contains("agy-acp-server"));
    QVERIFY(!AntigravityInstaller::installDirectory().isEmpty());
}

void OrbitTests::testAcpClientExtractText() {
    QCOMPARE(AcpClient::extractText(QJsonValue("plain")), QString("plain"));

    QJsonObject block{{"type", "text"}, {"text", "chunk"}};
    QCOMPARE(AcpClient::extractText(block), QString("chunk"));

    QJsonArray parts{
        QJsonObject{{"type", "text"}, {"text", "hel"}},
        QJsonObject{{"type", "text"}, {"text", "lo"}}
    };
    QCOMPARE(AcpClient::extractText(parts), QString("hello"));
}

void OrbitTests::testAcpClientMockHandshake() {
    const QString mock = QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("mock_acp_server.py");
    QVERIFY(QFileInfo::exists(mock));

    AcpClient client;
    QSignalSpy startedSpy(&client, &AcpClient::started);
    QSignalSpy initSpy(&client, &AcpClient::initialized);
    QSignalSpy sessionSpy(&client, &AcpClient::sessionReady);
    QSignalSpy textSpy(&client, &AcpClient::agentText);
    QSignalSpy doneSpy(&client, &AcpClient::promptFinished);
    QSignalSpy errorSpy(&client, &AcpClient::errorOccurred);

    client.start(QStringLiteral("python3"), QStringList{QStringLiteral("-u"), mock}, QString());
    QVERIFY(startedSpy.wait(3000));

    client.initialize();
    QVERIFY(initSpy.wait(3000));
    QCOMPARE(errorSpy.count(), 0);

    client.createSession(QDir::tempPath());
    QVERIFY(sessionSpy.wait(3000));
    QCOMPARE(client.sessionId(), QString("sess_test"));

    QJsonArray prompt{QJsonObject{{"type", "text"}, {"text", "hi"}}};
    client.sendPrompt(prompt);
    QVERIFY(textSpy.wait(3000));
    QCOMPARE(textSpy.takeFirst().at(0).toString(), QString("hello from mock"));
    QVERIFY(doneSpy.wait(3000) || doneSpy.count() > 0);

    client.stop();
}

void OrbitTests::testAntigravityPanelToggle() {
    MainWindow window;
    window.resize(1100, 720);
    window.show();

    auto *panel = window.findChild<AntigravityPanel *>();
    QVERIFY(panel != nullptr);
    QVERIFY(!panel->isVisible());

    QMetaObject::invokeMethod(&window, "onToggleAntigravity");
    QTest::qWait(50);
    QVERIFY(panel->isVisible());

    QMetaObject::invokeMethod(&window, "onToggleAntigravity");
    QTest::qWait(50);
    QVERIFY(!panel->isVisible());
}

void OrbitTests::testAgentThreadToolUpsertAndDiffs() {
    AgentThread thread;
    QCOMPARE(thread.count(), 0);

    thread.appendUser("refactor this");
    thread.appendAgentChunk("I'll edit the file.");
    thread.appendAgentChunk(" Next chunk.");
    QCOMPARE(thread.count(), 2);
    QCOMPARE(thread.at(1).text, QString("I'll edit the file. Next chunk."));

    QJsonObject tool{
        {"toolCallId", "call_1"},
        {"title", "Editing MainWindow.cpp"},
        {"kind", "edit"},
        {"status", "in_progress"},
        {"locations", QJsonArray{QJsonObject{{"path", "/tmp/MainWindow.cpp"}}}},
        {"content", QJsonArray{QJsonObject{
            {"type", "diff"},
            {"path", "/tmp/MainWindow.cpp"},
            {"oldText", "int a = 1;"},
            {"newText", "int a = 2;"}
        }}}
    };
    thread.upsertToolCall(tool);
    QCOMPARE(thread.count(), 3);
    QCOMPARE(thread.at(2).tool.title, QString("Editing MainWindow.cpp"));
    QCOMPARE(thread.pendingDiffs().size(), 1);

    thread.upsertToolCall(QJsonObject{
        {"toolCallId", "call_1"},
        {"status", "completed"}
    });
    QCOMPARE(thread.at(2).tool.status, QString("completed"));
    QCOMPARE(thread.at(2).tool.title, QString("Editing MainWindow.cpp"));

    thread.markDiffsReviewed();
    QCOMPARE(thread.pendingDiffs().size(), 0);
}

QTEST_MAIN(OrbitTests)
#include "test_editor.moc"
