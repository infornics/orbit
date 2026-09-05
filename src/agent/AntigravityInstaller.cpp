#include "agent/AntigravityInstaller.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSysInfo>
#include <QUrl>
#include <QSaveFile>
#include <QDirIterator>

namespace Orbit {
namespace {

const char kRegistryUrl[] = "https://cdn.agentclientprotocol.com/registry/v1/latest/registry.json";
const char kFallbackLinuxX64[] =
    "https://dl.google.com/agy-extensions/releases/linux/agy-acp-server-agy_acp_server_1.1.1-linux-x86_64.zip";
const char kFallbackLinuxArm[] =
    "https://dl.google.com/agy-extensions/releases/linux/agy-acp-server-agy_acp_server_1.1.1-linux-arm64.zip";
const char kFallbackMacArm[] =
    "https://dl.google.com/agy-extensions/releases/macos/agy-acp-server-agy_acp_server_1.1.1-darwin-arm64.zip";
const char kFallbackMacX64[] =
    "https://dl.google.com/agy-extensions/releases/macos/agy-acp-server-agy_acp_server_1.1.1-darwin-x64.zip";
const char kFallbackWinX64[] =
    "https://dl.google.com/agy-extensions/releases/windows/agy-acp-server-agy_acp_server_1.1.1-windows-x86_64.zip";
const char kFallbackWinArm[] =
    "https://dl.google.com/agy-extensions/releases/windows/agy-acp-server-agy_acp_server_1.1.1-windows-arm64.zip";

QString currentPlatformId() {
#if defined(Q_OS_LINUX)
    if (QSysInfo::currentCpuArchitecture().contains("arm", Qt::CaseInsensitive)
        || QSysInfo::currentCpuArchitecture().contains("aarch64", Qt::CaseInsensitive)) {
        return QStringLiteral("linux-aarch64");
    }
    return QStringLiteral("linux-x86_64");
#elif defined(Q_OS_MACOS)
    if (QSysInfo::currentCpuArchitecture().contains("arm", Qt::CaseInsensitive)
        || QSysInfo::currentCpuArchitecture().contains("aarch64", Qt::CaseInsensitive)) {
        return QStringLiteral("darwin-aarch64");
    }
    return QStringLiteral("darwin-x86_64");
#elif defined(Q_OS_WIN)
    if (QSysInfo::currentCpuArchitecture().contains("arm", Qt::CaseInsensitive)) {
        return QStringLiteral("windows-aarch64");
    }
    return QStringLiteral("windows-x86_64");
#else
    return QStringLiteral("linux-x86_64");
#endif
}

void ensureExecutable(const QString &path) {
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return;
    }
    QFile::setPermissions(path, QFile::permissions(path)
                                      | QFile::ExeOwner | QFile::ExeUser
                                      | QFile::ExeGroup | QFile::ExeOther);
}

QString lookForBinary(const QString &directory) {
    if (directory.isEmpty() || !QDir(directory).exists()) {
        return {};
    }

    const QStringList names = {
        QStringLiteral("agy_acp_server.par"),
        QStringLiteral("agy_acp_server"),
        QStringLiteral("agy_acp_server.exe")
    };

    QDirIterator named(directory, names, QDir::Files, QDirIterator::Subdirectories);
    while (named.hasNext()) {
        named.next();
        return named.filePath();
    }

    QDirIterator any(directory, QDir::Files, QDirIterator::Subdirectories);
    while (any.hasNext()) {
        any.next();
        if (any.fileName().startsWith(QLatin1String("agy_acp_server"))) {
            return any.filePath();
        }
    }
    return {};
}

} // namespace

AntigravityInstaller::AntigravityInstaller(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this)) {
}

QString AntigravityInstaller::installDirectory() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QStringLiteral("antigravity"));
}

