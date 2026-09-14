#pragma once

#include <QWidget>
#include <QString>

class QTabBar;
class QStackedWidget;
class QPushButton;

namespace Orbit {

class TerminalWidget;
class PtyProcess;

class TerminalPanel : public QWidget {
    Q_OBJECT

public:
    explicit TerminalPanel(QWidget *parent = nullptr);
    ~TerminalPanel() override;

    TerminalWidget *addTerminalTab(const QString &cwd = QString());
    void closeTerminalTab(int index);
    void setWorkingDir(const QString &cwd);
    QString workingDir() const { return m_workingDir; }

    TerminalWidget *currentTerminal() const;

signals:
    void closeRequested();
    void toggleTerminalRequested();

public slots:
    void onNewTerminalClicked();
    void onClearTerminalClicked();
    void onKillTerminalClicked();

private slots:
    void onTabChanged(int index);
    void onTabCloseRequested(int index);

private:
    void setupUi();

    QTabBar *m_tabBar = nullptr;
    QStackedWidget *m_stackedWidget = nullptr;
    QPushButton *m_newTabButton = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPushButton *m_killButton = nullptr;
    QPushButton *m_closePanelButton = nullptr;

    QString m_workingDir;
    int m_terminalCounter = 0;
};

} // namespace Orbit
