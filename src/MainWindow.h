#pragma once

#include <QMainWindow>
#include <QString>

class QSplitter;
class QStackedWidget;
class QLabel;
class QPushButton;

namespace Orbit {

class ExplorerPanel;
class CodeEditor;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool openFile(const QString &filePath);
    void openFolder(const QString &folderPath);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNewFile();
    void onOpenFile();
    void onOpenFolder();
    bool onSaveFile();
    bool onSaveFileAs();
    void onCloseFile();
    void onToggleSidebar();
    void onAbout();

    void onDocumentModified();
    void onCursorLocationChanged(int line, int col);

private:
    void setupUi();
    void createMenus();
    void createShortcuts();
    void updateTitleAndHeader();
    bool maybeSave();
    bool saveToFile(const QString &filePath);

    void loadSettings();
    void saveSettings();

    // UI Widgets
    QSplitter *m_splitter;
    ExplorerPanel *m_explorerPanel;
    QWidget *m_editorContainer;
    QWidget *m_fileHeaderBar;
    QLabel *m_filePathLabel;
    QLabel *m_dirtyIndicatorLabel;
    QPushButton *m_quickSaveBtn;
    QPushButton *m_quickCloseBtn;

    QStackedWidget *m_editorStack;
    QWidget *m_welcomeWidget;
    CodeEditor *m_editor;

    // Status bar labels
    QLabel *m_statusMsgLabel;
    QLabel *m_languageLabel;
    QLabel *m_encodingLabel;
    QLabel *m_cursorPosLabel;
    QLabel *m_indentLabel;

    // State
    QString m_currentFilePath;
    bool m_isDirty;
    bool m_isUntitled;
    int m_untitledCounter;
};

} // namespace Orbit
