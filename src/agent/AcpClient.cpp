#include "agent/AcpClient.h"
#include "agent/AgentTerminalHost.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStringConverter>

namespace Orbit {

AcpClient::AcpClient(QObject *parent)
    : QObject(parent)
    , m_terminals(new AgentTerminalHost(this)) {
    connect(m_terminals, &AgentTerminalHost::outputAppended,
            this, &AcpClient::terminalOutput);
}

AcpClient::~AcpClient() {
    stop();
}

void AcpClient::setFileHandlers(ReadFileFn reader, WriteFileFn writer) {
    m_readFile = std::move(reader);
    m_writeFile = std::move(writer);
}

void AcpClient::start(const QString &command, const QStringList &args, const QString &workingDirectory) {
    stop();

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    if (!workingDirectory.isEmpty()) {
        m_process->setWorkingDirectory(workingDirectory);
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!workingDirectory.isEmpty()) {
        const QString path = env.value("PATH");
        env.insert("PATH", workingDirectory + ":" + path);
    }
    m_process->setProcessEnvironment(env);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &AcpClient::onReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &AcpClient::onReadyReadStderr);
    connect(m_process, &QProcess::started, this, &AcpClient::started);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            emit errorOccurred(tr("Failed to start Antigravity ACP server: %1")
                                   .arg(m_process ? m_process->errorString() : QString()));
        }
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus status) {
                const QString err = (status == QProcess::CrashExit)
                                        ? tr("Antigravity ACP server crashed")
                                        : QString();
                m_sessionId.clear();
                m_prompting = false;
                m_pending.clear();
                emit stopped(code, err);
            });

    m_stdoutBuffer.clear();
    m_nextId = 1;
    m_pending.clear();
    m_sessionId.clear();
    m_authMethods = QJsonArray();
    m_prompting = false;
    m_waitingForIdle = false;

    m_process->start(command, args);
}

void AcpClient::stop() {
    if (!m_process) {
        return;
    }

    QProcess *proc = m_process;
    m_process = nullptr;
    m_pending.clear();
    m_sessionId.clear();
    m_prompting = false;
    m_permissionIds.clear();
    if (m_terminals) {
        m_terminals->reset();
    }

    proc->disconnect(this);
    if (proc->state() != QProcess::NotRunning) {
        proc->terminate();
        if (!proc->waitForFinished(1500)) {
            proc->kill();
            proc->waitForFinished(500);
        }
    }
    proc->deleteLater();
}

bool AcpClient::isRunning() const {
    return m_process && m_process->state() != QProcess::NotRunning;
}

void AcpClient::initialize() {
    QJsonObject fs;
    fs.insert("readTextFile", true);
    fs.insert("writeTextFile", true);

    QJsonObject capabilities;
    capabilities.insert("fs", fs);
    capabilities.insert("terminal", true);

    QJsonObject info{
        {"name", "orbit"},
        {"title", "Orbit"},
        {"version", "0.1.0"}
    };

    QJsonObject params;
    params.insert("protocolVersion", 1);
    params.insert("clientCapabilities", capabilities);
    params.insert("clientInfo", info);
    // v2 aliases for newer servers
    params.insert("capabilities", QJsonObject{
        {"fs", QJsonObject{{"readTextFile", QJsonObject()}, {"writeTextFile", QJsonObject()}}},
        {"terminal", QJsonObject()}
    });
    params.insert("info", info);

    sendRequest("initialize", params, [this](const QJsonObject &result, const QJsonObject &error) {
        if (!error.isEmpty()) {
            emit errorOccurred(error.value("message").toString(tr("Initialize failed")));
            return;
        }

        m_authMethods = result.value("authMethods").toArray();
        if (m_authMethods.isEmpty()) {
            const QJsonObject caps = result.value("agentCapabilities").toObject();
            Q_UNUSED(caps);
        }
        emit initialized(result);
    });
}

void AcpClient::authenticate(const QString &methodId) {
    QJsonObject params{{"methodId", methodId}};
    sendRequest("authenticate", params, [this](const QJsonObject &result, const QJsonObject &error) {
        Q_UNUSED(result);
        if (!error.isEmpty()) {
            emit errorOccurred(error.value("message").toString(tr("Authentication failed")));
            return;
        }
        emit authenticated();
    });
}

