#include "agent/AgentThread.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

namespace Orbit {

AgentThread::AgentThread(QObject *parent)
    : QAbstractListModel(parent) {
}

int AgentThread::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant AgentThread::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    const AgentEntry &entry = m_entries.at(index.row());
    if (role == TypeRole) {
        return static_cast<int>(entry.type);
    }
    if (role == Qt::DisplayRole) {
        return entry.text;
    }
    return {};
}

QHash<int, QByteArray> AgentThread::roleNames() const {
    return {
        {Qt::DisplayRole, "display"},
        {TypeRole, "type"}
    };
}

const AgentEntry &AgentThread::at(int row) const {
    return m_entries.at(row);
}

void AgentThread::clear() {
    beginResetModel();
    m_entries.clear();
    m_openAgentRow = -1;
    m_seq = 0;
    endResetModel();
    emit reviewStateChanged();
}

void AgentThread::appendUser(const QString &text) {
    closeAgentMessage();
    AgentEntry entry;
    entry.id = QStringLiteral("user-%1").arg(++m_seq);
    entry.type = AgentEntryType::User;
    entry.text = text;
    const int row = m_entries.size();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.append(entry);
    endInsertRows();
}

void AgentThread::appendAgentChunk(const QString &text) {
    if (m_openAgentRow >= 0 && m_openAgentRow < m_entries.size()) {
        m_entries[m_openAgentRow].text += text;
        emitRow(m_openAgentRow);
        return;
    }
    AgentEntry entry;
    entry.id = QStringLiteral("agent-%1").arg(++m_seq);
    entry.type = AgentEntryType::Agent;
    entry.text = text;
    const int row = m_entries.size();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.append(entry);
    m_openAgentRow = row;
    endInsertRows();
}

void AgentThread::closeAgentMessage() {
    m_openAgentRow = -1;
}

void AgentThread::appendThought(const QString &text) {
    AgentEntry entry;
    entry.id = QStringLiteral("thought-%1").arg(++m_seq);
    entry.type = AgentEntryType::Thought;
    entry.text = text;
    const int row = m_entries.size();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.append(entry);
    endInsertRows();
}

void AgentThread::appendSystem(const QString &text) {
    AgentEntry entry;
    entry.id = QStringLiteral("sys-%1").arg(++m_seq);
    entry.type = AgentEntryType::System;
    entry.text = text;
    const int row = m_entries.size();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.append(entry);
    endInsertRows();
}

void AgentThread::upsertToolCall(const QJsonObject &update) {
    const QString id = update.value("toolCallId").toString();
    if (id.isEmpty()) {
        return;
    }

    int row = findToolRow(id);
    const bool isNew = row < 0;
    if (isNew) {
        AgentEntry entry;
        entry.id = QStringLiteral("tool-%1").arg(id);
        entry.type = AgentEntryType::ToolCall;
        entry.tool.id = id;
        row = m_entries.size();
        beginInsertRows(QModelIndex(), row, row);
        m_entries.append(entry);
        endInsertRows();
    }

    AgentToolCall &tool = m_entries[row].tool;
    if (update.contains(QStringLiteral("title"))) {
        tool.title = update.value("title").toString();
    }
    if (update.contains(QStringLiteral("kind"))) {
        tool.kind = update.value("kind").toString();
    }
    if (update.contains(QStringLiteral("status"))) {
        tool.status = update.value("status").toString();
    }
    if (update.contains(QStringLiteral("locations"))) {
        tool.locations = parseLocations(update.value("locations").toArray());
    }
    if (update.contains(QStringLiteral("content"))) {
        const QJsonArray content = update.value("content").toArray();
        tool.body = parseBody(content);
        tool.diffs = parseDiffs(content);
        tool.terminalId = parseTerminalId(content);
    }

    if (tool.title.isEmpty()) {
        tool.title = tool.kind.isEmpty() ? tr("Tool") : tool.kind;
    }

    emitRow(row);
    emit reviewStateChanged();

    if (!tool.locations.isEmpty()
        && (tool.kind == QLatin1String("edit") || tool.kind == QLatin1String("write"))) {
        emit followFile(tool.locations.first());
    }
}

void AgentThread::setPlan(const QJsonObject &plan) {
    QVector<AgentPlanStep> steps;
    const QJsonArray entries = plan.value("entries").toArray();
    for (const QJsonValue &value : entries) {
        const QJsonObject obj = value.toObject();
        AgentPlanStep step;
        step.content = obj.value("content").toString();
        step.status = obj.value("status").toString();
        if (!step.content.isEmpty()) {
            steps.append(step);
        }
    }

    int row = -1;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].type == AgentEntryType::Plan) {
            row = i;
            break;
        }
    }

    if (steps.isEmpty()) {
        return;
    }

    if (row < 0) {
        AgentEntry entry;
        entry.id = QStringLiteral("plan");
        entry.type = AgentEntryType::Plan;
        entry.plan = steps;
        row = m_entries.size();
        beginInsertRows(QModelIndex(), row, row);
        m_entries.append(entry);
        endInsertRows();
        return;
    }

    m_entries[row].plan = steps;
    emitRow(row);
}

