#pragma once

#include "agent/AgentTypes.h"

#include <QWidget>
#include <QVector>
#include <QJsonArray>
#include <functional>

class QPlainTextEdit;
class QListWidget;
class QFrame;
class QHBoxLayout;
class QComboBox;
class QPushButton;
class QLabel;

namespace Orbit {

class AgentComposer : public QWidget {
    Q_OBJECT

public:
    explicit AgentComposer(QWidget *parent = nullptr);

    void setWorkspace(const QString &root);
    void setCurrentFile(const QString &path);
    void setSelection(const QString &text);
    void setSelectionProvider(std::function<QString()> fn);
    void setCommands(const QVector<AgentCommand> &commands);
    void setModes(const QVector<AgentMode> &modes, const QString &currentId);
    void setPrompting(bool prompting);
    void focusInput();

    QString currentModeId() const;

signals:
    void sendRequested(const QJsonArray &prompt, const QString &displayText);
    void stopRequested();
    void modeChangeRequested(const QString &modeId);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void rebuildChips();
    void addFile(const QString &path);
    void addImage(const QString &path);
    void showMentionPopup(bool slash);
    void hidePopup();
    void applyMention(const QString &token);
    QJsonArray buildPrompt(const QString &text) const;
    void indexWorkspace();
    bool eventFilter(QObject *watched, QEvent *event) override;

    QPlainTextEdit *m_input = nullptr;
    QWidget *m_chipBar = nullptr;
    QHBoxLayout *m_chipLayout = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QPushButton *m_attachButton = nullptr;
    QPushButton *m_sendButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QFrame *m_popup = nullptr;
    QListWidget *m_popupList = nullptr;

    QString m_workspace;
    QString m_currentFile;
    QString m_selection;
    std::function<QString()> m_selectionProvider;
    QVector<AgentAttachment> m_attachments;
    QVector<AgentCommand> m_commands;
    QStringList m_workspaceFiles;
    bool m_prompting = false;
    bool m_slashPopup = false;
};

} // namespace Orbit
