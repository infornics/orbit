#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <sys/types.h>

class QSocketNotifier;

namespace Orbit {

class PtyProcess : public QObject {
    Q_OBJECT

public:
    explicit PtyProcess(QObject *parent = nullptr);
    ~PtyProcess() override;

    bool start(const QString &cwd = QString(), int rows = 24, int cols = 80);
    void stop();
    bool isRunning() const;

    void writeInput(const QByteArray &data);
    void resizePty(int rows, int cols);

    int masterFd() const { return m_masterFd; }
    pid_t processId() const { return m_pid; }

signals:
    void readyRead(const QByteArray &data);
    void finished(int exitCode);

private slots:
    void onSocketRead();

private:
    int m_masterFd = -1;
    pid_t m_pid = -1;
    QSocketNotifier *m_notifier = nullptr;
};

} // namespace Orbit
