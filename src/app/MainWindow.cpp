#include "app/MainWindow.h"
#include "explorer/ExplorerPanel.h"
#include "editor/CodeEditor.h"
#include "ui/Icons.h"

#include <QApplication>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QStringConverter>
#include <QKeySequence>
#include <QShortcut>

namespace Orbit {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_splitter(nullptr)
    , m_explorerPanel(nullptr)
    , m_editorContainer(nullptr)
    , m_fileHeaderBar(nullptr)
    , m_filePathLabel(nullptr)
    , m_dirtyIndicatorLabel(nullptr)
    , m_editorStack(nullptr)
    , m_welcomeWidget(nullptr)
    , m_editor(nullptr)
    , m_statusMsgLabel(nullptr)
    , m_languageLabel(nullptr)
    , m_encodingLabel(nullptr)
    , m_cursorPosLabel(nullptr)
    , m_indentLabel(nullptr)
    , m_isDirty(false)
    , m_isUntitled(false)
    , m_untitledCounter(0) {

    setObjectName("MainWindow");
    setWindowTitle("Orbit");
    setWindowIcon(Icons::orbit(32));
    resize(1100, 720);

    setupUi();
    createMenus();
    createShortcuts();
    loadSettings();
    updateTitleAndHeader();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    auto *central = new QWidget(this);
    central->setObjectName("centralWidget");
    setCentralWidget(central);

    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Horizontal splitter dividing Explorer and Editor
    m_splitter = new QSplitter(Qt::Horizontal, central);
    m_splitter->setChildrenCollapsible(false);

    // Explorer Panel on the left
    m_explorerPanel = new ExplorerPanel(m_splitter);
    connect(m_explorerPanel, &ExplorerPanel::fileActivated, this, &MainWindow::openFile);
    connect(m_explorerPanel, &ExplorerPanel::requestOpenFolder, this, &MainWindow::onOpenFolder);

    // Editor container on the right
    m_editorContainer = new QWidget(m_splitter);
    m_editorContainer->setStyleSheet("background-color: #1e1e24;");
    auto *editorLayout = new QVBoxLayout(m_editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    // File header / breadcrumb bar
    m_fileHeaderBar = new QWidget(m_editorContainer);
    m_fileHeaderBar->setFixedHeight(36);
    m_fileHeaderBar->setStyleSheet(QString(R"(
        QWidget {
            background-color: #1a1a20;
            border-bottom: 1px solid #24242c;
        }
    )"));

    auto *headerLayout = new QHBoxLayout(m_fileHeaderBar);
    headerLayout->setContentsMargins(14, 0, 8, 0);
    headerLayout->setSpacing(8);

    m_dirtyIndicatorLabel = new QLabel("•", m_fileHeaderBar);
    m_dirtyIndicatorLabel->setStyleSheet("color: #4f8cf6; font-size: 16px; font-weight: bold; border: none;");
    m_dirtyIndicatorLabel->setVisible(false);

    m_filePathLabel = new QLabel(m_fileHeaderBar);
    m_filePathLabel->setStyleSheet("color: #b0b0be; font-size: 12px; font-weight: 500; border: none;");
    headerLayout->addWidget(m_dirtyIndicatorLabel);
    headerLayout->addWidget(m_filePathLabel);
    headerLayout->addStretch();

    editorLayout->addWidget(m_fileHeaderBar);
    m_fileHeaderBar->hide();

    // Editor Stack: Page 0 = Welcome Screen, Page 1 = Code Editor
    m_editorStack = new QStackedWidget(m_editorContainer);

    // --- Page 0: Welcome Screen ---
    m_welcomeWidget = new QWidget(m_editorStack);
    m_welcomeWidget->setStyleSheet("background-color: #1e1e24;");
    auto *welcomeLayout = new QVBoxLayout(m_welcomeWidget);
    welcomeLayout->setContentsMargins(40, 60, 40, 40);
    welcomeLayout->setAlignment(Qt::AlignCenter);
    welcomeLayout->setSpacing(16);

    auto *logoLabel = new QLabel(m_welcomeWidget);
    logoLabel->setPixmap(Icons::orbit(64).pixmap(64, 64));
    logoLabel->setAlignment(Qt::AlignCenter);

    auto *titleLabel = new QLabel("Orbit", m_welcomeWidget);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #ffffff; font-size: 24px; font-weight: 700; letter-spacing: 1px; border: none;");

    auto *descLabel = new QLabel(tr("A fast, focused native code and text editor"), m_welcomeWidget);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet("color: #7e7e8e; font-size: 13px; border: none;");

    auto *cardContainer = new QWidget(m_welcomeWidget);
    cardContainer->setStyleSheet(QString(R"(
        QWidget {
            background-color: #16161a;
            border: 1px solid #282832;
            border-radius: 8px;
        }
    )"));
    cardContainer->setFixedWidth(380);

    auto *cardLayout = new QVBoxLayout(cardContainer);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    cardLayout->setSpacing(12);

    auto addShortcutRow = [&](const QString &action, const QString &keys) {
        auto *row = new QWidget(cardContainer);
        row->setStyleSheet("border: none; background: transparent;");
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto *actLabel = new QLabel(action, row);
        actLabel->setStyleSheet("color: #c4c4d0; font-size: 12px; border: none;");
        auto *keyBadge = new QLabel(keys, row);
        keyBadge->setStyleSheet(QString(R"(
            QLabel {
                background-color: #23232c;
                color: #8f92a4;
                border: 1px solid #32323e;
                border-radius: 4px;
                padding: 2px 8px;
                font-family: monospace;
                font-size: 11px;
            }
        )"));
        rowLayout->addWidget(actLabel);
        rowLayout->addStretch();
        rowLayout->addWidget(keyBadge);
        cardLayout->addWidget(row);
    };

    addShortcutRow(tr("Open File"), "Ctrl + O");
    addShortcutRow(tr("Open Folder"), "Ctrl + Shift + O");
    addShortcutRow(tr("New File"), "Ctrl + N");
    addShortcutRow(tr("Save File"), "Ctrl + S");
    addShortcutRow(tr("Toggle Sidebar"), "Ctrl + B");

    welcomeLayout->addWidget(logoLabel);
    welcomeLayout->addWidget(titleLabel);
    welcomeLayout->addWidget(descLabel);
    welcomeLayout->addSpacing(10);
    welcomeLayout->addWidget(cardContainer, 0, Qt::AlignCenter);
    welcomeLayout->addStretch();

    m_editorStack->addWidget(m_welcomeWidget);

    // --- Page 1: Code Editor ---
    m_editor = new CodeEditor(m_editorStack);
    connect(m_editor, &QPlainTextEdit::textChanged, this, &MainWindow::onDocumentModified);
    connect(m_editor, &CodeEditor::cursorLocationChanged, this, &MainWindow::onCursorLocationChanged);
    connect(m_editor, &CodeEditor::languageChanged, this, [this](const QString &name) {
        if (m_languageLabel) {
            m_languageLabel->setText(name);
        }
    });

    m_editorStack->addWidget(m_editor);
    editorLayout->addWidget(m_editorStack);

    // Splitter configuration
    m_splitter->addWidget(m_explorerPanel);
    m_splitter->addWidget(m_editorContainer);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({260, 840});

    rootLayout->addWidget(m_splitter);

    // Status Bar
    auto *status = statusBar();
    m_statusMsgLabel = new QLabel(tr("Ready"), this);
    m_languageLabel = new QLabel(tr("Plain Text"), this);
    m_encodingLabel = new QLabel(tr("UTF-8 · LF"), this);
    m_cursorPosLabel = new QLabel(tr("Ln 1, Col 1"), this);
    m_indentLabel = new QLabel(tr("Spaces: 4"), this);

    status->addWidget(m_statusMsgLabel, 1);
    status->addPermanentWidget(m_languageLabel);
    status->addPermanentWidget(m_encodingLabel);
    status->addPermanentWidget(m_indentLabel);
    status->addPermanentWidget(m_cursorPosLabel);

    m_editorStack->setCurrentIndex(0);
}

void MainWindow::createMenus() {
    // --- File Menu ---
    auto *fileMenu = menuBar()->addMenu(tr("&File"));

    fileMenu->addAction(Icons::plus(16), tr("&New File"), QKeySequence::New, this, &MainWindow::onNewFile);
    fileMenu->addAction(Icons::file(16), tr("&Open File..."), QKeySequence::Open, this, &MainWindow::onOpenFile);
    fileMenu->addAction(Icons::folderOpen(16), tr("Open &Folder..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), this, &MainWindow::onOpenFolder);
    fileMenu->addSeparator();

    fileMenu->addAction(Icons::save(16), tr("&Save"), QKeySequence::Save, this, &MainWindow::onSaveFile);
    fileMenu->addAction(tr("Save &As..."), QKeySequence::SaveAs, this, &MainWindow::onSaveFileAs);
    fileMenu->addAction(tr("&Close File"), QKeySequence::Close, this, &MainWindow::onCloseFile);
    fileMenu->addSeparator();

    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

    // --- View Menu ---
    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("Toggle &Sidebar"), QKeySequence(Qt::CTRL | Qt::Key_B), this, &MainWindow::onToggleSidebar);
    viewMenu->addSeparator();
    viewMenu->addAction(tr("Zoom &In"), QKeySequence::ZoomIn, this, [this]() {
        m_editor->setEditorFontSize(m_editor->editorFontSize() + 1);
    });
    viewMenu->addAction(tr("Zoom &Out"), QKeySequence::ZoomOut, this, [this]() {
        m_editor->setEditorFontSize(m_editor->editorFontSize() - 1);
    });
    viewMenu->addAction(tr("&Reset Zoom"), QKeySequence(Qt::CTRL | Qt::Key_0), this, [this]() {
        m_editor->resetEditorFontSize();
    });

    // --- Help Menu ---
    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About Orbit"), this, &MainWindow::onAbout);
}

void MainWindow::createShortcuts() {
    // Supplementary shortcuts
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this, [this]() {
        m_editor->setEditorFontSize(m_editor->editorFontSize() + 1);
    });
}

void MainWindow::updateTitleAndHeader() {
    QString title = "Orbit";
    if (!m_currentFilePath.isEmpty()) {
        QFileInfo fi(m_currentFilePath);
        QString displayName = fi.fileName();
        if (m_isUntitled) {
            displayName = m_currentFilePath;
        }

        title = QString("%1%2 — Orbit").arg(displayName, m_isDirty ? " •" : "");
        m_filePathLabel->setText(displayName);
        m_dirtyIndicatorLabel->setVisible(m_isDirty);
    } else {
        m_filePathLabel->clear();
        m_dirtyIndicatorLabel->setVisible(false);
    }

    setWindowTitle(title);
}

bool MainWindow::maybeSave() {
    if (!m_isDirty) return true;

    QString displayName = m_isUntitled ? m_currentFilePath : QFileInfo(m_currentFilePath).fileName();
    const auto button = QMessageBox::warning(
        this,
        tr("Unsaved Changes"),
        tr("Do you want to save changes to '%1'?").arg(displayName),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save
    );

    if (button == QMessageBox::Save) {
        return onSaveFile();
    } else if (button == QMessageBox::Cancel) {
        return false;
    }

    return true; // Discard
}

void MainWindow::onNewFile() {
    if (!maybeSave()) return;

    m_isUntitled = true;
    m_currentFilePath = tr("Untitled-%1").arg(++m_untitledCounter);
    m_isDirty = false;

    m_editor->clear();
    m_editor->setFilePath(QString());
    m_editorStack->setCurrentIndex(1);
    m_fileHeaderBar->show();
    updateTitleAndHeader();
    m_editor->setFocus();
    m_statusMsgLabel->setText(tr("New file"));
}

void MainWindow::onOpenFile() {
    if (!maybeSave()) return;

    const QString startDir = m_explorerPanel->currentFolderPath().isEmpty()
                                ? QDir::homePath()
                                : m_explorerPanel->currentFolderPath();

    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open File"),
        startDir,
        tr("All Files (*);;Text Files (*.txt *.md *.cpp *.h *.c *.hpp *.py *.js *.ts *.json *.html *.css)")
    );

    if (!path.isEmpty()) {
        openFile(path);
    }
}

bool MainWindow::openFile(const QString &filePath) {
    if (filePath == m_currentFilePath && !m_isUntitled) {
        return true;
    }

    if (!maybeSave()) return false;

    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Orbit — Error"),
                             tr("Cannot open file %1:\n%2.").arg(filePath, file.errorString()));
        return false;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    const QString content = in.readAll();
    file.close();

    m_editor->setPlainText(content);
    m_editor->setFilePath(filePath);
    m_currentFilePath = filePath;
    m_isDirty = false;
    m_isUntitled = false;

    m_editorStack->setCurrentIndex(1);
    m_fileHeaderBar->show();
    updateTitleAndHeader();
    m_editor->setFocus();
    m_statusMsgLabel->setText(tr("Ready"));

    return true;
}

void MainWindow::onOpenFolder() {
    const QString startDir = m_explorerPanel->currentFolderPath().isEmpty()
                                ? QDir::homePath()
                                : m_explorerPanel->currentFolderPath();

    const QString dirPath = QFileDialog::getExistingDirectory(
        this,
        tr("Open Project Folder"),
        startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!dirPath.isEmpty()) {
        openFolder(dirPath);
    }
}

void MainWindow::openFolder(const QString &folderPath) {
    m_explorerPanel->setRootFolder(folderPath);
    m_statusMsgLabel->setText(tr("Opened folder: %1").arg(QDir(folderPath).dirName()));
}

bool MainWindow::onSaveFile() {
    if (m_isUntitled || m_currentFilePath.isEmpty()) {
        return onSaveFileAs();
    }
    return saveToFile(m_currentFilePath);
}

bool MainWindow::onSaveFileAs() {
    const QString startDir = m_explorerPanel->currentFolderPath().isEmpty()
                                ? QDir::homePath()
                                : m_explorerPanel->currentFolderPath();

    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save File As"),
        startDir + "/" + (m_isUntitled ? m_currentFilePath : QFileInfo(m_currentFilePath).fileName()),
        tr("All Files (*)")
    );

    if (path.isEmpty()) {
        return false;
    }

    return saveToFile(path);
}

bool MainWindow::saveToFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Orbit — Error"),
                             tr("Cannot save file %1:\n%2.").arg(filePath, file.errorString()));
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << m_editor->toPlainText();
    file.close();