void AcpClient::createSession(const QString &cwd) {
    QJsonObject params;
    params.insert("cwd", cwd);
    params.insert("mcpServers", QJsonArray());

    sendRequest("session/new", params, [this](const QJsonObject &result, const QJsonObject &error) {
        if (!error.isEmpty()) {
            if (isAuthError(error)) {
                emit authRequired(m_authMethods);
                return;
            }
            emit errorOccurred(error.value("message").toString(tr("Could not create session")));
            return;
        }
        m_sessionId = result.value("sessionId").toString();
        if (m_sessionId.isEmpty()) {
            emit errorOccurred(tr("ACP server did not return a session id"));
            return;
        }
        m_sessionMeta = result;
        emit sessionMeta(result);
        emit sessionReady(m_sessionId);
    });
}

void AcpClient::setSessionMode(const QString &modeId) {
    if (m_sessionId.isEmpty() || modeId.isEmpty()) {
        return;
    }
    sendRequest("session/set_mode",
                QJsonObject{{"sessionId", m_sessionId}, {"modeId", modeId}},
                [](const QJsonObject &, const QJsonObject &) {});
}

void AcpClient::sendPrompt(const QJsonArray &prompt) {
    if (m_sessionId.isEmpty()) {
        emit errorOccurred(tr("No active Antigravity session"));
        return;
    }
    if (m_prompting) {
        emit errorOccurred(tr("A prompt is already running"));
        return;
    }

    QJsonObject params;
    params.insert("sessionId", m_sessionId);
    params.insert("prompt", prompt);

    m_prompting = true;
    m_waitingForIdle = false;

    sendRequest("session/prompt", params, [this](const QJsonObject &result, const QJsonObject &error) {
        if (!error.isEmpty()) {
            m_prompting = false;
            m_waitingForIdle = false;
            if (isAuthError(error)) {
                emit authRequired(m_authMethods);
                return;
            }
            emit errorOccurred(error.value("message").toString(tr("Prompt failed")));
            emit promptFinished("error");
            return;
        }

        const QString stopReason = result.value("stopReason").toString();
        if (!stopReason.isEmpty()) {
            m_prompting = false;
            emit promptFinished(stopReason);
            return;
        }

        // v2: completion arrives later as a state_update
        m_waitingForIdle = true;
    });
}

void AcpClient::cancelPrompt() {
    if (m_sessionId.isEmpty()) {
        return;
    }
    sendNotification("session/cancel", QJsonObject{{"sessionId", m_sessionId}});
    m_prompting = false;
    m_waitingForIdle = false;
    emit promptFinished("cancelled");
}

void AcpClient::respondToPermission(int requestId, const QString &optionId) {
    const QJsonValue id = m_permissionIds.take(requestId);
    QJsonObject outcome{
        {"outcome", "selected"},
        {"optionId", optionId}
    };
    sendResponse(id.isUndefined() ? QJsonValue(requestId) : id, QJsonObject{{"outcome", outcome}});
}

void AcpClient::cancelPermission(int requestId) {
    const QJsonValue id = m_permissionIds.take(requestId);
    sendResponse(id.isUndefined() ? QJsonValue(requestId) : id, QJsonObject{
        {"outcome", QJsonObject{{"outcome", "cancelled"}}}
    });
}

QString AcpClient::extractText(const QJsonValue &content) {
    if (content.isString()) {
        return content.toString();
    }
    if (content.isObject()) {
        const QJsonObject obj = content.toObject();
        if (obj.contains("text")) {
            return obj.value("text").toString();
        }
        if (obj.contains("content")) {
            return extractText(obj.value("content"));
        }
        return QString();
    }
    if (content.isArray()) {
        QStringList parts;
        const QJsonArray arr = content.toArray();
        for (const QJsonValue &item : arr) {
            const QString piece = extractText(item);
            if (!piece.isEmpty()) {
                parts.append(piece);
            }
        }
        return parts.join(QString());
    }
    return QString();
}

void AcpClient::sendRequest(const QString &method, const QJsonObject &params,
                            std::function<void(const QJsonObject &, const QJsonObject &)> callback) {
    const int id = m_nextId++;
    m_pending.insert(id, std::move(callback));

    QJsonObject message{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", method},
        {"params", params}
    };
    writeMessage(message);
}

