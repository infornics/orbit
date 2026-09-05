#include "agent/AntigravityPanel.h"
#include "agent/AcpClient.h"
#include "agent/AntigravityInstaller.h"
#include "agent/AgentThread.h"
#include "agent/AgentTranscriptView.h"
#include "agent/AgentComposer.h"
#include "ui/Icons.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QProgressBar>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QStringConverter>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonArray>
#include <QJsonObject>

namespace Orbit {

AntigravityPanel::AntigravityPanel(QWidget *parent)
    : QWidget(parent)
    , m_client(new AcpClient(this))
    , m_installer(new AntigravityInstaller(this))
    , m_thread(new AgentThread(this)) {
    setObjectName("antigravityPanel");
    setMinimumWidth(340);

    m_client->setFileHandlers(
        [this](const QString &path, int line, int limit) {
            return readFileForAgent(path, line, limit);
        },
        [this](const QString &path, const QString &content) {
            return writeFileForAgent(path, content);
        });

    setupUi();

    connect(m_installer, &AntigravityInstaller::statusChanged, this, [this](const QString &msg) {
        if (m_setupStatus) m_setupStatus->setText(msg);
    });
    connect(m_installer, &AntigravityInstaller::progress, this, [this](qint64 received, qint64 total) {
        if (!m_setupProgress) return;
        m_setupProgress->setVisible(true);
        if (total > 0) {
            m_setupProgress->setMaximum(static_cast<int>(total));
            m_setupProgress->setValue(static_cast<int>(received));
        } else {
            m_setupProgress->setMaximum(0);
        }
    });
    connect(m_installer, &AntigravityInstaller::finished, this, [this](bool ok, const QString &error) {
        if (m_installButton) {
            m_installButton->setEnabled(true);
            m_installButton->setText(tr("Install Antigravity"));
        }
        if (ok) {
            connectToAgent();
        } else if (m_setupStatus) {
            m_setupStatus->setText(error);
            m_setupStatus->setStyleSheet("color: #f07178; font-size: 12px; border: none;");
        }
    });

    connect(m_client, &AcpClient::started, this, [this]() {
        if (m_connectingLabel) m_connectingLabel->setText(tr("Negotiating session…"));
        m_client->initialize();
    });
    connect(m_client, &AcpClient::initialized, this, [this](const QJsonObject &) {
        beginSession();
    });
    connect(m_client, &AcpClient::sessionMeta, this, &AntigravityPanel::applySessionMeta);
    connect(m_client, &AcpClient::sessionReady, this, [this](const QString &) {
        showPage(Page::Chat);
        m_headerStatus->setText(tr("Ready"));
        m_thread->appendSystem(tr("Antigravity is connected to this workspace. @ mention files, drop images, review edits in place."));
        if (m_composer) {
            m_composer->setWorkspace(workspacePath());
            m_composer->focusInput();
        }
    });
    connect(m_client, &AcpClient::authRequired, this, [this](const QJsonArray &) {
        showPage(Page::Auth);
        if (m_signInButton) {
            m_signInButton->setEnabled(true);
            m_signInButton->setText(tr("Sign in with Google"));
        }
    });
    connect(m_client, &AcpClient::authenticated, this, [this]() {
        if (m_authStatus) m_authStatus->setText(tr("Signed in. Opening a session…"));
        beginSession();
    });
    connect(m_client, &AcpClient::agentText, m_thread, &AgentThread::appendAgentChunk);
    connect(m_client, &AcpClient::agentThought, m_thread, &AgentThread::appendThought);
    connect(m_client, &AcpClient::toolCallUpdated, m_thread, &AgentThread::upsertToolCall);
    connect(m_client, &AcpClient::planUpdated, m_thread, &AgentThread::setPlan);
    connect(m_client, &AcpClient::terminalOutput, m_thread, &AgentThread::appendTerminalOutput);
    connect(m_client, &AcpClient::availableCommands, this, [this](const QJsonArray &commands) {
        QVector<AgentCommand> parsed;
        for (const QJsonValue &value : commands) {
            const QJsonObject obj = value.toObject();
            AgentCommand cmd;
            cmd.name = obj.value("name").toString();
            cmd.description = obj.value("description").toString();
            cmd.hint = obj.value("input").toObject().value("hint").toString();
            if (!cmd.name.isEmpty()) {
                parsed.append(cmd);
            }
        }
        if (m_composer) m_composer->setCommands(parsed);
    });
    connect(m_thread, &AgentThread::followFile, this, &AntigravityPanel::requestOpenFile);
    connect(m_thread, &AgentThread::reviewStateChanged, this, &AntigravityPanel::refreshReviewBar);

    connect(m_client, &AcpClient::promptFinished, this, [this](const QString &) {
        m_thread->closeAgentMessage();
        setPrompting(false);
        m_headerStatus->setText(tr("Ready"));
        refreshReviewBar();
    });
    connect(m_client, &AcpClient::permissionRequested, this, [this](int cookie, const QJsonObject &params) {
        m_thread->addPermission(cookie, params);
    });
    connect(m_client, &AcpClient::fileWritten, this, [this](const QString &path) {
        if (m_fileReloaded) m_fileReloaded(path);
        emit requestOpenFile(path);
        refreshReviewBar();
    });
    connect(m_client, &AcpClient::errorOccurred, this, [this](const QString &message) {
        if (m_stack && m_stack->currentWidget() == m_connectingPage) {
            showPage(Page::Setup);
            if (m_setupStatus) {
                m_setupStatus->setText(message);
                m_setupStatus->setStyleSheet("color: #f07178; font-size: 12px; border: none;");
            }
            return;
        }
        m_thread->appendSystem(message);
        setPrompting(false);
        m_headerStatus->setText(tr("Error"));
    });
    connect(m_client, &AcpClient::stopped, this, [this](int, const QString &error) {
        setPrompting(false);
        if (!error.isEmpty()) m_thread->appendSystem(error);
        m_headerStatus->setText(tr("Disconnected"));
    });
}

AntigravityPanel::~AntigravityPanel() = default;

void AntigravityPanel::setWorkspaceProvider(WorkspaceFn fn) { m_workspaceProvider = std::move(fn); }
void AntigravityPanel::setBufferProvider(BufferFn fn) { m_bufferProvider = std::move(fn); }
void AntigravityPanel::setFileReloadedHandler(FileReloadedFn fn) { m_fileReloaded = std::move(fn); }
void AntigravityPanel::setSelectionProvider(SelectionFn fn) {
    m_selectionProvider = std::move(fn);
    if (m_composer) m_composer->setSelectionProvider(m_selectionProvider);
}

void AntigravityPanel::setCurrentFile(const QString &path) {
    m_currentFilePath = path;
    if (m_composer) m_composer->setCurrentFile(path);
}

void AntigravityPanel::startIfInstalled() {
    if (m_client->isRunning()) return;
    const QString binary = m_installer->binaryPath();
    if (!binary.isEmpty() && QFileInfo(binary).isExecutable()) {
        connectToAgent();
    } else {
        showPage(Page::Setup);
    }
}

void AntigravityPanel::newConversation() { onNewChatClicked(); }

void AntigravityPanel::setupUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QWidget(this);
    header->setFixedHeight(36);
    header->setStyleSheet(QStringLiteral(
        "QWidget { background-color: #1a1a20; border-bottom: 1px solid #24242c; }"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 8, 0);
    headerLayout->setSpacing(8);

    auto *icon = new QLabel(header);
    icon->setPixmap(Icons::antigravity(16).pixmap(16, 16));
    icon->setStyleSheet("border: none; background: transparent;");
    auto *title = new QLabel(tr("Antigravity"), header);
    title->setStyleSheet("color: #eaeaf0; font-size: 12px; font-weight: 600; border: none;");
    m_headerStatus = new QLabel(tr("Idle"), header);
    m_headerStatus->setStyleSheet("color: #7e7e8e; font-size: 11px; border: none;");

    auto *newChatBtn = new QPushButton(header);
    newChatBtn->setIcon(Icons::plus(14));
    newChatBtn->setFixedSize(26, 24);
    newChatBtn->setToolTip(tr("New thread"));
    newChatBtn->setFlat(true);
    newChatBtn->setCursor(Qt::PointingHandCursor);
    newChatBtn->setStyleSheet("QPushButton { border: none; background: transparent; } QPushButton:hover { background: #2b2b35; border-radius: 4px; }");
    connect(newChatBtn, &QPushButton::clicked, this, &AntigravityPanel::onNewChatClicked);

    headerLayout->addWidget(icon);
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(m_headerStatus);
    headerLayout->addWidget(newChatBtn);

    m_stack = new QStackedWidget(this);

    // Setup
    m_setupPage = new QWidget(m_stack);
    auto *setupLayout = new QVBoxLayout(m_setupPage);
    setupLayout->setContentsMargins(20, 24, 20, 20);
    setupLayout->setSpacing(12);
    auto *setupTitle = new QLabel(tr("Google Antigravity"), m_setupPage);
    setupTitle->setStyleSheet("color: #ffffff; font-size: 16px; font-weight: 700; border: none;");
    auto *setupBody = new QLabel(
        tr("Install the official Antigravity agent. Orbit talks to it the same way Zed does — ACP — with diffs, terminals, and a real thread."),
        m_setupPage);
    setupBody->setWordWrap(true);
    setupBody->setStyleSheet("color: #9a9aaa; font-size: 12px; border: none;");
    m_setupStatus = new QLabel(m_setupPage);
    m_setupStatus->setWordWrap(true);
    m_setupStatus->setStyleSheet("color: #8f92a4; font-size: 12px; border: none;");
    m_setupProgress = new QProgressBar(m_setupPage);
    m_setupProgress->setTextVisible(false);
    m_setupProgress->setFixedHeight(4);
    m_setupProgress->setVisible(false);
    m_installButton = new QPushButton(tr("Install Antigravity"), m_setupPage);
    m_installButton->setObjectName("primaryButton");
    m_installButton->setCursor(Qt::PointingHandCursor);
    connect(m_installButton, &QPushButton::clicked, this, &AntigravityPanel::onInstallClicked);
    auto *docsLink = new QPushButton(tr("View Docs"), m_setupPage);
    docsLink->setCursor(Qt::PointingHandCursor);
    connect(docsLink, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://antigravity.google/docs/ide/extensions/")));
    });
    setupLayout->addWidget(setupTitle);
    setupLayout->addWidget(setupBody);
    setupLayout->addWidget(m_installButton);
    setupLayout->addWidget(docsLink);
    setupLayout->addWidget(m_setupProgress);
    setupLayout->addWidget(m_setupStatus);
    setupLayout->addStretch();
    m_stack->addWidget(m_setupPage);

