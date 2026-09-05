#pragma once

#include "agent/AgentTypes.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QVector>

class QTimer;

namespace Orbit {

class AgentThread : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        TypeRole = Qt::UserRole + 1,
        EntryRole
    };

    explicit AgentThread(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    const AgentEntry &at(int row) const;
    int count() const { return m_entries.size(); }
    void clear();

    void appendUser(const QString &text);
    void appendAgentChunk(const QString &text);
    void closeAgentMessage();
    void appendThought(const QString &text);
    void appendSystem(const QString &text);
    void upsertToolCall(const QJsonObject &update);
    void setPlan(const QJsonObject &plan);
    int addPermission(int cookie, const QJsonObject &params);
    void resolvePermission(int cookie, const QString &resolution);
    void appendTerminalOutput(const QString &terminalId, const QString &chunk);
    void toggleToolExpanded(int row);

    QVector<AgentDiff> pendingDiffs() const;
    void markDiffsReviewed();
    QVector<AgentDiff> takeRejectedRestores(const QStringList &paths);

    int findToolRow(const QString &toolCallId) const;

signals:
    void followFile(const QString &path);
    void reviewStateChanged();

private:
    static QVector<AgentDiff> parseDiffs(const QJsonArray &content);
    static QString parseBody(const QJsonArray &content);
    static QString parseTerminalId(const QJsonArray &content);
    static QStringList parseLocations(const QJsonArray &locations);
    void emitRow(int row);

    QVector<AgentEntry> m_entries;
    int m_openAgentRow = -1;
    int m_seq = 0;
};

} // namespace Orbit
