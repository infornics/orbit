#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Orbit {

enum class AgentEntryType {
    User,
    Agent,
    Thought,
    ToolCall,
    Plan,
    Permission,
    System
};

struct AgentDiff {
    QString path;
    QString oldText;
    QString newText;
    bool reviewed = false;
    bool rejected = false;
};

struct AgentPlanStep {
    QString content;
    QString status; // pending, in_progress, completed
};

struct AgentToolCall {
    QString id;
    QString title;
    QString kind;
    QString status;
    QString body;
    QString terminalId;
    QStringList locations;
    QVector<AgentDiff> diffs;
    bool expanded = false;
};

struct AgentPermission {
    int cookie = -1;
    QString title;
    QString kind;
    QJsonArray options;
    QVector<AgentDiff> diffs;
    bool resolved = false;
    QString resolution;
};

struct AgentEntry {
    QString id;
    AgentEntryType type = AgentEntryType::System;
    QString text;
    AgentToolCall tool;
    QVector<AgentPlanStep> plan;
    AgentPermission permission;
};

struct AgentAttachment {
    enum class Kind { File, Image, Selection };
    Kind kind = Kind::File;
    QString path;
    QString name;
    QString mime;
    QByteArray imageData;
    QString text;
};

struct AgentCommand {
    QString name;
    QString description;
    QString hint;
};

struct AgentMode {
    QString id;
    QString name;
    QString description;
};

} // namespace Orbit