    m_connectingPage = new QWidget(m_stack);
    auto *connectingLayout = new QVBoxLayout(m_connectingPage);
    connectingLayout->setAlignment(Qt::AlignCenter);
    m_connectingLabel = new QLabel(tr("Starting Antigravity…"), m_connectingPage);
    m_connectingLabel->setAlignment(Qt::AlignCenter);
    m_connectingLabel->setStyleSheet("color: #9a9aaa; font-size: 13px; border: none;");
    connectingLayout->addWidget(m_connectingLabel);
    m_stack->addWidget(m_connectingPage);

    m_authPage = new QWidget(m_stack);
    auto *authLayout = new QVBoxLayout(m_authPage);
    authLayout->setContentsMargins(20, 28, 20, 20);
    authLayout->setSpacing(12);
    auto *authTitle = new QLabel(tr("Sign in"), m_authPage);
    authTitle->setStyleSheet("color: #ffffff; font-size: 16px; font-weight: 700; border: none;");
    m_authStatus = new QLabel(
        tr("Complete Google sign-in in the browser. Orbit never sees your password."),
        m_authPage);
    m_authStatus->setWordWrap(true);
    m_authStatus->setStyleSheet("color: #9a9aaa; font-size: 12px; border: none;");
    m_signInButton = new QPushButton(tr("Sign in with Google"), m_authPage);
    m_signInButton->setObjectName("primaryButton");
    m_signInButton->setCursor(Qt::PointingHandCursor);
    connect(m_signInButton, &QPushButton::clicked, this, &AntigravityPanel::onSignInClicked);
    authLayout->addWidget(authTitle);
    authLayout->addWidget(m_authStatus);
    authLayout->addWidget(m_signInButton);
    authLayout->addStretch();
    m_stack->addWidget(m_authPage);