AntigravityPlatformSpec AntigravityInstaller::fallbackSpec() {
    AntigravityPlatformSpec spec;
    spec.platformId = currentPlatformId();

    if (spec.platformId == "linux-x86_64") {
        spec.archiveUrl = QString::fromUtf8(kFallbackLinuxX64);
        spec.command = QStringLiteral("./agy_acp_server.par");
        spec.args = QStringList{QStringLiteral("--uid=")};
    } else if (spec.platformId == "linux-aarch64") {
        spec.archiveUrl = QString::fromUtf8(kFallbackLinuxArm);
        spec.command = QStringLiteral("./agy_acp_server.par");
        spec.args = QStringList{QStringLiteral("--uid=")};
    } else if (spec.platformId == "darwin-aarch64") {
        spec.archiveUrl = QString::fromUtf8(kFallbackMacArm);
        spec.command = QStringLiteral("./agy_acp_server.par");
    } else if (spec.platformId == "darwin-x86_64") {
        spec.archiveUrl = QString::fromUtf8(kFallbackMacX64);
        spec.command = QStringLiteral("./agy_acp_server.par");
    } else if (spec.platformId == "windows-aarch64") {
        spec.archiveUrl = QString::fromUtf8(kFallbackWinArm);
        spec.command = QStringLiteral("./agy_acp_server.exe");
    } else {
        spec.archiveUrl = QString::fromUtf8(kFallbackWinX64);
        spec.command = QStringLiteral("./agy_acp_server.exe");
    }
    return spec;
}

AntigravityPlatformSpec AntigravityInstaller::specFromRegistry(const QByteArray &registryJson) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(registryJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return fallbackSpec();
    }

    const QJsonArray agents = doc.object().value("agents").toArray();
    QJsonObject agent;
    for (const QJsonValue &value : agents) {
        const QJsonObject obj = value.toObject();
        if (obj.value("id").toString() == "antigravity-acp") {
            agent = obj;
            break;
        }
    }
    if (agent.isEmpty()) {
        return fallbackSpec();
    }

    const QString platform = currentPlatformId();
    const QJsonObject binary = agent.value("distribution").toObject().value("binary").toObject();
    const QJsonObject entry = binary.value(platform).toObject();
    if (entry.isEmpty()) {
        return fallbackSpec();
    }

    AntigravityPlatformSpec spec;
    spec.platformId = platform;
    spec.archiveUrl = entry.value("archive").toString();
    spec.command = entry.value("cmd").toString(QStringLiteral("./agy_acp_server.par"));
    const QJsonArray args = entry.value("args").toArray();
    for (const QJsonValue &arg : args) {
        spec.args.append(arg.toString());
    }
    if (spec.archiveUrl.isEmpty()) {
        return fallbackSpec();
    }
    return spec;
}

QString AntigravityInstaller::findExistingBinary() {
    const QString installed = lookForBinary(installDirectory());
    if (!installed.isEmpty()) {
        ensureExecutable(installed);
        ensureExecutable(QFileInfo(installed).dir().filePath(QStringLiteral("localharness_external")));
        return installed;
    }

    const QStringList names = {
        QStringLiteral("agy_acp_server.par"),
        QStringLiteral("agy_acp_server"),
        QStringLiteral("agy_acp_server.exe")
    };
    for (const QString &name : names) {
        const QString found = QStandardPaths::findExecutable(name);
        if (!found.isEmpty()) {
            return found;
        }
    }
    return QString();
}

QStringList AntigravityInstaller::launchArguments(const QString &binaryPath) {
    QStringList args;
#if defined(Q_OS_LINUX)
    if (binaryPath.endsWith(".par")) {
        args << QStringLiteral("--uid=");
    }
#else
    Q_UNUSED(binaryPath);
#endif
    return args;
}

bool AntigravityInstaller::isInstalled() const {
    return !binaryPath().isEmpty();
}

QString AntigravityInstaller::binaryPath() const {
    return findExistingBinary();
}

QStringList AntigravityInstaller::launchArgs() const {
    return launchArguments(binaryPath());
}

void AntigravityInstaller::install() {
    if (m_busy) {
        return;
    }
    m_busy = true;
    m_cancelled = false;

    const QString existing = lookForBinary(installDirectory());
    if (!existing.isEmpty() && QFileInfo(existing).size() > 1024 * 1024) {
        emit statusChanged(tr("Finishing existing Antigravity install…"));
        ensureExecutable(existing);
        const QDir dir(QFileInfo(existing).absolutePath());
        ensureExecutable(dir.filePath(QStringLiteral("localharness_external")));
        finishOk();
        return;
    }

    m_spec = fallbackSpec();
    emit statusChanged(tr("Looking up the latest Antigravity ACP server…"));
    fetchRegistry();
}

void AntigravityInstaller::cancel() {
    m_cancelled = true;
    cleanupReply();
    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile->remove();
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }
    if (m_busy) {
        fail(tr("Installation cancelled"));
    }
}

