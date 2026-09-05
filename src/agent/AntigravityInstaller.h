#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

namespace Orbit {

struct AntigravityPlatformSpec {
    QString platformId;
    QString archiveUrl;
    QString command;
    QStringList args;
};

class AntigravityInstaller : public QObject {
    Q_OBJECT

public:
    explicit AntigravityInstaller(QObject *parent = nullptr);

    static QString installDirectory();
    static AntigravityPlatformSpec fallbackSpec();
    static AntigravityPlatformSpec specFromRegistry(const QByteArray &registryJson);
    static QString findExistingBinary();
    static QStringList launchArguments(const QString &binaryPath);

    bool isInstalled() const;
    QString binaryPath() const;
    QStringList launchArgs() const;
    bool isBusy() const { return m_busy; }

    void install();
    void cancel();

signals:
    void progress(qint64 received, qint64 total);
    void statusChanged(const QString &message);
    void finished(bool ok, const QString &error);

private:
    void fetchRegistry();
    void downloadArchive(const AntigravityPlatformSpec &spec);
    void extractArchive(const QString &zipPath, const AntigravityPlatformSpec &spec);
    void finishOk();
    void fail(const QString &error);
    void cleanupReply();

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QFile *m_downloadFile = nullptr;
    QString m_downloadPath;
    AntigravityPlatformSpec m_spec;
    bool m_busy = false;
    bool m_cancelled = false;
};

} // namespace Orbit
