#include "terminal/TerminalPanel.h"
#include "terminal/TerminalWidget.h"
#include "terminal/PtyProcess.h"
#include "ui/Icons.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabBar>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QDir>
#include <QDebug>

namespace Orbit {

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent) {
    setObjectName("TerminalPanel");
    setupUi();
}

TerminalPanel::~TerminalPanel() = default;

void TerminalPanel::setupUi() {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Header toolbar bar
    auto *headerBar = new QWidget(this);
    headerBar->setFixedHeight(34);
    headerBar->setStyleSheet(R"(
        QWidget {
            background-color: #16161b;
            border-top: 1px solid #282834;
            border-bottom: 1px solid #1f1f28;
        }
    )");

    auto *headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(8, 0, 8, 0);
    headerLayout->setSpacing(4);

    // Tab bar
    m_tabBar = new QTabBar(headerBar);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setDrawBase(false);
    m_tabBar->setStyleSheet(R"(
        QTabBar {
            background: transparent;
            border: none;
        }
        QTabBar::tab {
            background: #1c1c22;
            color: #9494a0;
            border: 1px solid #282834;
            border-bottom: none;
            padding: 4px 12px;
            font-size: 11px;
            font-weight: 500;
            border-top-left-radius: 5px;
            border-top-right-radius: 5px;
            margin-right: 3px;
            margin-top: 3px;
        }
        QTabBar::tab:selected {
            background: #0d0d11;
            color: #4f8cf6;
            border: 1px solid #4f8cf6;
            font-weight: 600;
        }
        QTabBar::tab:hover:!selected {
            background: #24242c;
            color: #e4e4e7;
        }
    )");

    connect(m_tabBar, &QTabBar::currentChanged, this, &TerminalPanel::onTabChanged);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &TerminalPanel::onTabCloseRequested);

    // Toolbar buttons
    QString btnStyle = R"(
        QPushButton {
            background: transparent;
            color: #a0a0b0;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 3px 8px;
            font-size: 11px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #242430;
            color: #ffffff;
            border-color: #323242;
        }
        QPushButton:pressed {
            background-color: #2c2c3c;
        }
    )";

    m_newTabButton = new QPushButton("+ New", headerBar);
    m_newTabButton->setToolTip("Open New Terminal (Ctrl+Shift+T)");
    m_newTabButton->setStyleSheet(btnStyle);
    connect(m_newTabButton, &QPushButton::clicked, this, &TerminalPanel::onNewTerminalClicked);

    m_clearButton = new QPushButton("Clear", headerBar);
    m_clearButton->setToolTip("Clear Terminal Output");
    m_clearButton->setStyleSheet(btnStyle);
    connect(m_clearButton, &QPushButton::clicked, this, &TerminalPanel::onClearTerminalClicked);

    m_killButton = new QPushButton("Kill", headerBar);
    m_killButton->setToolTip("Kill Active Terminal Session");
    m_killButton->setStyleSheet(btnStyle);
    connect(m_killButton, &QPushButton::clicked, this, &TerminalPanel::onKillTerminalClicked);

    m_closePanelButton = new QPushButton("✕", headerBar);
    m_closePanelButton->setToolTip("Close Terminal Panel (Ctrl+~ / Ctrl+J)");
    m_closePanelButton->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            color: #71717a;
            border: none;
            border-radius: 4px;
            font-size: 12px;
            font-weight: bold;
            padding: 2px 8px;
        }
        QPushButton:hover {
            background-color: #ef4444;
            color: #ffffff;
        }
    )");
    connect(m_closePanelButton, &QPushButton::clicked, this, &TerminalPanel::closeRequested);

    headerLayout->addWidget(m_tabBar);
    headerLayout->addWidget(m_newTabButton);
    headerLayout->addStretch();
    headerLayout->addWidget(m_clearButton);
    headerLayout->addWidget(m_killButton);
    headerLayout->addWidget(m_closePanelButton);

    rootLayout->addWidget(headerBar);

    // Stacked widget hosting terminal views
    m_stackedWidget = new QStackedWidget(this);
    rootLayout->addWidget(m_stackedWidget);
}

void TerminalPanel::setWorkingDir(const QString &cwd) {
    m_workingDir = cwd;
}

TerminalWidget *TerminalPanel::addTerminalTab(const QString &cwd) {
    QString targetDir = !cwd.isEmpty() ? cwd : m_workingDir;
    if (targetDir.isEmpty()) {
        targetDir = QDir::homePath();
    }

    auto *pty = new PtyProcess(this);
    auto *termWidget = new TerminalWidget(m_stackedWidget);
    termWidget->setPtyProcess(pty);

    connect(termWidget, &TerminalWidget::toggleTerminalRequested, this, &TerminalPanel::toggleTerminalRequested);

    connect(pty, &PtyProcess::finished, this, [this, termWidget](int exitCode) {
        Q_UNUSED(exitCode);
        int idx = m_stackedWidget->indexOf(termWidget);
        if (idx != -1) {
            closeTerminalTab(idx);
        }
    });

    m_terminalCounter++;
    QString tabTitle = QString("Terminal %1").arg(m_terminalCounter);

    int pageIdx = m_stackedWidget->addWidget(termWidget);
    int tabIdx = m_tabBar->addTab(tabTitle);
    m_tabBar->setTabData(tabIdx, QVariant::fromValue(static_cast<void*>(termWidget)));

    m_tabBar->setCurrentIndex(tabIdx);
    m_stackedWidget->setCurrentIndex(pageIdx);

    pty->start(targetDir, 24, 80);
    termWidget->setFocus();

    return termWidget;
}

void TerminalPanel::closeTerminalTab(int index) {
    if (index < 0 || index >= m_tabBar->count()) return;

    auto *termWidget = qobject_cast<TerminalWidget*>(m_stackedWidget->widget(index));
    if (termWidget) {
        if (auto *pty = termWidget->ptyProcess()) {
            pty->stop();
        }
        m_stackedWidget->removeWidget(termWidget);
        termWidget->deleteLater();
    }

    m_tabBar->removeTab(index);

    if (m_tabBar->count() == 0) {
        emit closeRequested();
    }
}

TerminalWidget *TerminalPanel::currentTerminal() const {
    return qobject_cast<TerminalWidget*>(m_stackedWidget->currentWidget());
}

void TerminalPanel::onNewTerminalClicked() {
    addTerminalTab(m_workingDir);
}

void TerminalPanel::onClearTerminalClicked() {
    if (auto *term = currentTerminal()) {
        term->clearTerminal();
    }
}

void TerminalPanel::onKillTerminalClicked() {
    int idx = m_tabBar->currentIndex();
    if (idx != -1) {
        closeTerminalTab(idx);
    }
}

void TerminalPanel::onTabChanged(int index) {
    if (index >= 0 && index < m_stackedWidget->count()) {
        m_stackedWidget->setCurrentIndex(index);
        if (auto *term = currentTerminal()) {
            term->setFocus();
        }
    }
}

void TerminalPanel::onTabCloseRequested(int index) {
    closeTerminalTab(index);
}

} // namespace Orbit