    m_chatPage = new QWidget(m_stack);
    auto *chatLayout = new QVBoxLayout(m_chatPage);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(0);

    m_transcript = new AgentTranscriptView(m_chatPage);
    m_transcript->setThread(m_thread);
    connect(m_transcript, &AgentTranscriptView::openFileRequested, this, &AntigravityPanel::requestOpenFile);
    connect(m_transcript, &AgentTranscriptView::permissionChosen, this,
            [this](int cookie, const QString &optionId, const QString &label) {
                m_thread->resolvePermission(cookie, label);
                m_client->respondToPermission(cookie, optionId);
            });

    m_reviewBar = new QWidget(m_chatPage);
    m_reviewBar->setVisible(false);
    m_reviewBar->setStyleSheet("background: #16161c; border-top: 1px solid #2a3444;");
    auto *reviewLayout = new QHBoxLayout(m_reviewBar);
    reviewLayout->setContentsMargins(12, 6, 12, 6);
    m_reviewLabel = new QLabel(m_reviewBar);
    m_reviewLabel->setStyleSheet("color: #c4c4d0; font-size: 11px; border: none; background: transparent;");
    auto *keepBtn = new QPushButton(tr("Keep all"), m_reviewBar);
    keepBtn->setObjectName("primaryButton");
    keepBtn->setCursor(Qt::PointingHandCursor);
    connect(keepBtn, &QPushButton::clicked, this, &AntigravityPanel::keepAllDiffs);
    auto *rejectBtn = new QPushButton(tr("Reject all"), m_reviewBar);
    rejectBtn->setCursor(Qt::PointingHandCursor);
    connect(rejectBtn, &QPushButton::clicked, this, &AntigravityPanel::rejectAllDiffs);
    reviewLayout->addWidget(m_reviewLabel);
    reviewLayout->addStretch();
    reviewLayout->addWidget(keepBtn);
    reviewLayout->addWidget(rejectBtn);

