#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <functional>

class QProcess;

namespace Orbit {

class AgentTerminalHost;

class AcpClient : public QObject {
    Q_OBJECT

public:
    using ReadFileFn = std::function<QString(const QString &path, int line, int limit)>;
    using WriteFileFn = std::function<bool(const QString &path, const QString &content)>;

    explicit AcpClient(QObject *parent = nullptr);
    ~AcpClient() override;

    void setFileHandlers(ReadFileFn reader, WriteFileFn writer);

    void start(const QString &command, const QStringList &args, const QString &workingDirectory);
    void stop();
    bool isRunning() const;

    void initialize();
    void authenticate(const QString &methodId);
    void createSession(const QString &cwd);
    void sendPrompt(const QJsonArray &prompt);
    void cancelPrompt();
    void setSessionMode(const QString &modeId);

    QString sessionId() const { return m_sessionId; }
    QJsonArray authMethods() const { return m_authMethods; }
    QJsonObject lastSessionMeta() const { return m_sessionMeta; }
    bool isPrompting() const { return m_prompting; }

    void respondToPermission(int requestId, const QString &optionId);
    void cancelPermission(int requestId);

    static QString extractText(const QJsonValue &content);

signals:
    void started();
    void stopped(int exitCode, const QString &error);
    void initialized(const QJsonObject &result);
    void authenticated();
    void sessionReady(const QString &sessionId);
    void sessionMeta(const QJsonObject &result);
    void agentText(const QString &text);
    void agentThought(const QString &text);
    void toolCallUpdated(const QJsonObject &update);
    void planUpdated(const QJsonObject &plan);
    void availableCommands(const QJsonArray &commands);
    void promptFinished(const QString &stopReason);
    void permissionRequested(int requestId, const QJsonObject &params);
    void terminalOutput(const QString &terminalId, const QString &chunk);
    void authRequired(const QJsonArray &methods);
    void errorOccurred(const QString &message);
    void fileWritten(const QString &path);
    void stderrMessage(const QString &line);

private:
    void sendRequest(const QString &method, const QJsonObject &params,
                     std::function<void(const QJsonObject &result, const QJsonObject &error)> callback);
    void sendNotification(const QString &method, const QJsonObject &params);
    void sendResponse(const QJsonValue &id, const QJsonObject &result);
    void sendError(const QJsonValue &id, int code, const QString &message);
    void writeMessage(const QJsonObject &message);
    void onReadyRead();
    void onReadyReadStderr();
    void handleMessage(const QJsonObject &message);
    void handleAgentRequest(const QJsonObject &message);
    void handleSessionUpdate(const QJsonObject &params);
    void handleReadTextFile(const QJsonValue &id, const QJsonObject &params);
    void handleWriteTextFile(const QJsonValue &id, const QJsonObject &params);
    void handleRequestPermission(const QJsonValue &id, const QJsonObject &params);
    void handleTerminalCreate(const QJsonValue &id, const QJsonObject &params);
    void handleTerminalOutput(const QJsonValue &id, const QJsonObject &params);
    void handleTerminalWait(const QJsonValue &id, const QJsonObject &params);
    void handleTerminalKill(const QJsonValue &id, const QJsonObject &params);
    void handleTerminalRelease(const QJsonValue &id, const QJsonObject &params);
    bool isAuthError(const QJsonObject &error) const;

    QProcess *m_process = nullptr;
    AgentTerminalHost *m_terminals = nullptr;
    QByteArray m_stdoutBuffer;
    int m_nextId = 1;
    QHash<int, std::function<void(const QJsonObject &, const QJsonObject &)>> m_pending;
    QString m_sessionId;
    QJsonArray m_authMethods;
    QJsonObject m_sessionMeta;
    QHash<int, QJsonValue> m_permissionIds;
    int m_permCookie = 1;
    bool m_prompting = false;
    bool m_waitingForIdle = false;
    ReadFileFn m_readFile;
    WriteFileFn m_writeFile;
};

} // namespace Orbit