void AcpClient::sendNotification(const QString &method, const QJsonObject &params) {
    QJsonObject message{
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params}
    };
    writeMessage(message);
}

void AcpClient::sendResponse(const QJsonValue &id, const QJsonObject &result) {
    QJsonObject message{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
    writeMessage(message);
}

void AcpClient::sendError(const QJsonValue &id, int code, const QString &message) {
    QJsonObject error{
        {"code", code},
        {"message", message}
    };
    QJsonObject payload{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", error}
    };
    writeMessage(payload);
}

void AcpClient::writeMessage(const QJsonObject &message) {
    if (!m_process || m_process->state() != QProcess::Running) {
        emit errorOccurred(tr("Antigravity ACP server is not running"));
        return;
    }
    const QByteArray line = QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
    m_process->write(line);
}

void AcpClient::onReadyRead() {
    if (!m_process) {
        return;
    }
    m_stdoutBuffer.append(m_process->readAllStandardOutput());

    while (true) {
        const int newline = m_stdoutBuffer.indexOf('\n');
        if (newline < 0) {
            break;
        }
        QByteArray line = m_stdoutBuffer.left(newline);
        m_stdoutBuffer.remove(0, newline + 1);
        if (!line.isEmpty() && line.endsWith('\r')) {
            line.chop(1);
        }
        if (line.trimmed().isEmpty()) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            continue;
        }
        handleMessage(doc.object());
    }
}

void AcpClient::onReadyReadStderr() {
    if (!m_process) {
        return;
    }
    const QByteArray data = m_process->readAllStandardError();
    const QString text = QString::fromUtf8(data);
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        emit stderrMessage(line.trimmed());
    }
}

void AcpClient::handleMessage(const QJsonObject &message) {
    const bool hasMethod = message.contains("method");
    const bool hasId = message.contains("id");

    if (hasMethod && hasId) {
        handleAgentRequest(message);
        return;
    }

    if (hasMethod && !hasId) {
        const QString method = message.value("method").toString();
        if (method == "session/update") {
            handleSessionUpdate(message.value("params").toObject());
        }
        return;
    }

    if (hasId && !hasMethod) {
        const int id = message.value("id").toInt();
        auto callback = m_pending.take(id);
        if (!callback) {
            return;
        }
        if (message.contains("error")) {
            callback(QJsonObject(), message.value("error").toObject());
        } else {
            callback(message.value("result").toObject(), QJsonObject());
        }
    }
}

void AcpClient::handleAgentRequest(const QJsonObject &message) {
    const QString method = message.value("method").toString();
    const QJsonValue id = message.value("id");
    const QJsonObject params = message.value("params").toObject();

    if (method == "session/request_permission") {
        handleRequestPermission(id, params);
        return;
    }
    if (method == "fs/read_text_file") {
        handleReadTextFile(id, params);
        return;
    }
    if (method == "fs/write_text_file") {
        handleWriteTextFile(id, params);
        return;
    }
    if (method == "terminal/create") {
        handleTerminalCreate(id, params);
        return;
    }
    if (method == "terminal/output") {
        handleTerminalOutput(id, params);
        return;
    }
    if (method == "terminal/wait_for_exit") {
        handleTerminalWait(id, params);
        return;
    }
    if (method == "terminal/kill") {
        handleTerminalKill(id, params);
        return;
    }
    if (method == "terminal/release") {
        handleTerminalRelease(id, params);
        return;
    }

    sendError(id, -32601, QString("Method not found: %1").arg(method));
}

void AcpClient::handleSessionUpdate(const QJsonObject &params) {
    const QJsonObject update = params.value("update").toObject();
    const QString kind = update.value("sessionUpdate").toString();

    if (kind == "agent_message_chunk" || kind == "agent_message") {
        const QString text = extractText(update.value("content"));
        if (!text.isEmpty()) {
            emit agentText(text);
        }
        return;
    }
    if (kind == "agent_thought_chunk" || kind == "agent_thought") {
        const QString text = extractText(update.value("content"));
        if (!text.isEmpty()) {
            emit agentThought(text);
        }
        return;
    }
    if (kind == "tool_call" || kind == "tool_call_update") {
        emit toolCallUpdated(update);
        return;
    }
    if (kind == "plan") {
        emit planUpdated(update);
        return;
    }
    if (kind == "available_commands_update") {
        emit availableCommands(update.value("availableCommands").toArray());
        return;
    }
    if (kind == "current_mode_update") {
        return;
    }
    if (kind == "state_update") {
        const QString state = update.value("state").toString();
        if (state == "idle" && (m_prompting || m_waitingForIdle)) {
            m_prompting = false;
            m_waitingForIdle = false;
            emit promptFinished(update.value("stopReason").toString("end_turn"));
        }
    }
}

