#include "agent/AgentTerminalHost.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QUuid>
#include <QJsonArray>

namespace Orbit {

AgentTerminalHost::AgentTerminalHost(QObject *parent)
    : QObject(parent) {
}

AgentTerminalHost::~AgentTerminalHost() {
    reset();
}

QString AgentTerminalHost::create(const QString &command,
                                  const QStringList &args,
                                  const QJsonObject &env,
                                  const QString &cwd,
                                  qint64 outputByteLimit) {
    auto *term = new Term;
    term->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    term->byteLimit = outputByteLimit > 0 ? outputByteLimit : 1024 * 1024;
    term->process = new QProcess(this);
    term->process->setProcessChannelMode(QProcess::MergedChannels);
    if (!cwd.isEmpty()) {
        term->process->setWorkingDirectory(cwd);
    }

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QJsonArray envList = env.value("env").toArray();
    if (!envList.isEmpty()) {
        for (const QJsonValue &value : envList) {
            const QJsonObject item = value.toObject();
            environment.insert(item.value("name").toString(), item.value("value").toString());
        }
    } else {
        for (auto it = env.begin(); it != env.end(); ++it) {
            if (it.value().isString()) {
                environment.insert(it.key(), it.value().toString());
            }
        }
    }
    term->process->setProcessEnvironment(environment);

    connect(term->process, &QProcess::readyRead, this, [this, term]() {
        if (!term->process) {
            return;
        }
        appendOutput(term, term->process->readAll());
    });
    connect(term->process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, term](int code, QProcess::ExitStatus status) {
                finish(term, code, status == QProcess::CrashExit ? QStringLiteral("SIGTERM") : QString());
            });

    m_terms.insert(term->id, term);
    term->process->start(command, args);
    return term->id;
}

QJsonObject AgentTerminalHost::snapshot(const QString &id) const {
    const Term *term = find(id);
    if (!term) {
        return QJsonObject{
            {"output", QString()},
            {"truncated", false}
        };
    }
    QJsonObject result{
        {"output", QString::fromUtf8(term->output)},
        {"truncated", term->truncated}
    };
    if (term->finished) {
        result.insert("exitStatus", exitStatus(term));
    }
    return result;
}

void AgentTerminalHost::kill(const QString &id) {
    Term *term = find(id);
    if (!term || !term->process) {
        return;
    }
    term->process->terminate();
    if (!term->process->waitForFinished(400)) {
        term->process->kill();
    }
}

void AgentTerminalHost::release(const QString &id) {
    Term *term = m_terms.take(id);
    if (!term) {
        return;
    }
    if (term->process) {
        term->process->disconnect(this);
        if (term->process->state() != QProcess::NotRunning) {
            term->process->kill();
            term->process->waitForFinished(200);
        }
        term->process->deleteLater();
    }
    for (auto &waiter : term->waiters) {
        waiter(exitStatus(term));
    }
    delete term;
}

void AgentTerminalHost::waitForExit(const QString &id, std::function<void(QJsonObject)> callback) {
    Term *term = find(id);
    if (!term) {
        callback(QJsonObject{{"exitCode", -1}});
        return;
    }
    if (term->finished) {
        callback(exitStatus(term));
        return;
    }
    term->waiters.append(std::move(callback));
}

void AgentTerminalHost::reset() {
    const QStringList ids = m_terms.keys();
    for (const QString &id : ids) {
        release(id);
    }
}

void AgentTerminalHost::appendOutput(Term *term, const QByteArray &chunk) {
    if (chunk.isEmpty()) {
        return;
    }
    if (term->output.size() >= term->byteLimit) {
        term->truncated = true;
        return;
    }
    QByteArray piece = chunk;
    if (term->output.size() + piece.size() > term->byteLimit) {
        piece = piece.left(term->byteLimit - term->output.size());
        term->truncated = true;
    }
    term->output += piece;
    emit outputAppended(term->id, QString::fromUtf8(piece));
}

QJsonObject AgentTerminalHost::exitStatus(const Term *term) const {
    QJsonObject status;
    status.insert("exitCode", term->exitCode);
    if (!term->signal.isEmpty()) {
        status.insert("signal", term->signal);
    } else {
        status.insert("signal", QJsonValue::Null);
    }
    return status;
}

void AgentTerminalHost::finish(Term *term, int code, const QString &signal) {
    if (term->finished) {
        return;
    }
    term->finished = true;
    term->exitCode = code;
    term->signal = signal;
    const QJsonObject status = exitStatus(term);
    const auto waiters = term->waiters;
    term->waiters.clear();
    for (const auto &waiter : waiters) {
        waiter(status);
    }
    emit exited(term->id, code);
}

AgentTerminalHost::Term *AgentTerminalHost::find(const QString &id) const {
    return m_terms.value(id, nullptr);
}

} // namespace Orbit
