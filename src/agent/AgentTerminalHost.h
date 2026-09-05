#pragma once

#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <functional>

class QProcess;

namespace Orbit {

class AgentTerminalHost : public QObject {
    Q_OBJECT

public:
    explicit AgentTerminalHost(QObject *parent = nullptr);
    ~AgentTerminalHost() override;

    QString create(const QString &command,
                   const QStringList &args,
                   const QJsonObject &env,
                   const QString &cwd,
                   qint64 outputByteLimit);

    QJsonObject snapshot(const QString &id) const;
    void kill(const QString &id);
    void release(const QString &id);
    void waitForExit(const QString &id, std::function<void(QJsonObject)> callback);

    void reset();

signals:
    void outputAppended(const QString &id, const QString &chunk);
    void exited(const QString &id, int exitCode);

private:
    struct Term {
        QString id;
        QProcess *process = nullptr;
        QByteArray output;
        qint64 byteLimit = 1024 * 1024;
        bool truncated = false;
        bool finished = false;
        int exitCode = -1;
        QString signal;
        QList<std::function<void(QJsonObject)>> waiters;
    };

    void appendOutput(Term *term, const QByteArray &chunk);
    QJsonObject exitStatus(const Term *term) const;
    void finish(Term *term, int code, const QString &signal);
    Term *find(const QString &id) const;

    QHash<QString, Term *> m_terms;
};

} // namespace Orbit