void AcpClient::handleReadTextFile(const QJsonValue &id, const QJsonObject &params) {
    const QString path = params.value("path").toString();
    const int line = params.value("line").toInt(0);
    const int limit = params.value("limit").toInt(0);

    QString content;
    if (m_readFile) {
        content = m_readFile(path, line, limit);
    } else {
        QFile file(path);
        if (!file.open(QFile::ReadOnly | QFile::Text)) {
            sendError(id, -32000, QString("Cannot read file: %1").arg(path));
            return;
        }
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        content = in.readAll();
        if (line > 0 || limit > 0) {
            const QStringList lines = content.split('\n');
            const int start = qMax(0, line - 1);
            const int count = limit > 0 ? limit : (lines.size() - start);
            content = QStringList(lines.mid(start, count)).join('\n');
        }
    }

    sendResponse(id, QJsonObject{{"content", content}});
}

void AcpClient::handleWriteTextFile(const QJsonValue &id, const QJsonObject &params) {
    const QString path = params.value("path").toString();
    const QString content = params.value("content").toString();

    bool ok = false;
    if (m_writeFile) {
        ok = m_writeFile(path, content);
    } else {
        QFile file(path);
        ok = file.open(QFile::WriteOnly | QFile::Text);
        if (ok) {
            QTextStream out(&file);
            out.setEncoding(QStringConverter::Utf8);
            out << content;
        }
    }

    if (!ok) {
        sendError(id, -32000, QString("Cannot write file: %1").arg(path));
        return;
    }

    emit fileWritten(path);
    sendResponse(id, QJsonObject());
}

void AcpClient::handleRequestPermission(const QJsonValue &id, const QJsonObject &params) {
    const int cookie = m_permCookie++;
    m_permissionIds.insert(cookie, id);
    emit permissionRequested(cookie, params);
}

void AcpClient::handleTerminalCreate(const QJsonValue &id, const QJsonObject &params) {
    QJsonObject env;
    if (params.value("env").isArray()) {
        env.insert("env", params.value("env"));
    } else if (params.value("env").isObject()) {
        env = params.value("env").toObject();
    }
    const QString termId = m_terminals->create(
        params.value("command").toString(),
        [&]() {
            QStringList args;
            for (const QJsonValue &v : params.value("args").toArray()) {
                args.append(v.toString());
            }
            return args;
        }(),
        env,
        params.value("cwd").toString(),
        static_cast<qint64>(params.value("outputByteLimit").toDouble(1024 * 1024)));
    sendResponse(id, QJsonObject{{"terminalId", termId}});
}

void AcpClient::handleTerminalOutput(const QJsonValue &id, const QJsonObject &params) {
    sendResponse(id, m_terminals->snapshot(params.value("terminalId").toString()));
}

void AcpClient::handleTerminalWait(const QJsonValue &id, const QJsonObject &params) {
    m_terminals->waitForExit(params.value("terminalId").toString(), [this, id](const QJsonObject &status) {
        sendResponse(id, status);
    });
}

void AcpClient::handleTerminalKill(const QJsonValue &id, const QJsonObject &params) {
    m_terminals->kill(params.value("terminalId").toString());
    sendResponse(id, QJsonObject());
}

void AcpClient::handleTerminalRelease(const QJsonValue &id, const QJsonObject &params) {
    m_terminals->release(params.value("terminalId").toString());
    sendResponse(id, QJsonObject());
}

bool AcpClient::isAuthError(const QJsonObject &error) const {
    const QString message = error.value("message").toString().toLower();
    const QJsonObject data = error.value("data").toObject();
    if (data.value("authRequired").toBool()) {
        return true;
    }
    const QString code = data.value("code").toString().toLower();
    if (code.contains("auth")) {
        return true;
    }
    return message.contains("auth") || message.contains("sign in") || message.contains("login");
}

} // namespace Orbit