    m_composer = new AgentComposer(m_chatPage);
    connect(m_composer, &AgentComposer::sendRequested, this,
            [this](const QJsonArray &prompt, const QString &display) {
                if (!m_client->isRunning() || m_client->sessionId().isEmpty()) {
                    startIfInstalled();
                    m_thread->appendSystem(tr("Not connected yet."));
                    return;
                }
                m_thread->appendUser(display.isEmpty() ? tr("(attachment)") : display);
                setPrompting(true);
                m_headerStatus->setText(tr("Working"));
                m_client->sendPrompt(prompt);
            });
    connect(m_composer, &AgentComposer::stopRequested, this, [this]() {
        m_client->cancelPrompt();
    });
    connect(m_composer, &AgentComposer::modeChangeRequested, this, [this](const QString &modeId) {
        m_client->setSessionMode(modeId);
    });

    chatLayout->addWidget(m_transcript, 1);
    chatLayout->addWidget(m_reviewBar);
    chatLayout->addWidget(m_composer);
    m_stack->addWidget(m_chatPage);

    root->addWidget(header);
    root->addWidget(m_stack, 1);
    showPage(Page::Setup);
}

void AntigravityPanel::showPage(Page page) {
    switch (page) {
    case Page::Setup:
        m_stack->setCurrentWidget(m_setupPage);
        m_headerStatus->setText(tr("Setup"));
        break;
    case Page::Connecting:
        m_stack->setCurrentWidget(m_connectingPage);
        m_headerStatus->setText(tr("Connecting"));
        break;
    case Page::Auth:
        m_stack->setCurrentWidget(m_authPage);
        m_headerStatus->setText(tr("Sign in"));
        break;
    case Page::Chat:
        m_stack->setCurrentWidget(m_chatPage);
        break;
    }
}

void AntigravityPanel::onInstallClicked() {
    m_installButton->setEnabled(false);
    m_installButton->setText(tr("Installing…"));
    m_setupProgress->setVisible(true);
    m_setupProgress->setMaximum(0);
    m_setupStatus->setStyleSheet("color: #8f92a4; font-size: 12px; border: none;");
    m_installer->install();
}

