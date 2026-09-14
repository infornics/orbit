#include "terminal/PtyProcess.h"

#include <pty.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>

#include <QSocketNotifier>
#include <QDebug>
#include <QDir>

namespace Orbit {

PtyProcess::PtyProcess(QObject *parent)
    : QObject(parent) {
}

PtyProcess::~PtyProcess() {
    stop();
}

bool PtyProcess::start(const QString &cwd, int rows, int cols) {
    if (isRunning()) {
        stop();
    }

    struct winsize ws{};
    ws.ws_row = static_cast<unsigned short>(rows > 0 ? rows : 24);
    ws.ws_col = static_cast<unsigned short>(cols > 0 ? cols : 80);

    m_pid = forkpty(&m_masterFd, nullptr, nullptr, &ws);
    if (m_pid < 0) {
        qWarning() << "PtyProcess: forkpty failed, errno:" << errno;
        return false;
    }

    if (m_pid == 0) {
        // Child process
        if (!cwd.isEmpty() && QDir(cwd).exists()) {
            if (chdir(cwd.toUtf8().constData()) != 0) {
                // ignore failure, stay in current directory
            }
        }
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);

        const char *shell = getenv("SHELL");
        if (!shell || *shell == '\0') {
            shell = "/bin/bash";
        }

        execlp(shell, shell, nullptr);
        _exit(127);
    }

    // Parent process
    m_notifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &PtyProcess::onSocketRead);

    return true;
}

void PtyProcess::stop() {
    if (m_notifier) {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }

    if (m_masterFd != -1) {
        close(m_masterFd);
        m_masterFd = -1;
    }

    if (m_pid > 0) {
        kill(m_pid, SIGTERM);
        int status = 0;
        waitpid(m_pid, &status, WNOHANG);
        m_pid = -1;
    }
}

bool PtyProcess::isRunning() const {
    return m_masterFd != -1 && m_pid > 0;
}

void PtyProcess::writeInput(const QByteArray &data) {
    if (m_masterFd != -1 && !data.isEmpty()) {
        ssize_t ret = ::write(m_masterFd, data.constData(), data.size());
        Q_UNUSED(ret);
    }
}

void PtyProcess::resizePty(int rows, int cols) {
    if (m_masterFd != -1 && rows > 0 && cols > 0) {
        struct winsize ws{};
        ws.ws_row = static_cast<unsigned short>(rows);
        ws.ws_col = static_cast<unsigned short>(cols);
        ioctl(m_masterFd, TIOCSWINSZ, &ws);
    }
}

void PtyProcess::onSocketRead() {
    if (m_masterFd == -1) return;

    char buffer[4096];
    ssize_t bytesRead = ::read(m_masterFd, buffer, sizeof(buffer));
    if (bytesRead > 0) {
        emit readyRead(QByteArray(buffer, static_cast<int>(bytesRead)));
    } else if (bytesRead == 0 || (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        // Process closed or exited
        int exitCode = 0;
        if (m_pid > 0) {
            int status = 0;
            if (waitpid(m_pid, &status, WNOHANG) > 0 && WIFEXITED(status)) {
                exitCode = WEXITSTATUS(status);
            }
        }
        stop();
        emit finished(exitCode);
    }
}

} // namespace Orbit