void AntigravityInstaller::fetchRegistry() {
    QNetworkRequest request{QUrl(QString::fromUtf8(kRegistryUrl))};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Orbit/0.1.0"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        if (m_cancelled) {
            cleanupReply();
            return;
        }

        QByteArray body;
        bool ok = false;
        if (m_reply && m_reply->error() == QNetworkReply::NoError) {
            body = m_reply->readAll();
            ok = true;
        }
        cleanupReply();

        if (ok) {
            m_spec = specFromRegistry(body);
        } else {
            m_spec = fallbackSpec();
        }
        emit statusChanged(tr("Downloading Google Antigravity…"));
        downloadArchive(m_spec);
    });
}

void AntigravityInstaller::downloadArchive(const AntigravityPlatformSpec &spec) {
    QNetworkRequest request{QUrl(spec.archiveUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Orbit/0.1.0"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(0);

    m_downloadPath = QDir::temp().filePath(QStringLiteral("orbit-antigravity-acp.zip"));
    if (m_downloadFile) {
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }
    m_downloadFile = new QFile(m_downloadPath, this);
    if (!m_downloadFile->open(QFile::WriteOnly | QFile::Truncate)) {
        fail(tr("Could not save the downloaded archive"));
        return;
    }

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &AntigravityInstaller::progress);
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_downloadFile && m_reply) {
            m_downloadFile->write(m_reply->readAll());
        }
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, spec]() {
        if (m_cancelled) {
            cleanupReply();
            return;
        }
        if (m_downloadFile && m_reply) {
            m_downloadFile->write(m_reply->readAll());
            m_downloadFile->close();
        }

        if (!m_reply || m_reply->error() != QNetworkReply::NoError) {
            const QString err = m_reply ? m_reply->errorString() : tr("Network error");
            cleanupReply();
            if (m_downloadFile) {
                m_downloadFile->remove();
                m_downloadFile->deleteLater();
                m_downloadFile = nullptr;
            }
            fail(tr("Download failed: %1").arg(err));
            return;
        }

        cleanupReply();
        if (m_downloadFile) {
            m_downloadFile->deleteLater();
            m_downloadFile = nullptr;
        }

        emit statusChanged(tr("Unpacking Antigravity… this can take a few minutes"));
        extractArchive(m_downloadPath, spec);
    });
}

void AntigravityInstaller::extractArchive(const QString &zipPath, const AntigravityPlatformSpec &spec) {
    QDir().mkpath(installDirectory());

    QProcess unzip;
    unzip.setProcessChannelMode(QProcess::MergedChannels);
    unzip.start(QStringLiteral("python3"), QStringList{
        QStringLiteral("-c"),
        QStringLiteral("import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])"),
        zipPath,
        installDirectory()
    });

    // The official linux zip is ~650MB compressed / ~2GB uncompressed.
    bool extracted = unzip.waitForFinished(10 * 60 * 1000) && unzip.exitCode() == 0;
    if (!extracted) {
        unzip.start(QStringLiteral("unzip"), QStringList{
            QStringLiteral("-o"), zipPath, QStringLiteral("-d"), installDirectory()
        });
        extracted = unzip.waitForFinished(10 * 60 * 1000) && unzip.exitCode() == 0;
    }

    QFile::remove(zipPath);

    if (!extracted) {
        fail(tr("Could not unpack the Antigravity archive"));
        return;
    }

    const QString binary = lookForBinary(installDirectory());
    if (binary.isEmpty()) {
        fail(tr("The archive did not contain agy_acp_server"));
        return;
    }

    ensureExecutable(binary);
    const QDir dir(QFileInfo(binary).absolutePath());
    ensureExecutable(dir.filePath(QStringLiteral("localharness_external")));
    ensureExecutable(dir.filePath(QStringLiteral("agy_acp_server.par")));
    ensureExecutable(dir.filePath(QStringLiteral("agy_acp_server")));

    Q_UNUSED(spec);
    finishOk();
}

void AntigravityInstaller::finishOk() {
    m_busy = false;
    emit statusChanged(tr("Antigravity is installed"));
    emit finished(true, QString());
}

void AntigravityInstaller::fail(const QString &error) {
    m_busy = false;
    emit finished(false, error);
}

void AntigravityInstaller::cleanupReply() {
    if (!m_reply) {
        return;
    }
    m_reply->disconnect(this);
    m_reply->deleteLater();
    m_reply = nullptr;
}

} // namespace Orbit