void AntigravityPanel::onSignInClicked() {
    m_signInButton->setEnabled(false);
    m_signInButton->setText(tr("Waiting for browser…"));
    m_authStatus->setText(tr("Complete sign-in in your browser. Orbit continues automatically."));
    m_client->authenticate(preferredAuthMethod());
}

void AntigravityPanel::onNewChatClicked() {
    if (!m_installer->isInstalled()) {
        showPage(Page::Setup);
        return;
    }
    m_thread->clear();
    refreshReviewBar();
    connectToAgent();
}

void AntigravityPanel::connectToAgent() {
    const QString binary = m_installer->binaryPath();
    if (binary.isEmpty()) {
        showPage(Page::Setup);
        return;
    }
    showPage(Page::Connecting);
    const QFileInfo info(binary);
    m_client->start(binary, m_installer->launchArgs(), info.absolutePath());
}

void AntigravityPanel::beginSession() {
    m_client->createSession(workspacePath());
}

void AntigravityPanel::applySessionMeta(const QJsonObject &meta) {
    QVector<AgentMode> modes;
    QString current;
    const QJsonObject modeState = meta.value("modes").toObject();
    const QJsonArray available = modeState.value("availableModes").toArray();
    current = modeState.value("currentModeId").toString();
    for (const QJsonValue &value : available) {
        const QJsonObject obj = value.toObject();
        AgentMode mode;
        mode.id = obj.value("id").toString();
        mode.name = obj.value("name").toString(mode.id);
        mode.description = obj.value("description").toString();
        if (!mode.id.isEmpty()) modes.append(mode);
    }
    if (m_composer) m_composer->setModes(modes, current);
}

void AntigravityPanel::setPrompting(bool prompting) {
    m_prompting = prompting;
    if (m_composer) m_composer->setPrompting(prompting);
}

void AntigravityPanel::refreshReviewBar() {
    const QVector<AgentDiff> diffs = m_thread->pendingDiffs();
    m_reviewBar->setVisible(!diffs.isEmpty());
    if (diffs.isEmpty()) return;
    m_reviewLabel->setText(tr("%n file(s) changed", nullptr, diffs.size()));
}

void AntigravityPanel::keepAllDiffs() {
    m_thread->markDiffsReviewed();
    refreshReviewBar();
}

void AntigravityPanel::rejectAllDiffs() {
    QStringList paths;
    for (const AgentDiff &diff : m_thread->pendingDiffs()) {
        paths.append(diff.path);
    }
    const QVector<AgentDiff> restores = m_thread->takeRejectedRestores(paths);
    for (const AgentDiff &diff : restores) {
        writeFileForAgent(diff.path, diff.oldText);
        if (m_fileReloaded) m_fileReloaded(diff.path);
    }
    refreshReviewBar();
}

QString AntigravityPanel::readFileForAgent(const QString &path, int line, int limit) const {
    QString content;
    if (m_bufferProvider) {
        const QString buffered = m_bufferProvider(path);
        if (!buffered.isNull()) content = buffered;
    }
    if (content.isNull()) {
        QFile file(path);
        if (!file.open(QFile::ReadOnly | QFile::Text)) return {};
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        content = in.readAll();
    }
    if (line <= 0 && limit <= 0) return content;
    const QStringList lines = content.split('\n');
    const int start = qMax(0, line - 1);
    const int count = limit > 0 ? limit : (lines.size() - start);
    return QStringList(lines.mid(start, count)).join('\n');
}

bool AntigravityPanel::writeFileForAgent(const QString &path, const QString &content) {
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Text)) return false;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    return true;
}

QString AntigravityPanel::preferredAuthMethod() const {
    const QJsonArray methods = m_client->authMethods();
    QString first;
    for (const QJsonValue &value : methods) {
        const QString id = value.toObject().value("id").toString();
        if (id.isEmpty()) continue;
        if (first.isEmpty()) first = id;
        if (id == "oauth-personal") return id;
    }
    return first.isEmpty() ? QStringLiteral("oauth-personal") : first;
}

QString AntigravityPanel::workspacePath() const {
    if (m_workspaceProvider) {
        const QString ws = m_workspaceProvider();
        if (!ws.isEmpty()) return ws;
    }
    return QDir::homePath();
}

} // namespace Orbit
