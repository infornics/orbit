#pragma once

#include "agent/AgentTypes.h"

#include <QWidget>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <functional>

class QLabel;
class QPushButton;
class QStackedWidget;
class QProgressBar;
class QWidget;

namespace Orbit {

class AcpClient;
class AntigravityInstaller;
class AgentThread;
class AgentTranscriptView;
class AgentComposer;

class AntigravityPanel : public QWidget {
    Q_OBJECT

public:
    using WorkspaceFn = std::function<QString()>;
    using BufferFn = std::function<QString(const QString &path)>;
    using FileReloadedFn = std::function<void(const QString &path)>;
    using SelectionFn = std::function<QString()>;

    explicit AntigravityPanel(QWidget *parent = nullptr);
    ~AntigravityPanel() override;

    void setWorkspaceProvider(WorkspaceFn fn);
    void setBufferProvider(BufferFn fn);
    void setFileReloadedHandler(FileReloadedFn fn);
    void setSelectionProvider(SelectionFn fn);
    void setCurrentFile(const QString &path);

    void startIfInstalled();
    void newConversation();

signals:
    void requestOpenFile(const QString &path);

private slots:
    void onInstallClicked();
    void onSignInClicked();
    void onNewChatClicked();

private:
    enum class Page { Setup, Connecting, Auth, Chat };

    void setupUi();
    void showPage(Page page);
    void connectToAgent();
    void beginSession();
    void applySessionMeta(const QJsonObject &meta);
    void setPrompting(bool prompting);
    void refreshReviewBar();
    void keepAllDiffs();
    void rejectAllDiffs();
    QString readFileForAgent(const QString &path, int line, int limit) const;
    bool writeFileForAgent(const QString &path, const QString &content);
    QString preferredAuthMethod() const;
    QString workspacePath() const;

    AcpClient *m_client;
    AntigravityInstaller *m_installer;
    AgentThread *m_thread;

    WorkspaceFn m_workspaceProvider;
    BufferFn m_bufferProvider;
    FileReloadedFn m_fileReloaded;
    SelectionFn m_selectionProvider;
    QString m_currentFilePath;

    QStackedWidget *m_stack = nullptr;
    QWidget *m_setupPage = nullptr;
    QLabel *m_setupStatus = nullptr;
    QProgressBar *m_setupProgress = nullptr;
    QPushButton *m_installButton = nullptr;
    QWidget *m_connectingPage = nullptr;
    QLabel *m_connectingLabel = nullptr;
    QWidget *m_authPage = nullptr;
    QLabel *m_authStatus = nullptr;
    QPushButton *m_signInButton = nullptr;

    QWidget *m_chatPage = nullptr;
    QLabel *m_headerStatus = nullptr;
    AgentTranscriptView *m_transcript = nullptr;
    AgentComposer *m_composer = nullptr;
    QWidget *m_reviewBar = nullptr;
    QLabel *m_reviewLabel = nullptr;

    bool m_prompting = false;
};

} // namespace Orbit