int AgentThread::addPermission(int cookie, const QJsonObject &params) {
    const QJsonObject toolCall = params.value("toolCall").toObject();
    AgentEntry entry;
    entry.id = QStringLiteral("perm-%1").arg(cookie);
    entry.type = AgentEntryType::Permission;
    entry.permission.cookie = cookie;
    entry.permission.title = toolCall.value("title").toString(tr("Permission required"));
    entry.permission.kind = toolCall.value("kind").toString();
    entry.permission.options = params.value("options").toArray();
    entry.permission.diffs = parseDiffs(toolCall.value("content").toArray());
    const int row = m_entries.size();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.append(entry);
    endInsertRows();
    return row;
}

void AgentThread::resolvePermission(int cookie, const QString &resolution) {
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].type == AgentEntryType::Permission
            && m_entries[i].permission.cookie == cookie) {
            m_entries[i].permission.resolved = true;
            m_entries[i].permission.resolution = resolution;
            emitRow(i);
            return;
        }
    }
}

void AgentThread::appendTerminalOutput(const QString &terminalId, const QString &chunk) {
    if (terminalId.isEmpty() || chunk.isEmpty()) {
        return;
    }
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].type == AgentEntryType::ToolCall
            && m_entries[i].tool.terminalId == terminalId) {
            auto &body = m_entries[i].tool.body;
            body += chunk;
            if (body.size() > 80 * 1024) {
                body = body.right(64 * 1024);
            }
            m_entries[i].tool.expanded = true;
            emitRow(i);
            return;
        }
    }
}

void AgentThread::toggleToolExpanded(int row) {
    if (row < 0 || row >= m_entries.size()) {
        return;
    }
    if (m_entries[row].type != AgentEntryType::ToolCall) {
        return;
    }
    m_entries[row].tool.expanded = !m_entries[row].tool.expanded;
    emitRow(row);
}

QVector<AgentDiff> AgentThread::pendingDiffs() const {
    QVector<AgentDiff> diffs;
    QSet<QString> seen;
    for (const AgentEntry &entry : m_entries) {
        const QVector<AgentDiff> *src = nullptr;
        if (entry.type == AgentEntryType::ToolCall) {
            src = &entry.tool.diffs;
        } else if (entry.type == AgentEntryType::Permission && !entry.permission.resolved) {
            src = &entry.permission.diffs;
        }
        if (!src) {
            continue;
        }
        for (const AgentDiff &diff : *src) {
            if (diff.reviewed || diff.rejected || diff.path.isEmpty() || seen.contains(diff.path)) {
                continue;
            }
            seen.insert(diff.path);
            diffs.append(diff);
        }
    }
    return diffs;
}

void AgentThread::markDiffsReviewed() {
    for (AgentEntry &entry : m_entries) {
        for (AgentDiff &diff : entry.tool.diffs) {
            diff.reviewed = true;
        }
    }
    emit reviewStateChanged();
    if (!m_entries.isEmpty()) {
        emit dataChanged(index(0), index(m_entries.size() - 1));
    }
}

QVector<AgentDiff> AgentThread::takeRejectedRestores(const QStringList &paths) {
    QVector<AgentDiff> restores;
    const QSet<QString> wanted(paths.begin(), paths.end());
    for (AgentEntry &entry : m_entries) {
        for (AgentDiff &diff : entry.tool.diffs) {
            if (wanted.contains(diff.path) && !diff.rejected) {
                diff.rejected = true;
                diff.reviewed = true;
                restores.append(diff);
            }
        }
    }
    emit reviewStateChanged();
    if (!m_entries.isEmpty()) {
        emit dataChanged(index(0), index(m_entries.size() - 1));
    }
    return restores;
}

int AgentThread::findToolRow(const QString &toolCallId) const {
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].type == AgentEntryType::ToolCall
            && m_entries[i].tool.id == toolCallId) {
            return i;
        }
    }
    return -1;
}

QVector<AgentDiff> AgentThread::parseDiffs(const QJsonArray &content) {
    QVector<AgentDiff> diffs;
    for (const QJsonValue &value : content) {
        const QJsonObject obj = value.toObject();
        if (obj.value("type").toString() != QLatin1String("diff")) {
            continue;
        }
        AgentDiff diff;
        diff.path = obj.value("path").toString();
        diff.oldText = obj.value("oldText").toString();
        diff.newText = obj.value("newText").toString();
        if (!diff.path.isEmpty()) {
            diffs.append(diff);
        }
    }
    return diffs;
}

QString AgentThread::parseBody(const QJsonArray &content) {
    QStringList parts;
    for (const QJsonValue &value : content) {
        const QJsonObject obj = value.toObject();
        const QString type = obj.value("type").toString();
        if (type == QLatin1String("content") || type == QLatin1String("text")) {
            const QJsonValue inner = obj.contains("content") ? obj.value("content") : obj.value("text");
            if (inner.isString()) {
                parts.append(inner.toString());
            } else if (inner.isObject()) {
                parts.append(inner.toObject().value("text").toString());
            }
        }
    }
    return parts.join('\n');
}

QString AgentThread::parseTerminalId(const QJsonArray &content) {
    for (const QJsonValue &value : content) {
        const QJsonObject obj = value.toObject();
        if (obj.value("type").toString() == QLatin1String("terminal")) {
            return obj.value("terminalId").toString();
        }
    }
    return {};
}

QStringList AgentThread::parseLocations(const QJsonArray &locations) {
    QStringList paths;
    for (const QJsonValue &value : locations) {
        const QJsonObject obj = value.toObject();
        const QString path = obj.value("path").toString();
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    return paths;
}

void AgentThread::emitRow(int row) {
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

} // namespace Orbit