    m_currentFilePath = filePath;
    m_editor->setFilePath(filePath);
    m_isDirty = false;
    m_isUntitled = false;

    updateTitleAndHeader();
    m_statusMsgLabel->setText(tr("Saved"));
    return true;
}

void MainWindow::onCloseFile() {
    if (!maybeSave()) return;

    m_currentFilePath.clear();
    m_isDirty = false;
    m_isUntitled = false;

    m_editor->clear();
    m_editor->setFilePath(QString());
    m_editorStack->setCurrentIndex(0);
    m_fileHeaderBar->hide();
    updateTitleAndHeader();
    m_statusMsgLabel->setText(tr("Ready"));
}

void MainWindow::onToggleSidebar() {
    m_explorerPanel->setVisible(!m_explorerPanel->isVisible());
}

void MainWindow::onDocumentModified() {
    if (m_editorStack->currentIndex() != 1) return;

    if (!m_isDirty) {
        m_isDirty = true;
        updateTitleAndHeader();
        m_statusMsgLabel->setText(tr("Modified"));
    }
}

void MainWindow::onCursorLocationChanged(int line, int col) {
    m_cursorPosLabel->setText(tr("Ln %1, Col %2").arg(line).arg(col));
}

void MainWindow::onAbout() {
    QMessageBox::about(
        this,
        tr("About Orbit"),
        tr("<h3>Orbit 0.1.0</h3>"
           "<p>A fast, focused, and elegant native code and text editor built with C++20 and Qt 6.</p>"
           "<p>Designed for distraction-free editing on Linux.</p>")
    );
}

void MainWindow::loadSettings() {
    QSettings settings("OrbitEditor", "Orbit");

    const QByteArray geometry = settings.value("geometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    const QByteArray splitterState = settings.value("splitterSizes").toByteArray();
    if (!splitterState.isEmpty()) {
        m_splitter->restoreState(splitterState);
    }

    const QString lastFolder = settings.value("lastFolder").toString();
    if (!lastFolder.isEmpty() && QDir(lastFolder).exists()) {
        openFolder(lastFolder);
    }
}

void MainWindow::saveSettings() {
    QSettings settings("OrbitEditor", "Orbit");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("splitterSizes", m_splitter->saveState());

    if (!m_explorerPanel->currentFolderPath().isEmpty()) {
        settings.setValue("lastFolder", m_explorerPanel->currentFolderPath());
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (maybeSave()) {
        saveSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

} // namespace Orbit
