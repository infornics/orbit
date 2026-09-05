#pragma once

#include "agent/AgentTypes.h"

#include <QListView>
#include <QStyledItemDelegate>
#include <QHash>
#include <QTextDocument>

namespace Orbit {

class AgentThread;

class AgentEntryDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit AgentEntryDelegate(AgentThread *thread, QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;
    void invalidateCache();

signals:
    void openFileRequested(const QString &path);
    void permissionChosen(int cookie, const QString &optionId, const QString &label);
    void toolToggled(int row);

private:
    struct LayoutRects {
        QRect card;
        QRect allow;
        QRect reject;
        QVector<QRect> files;
        QRect expand;
    };

    LayoutRects layoutFor(const QStyleOptionViewItem &option, int row, int width) const;
    QTextDocument *documentFor(const AgentEntry &entry, int width) const;
    int toolExtraHeight(const AgentEntry &entry, int width) const;

    AgentThread *m_thread = nullptr;
    mutable QHash<QString, QTextDocument *> m_docs;
    mutable QHash<QString, int> m_docWidth;
    mutable QHash<QString, QString> m_docSource;
};

class AgentTranscriptView : public QListView {
    Q_OBJECT

public:
    explicit AgentTranscriptView(QWidget *parent = nullptr);
    void setThread(AgentThread *thread);
    void scrollToLatest();

signals:
    void openFileRequested(const QString &path);
    void permissionChosen(int cookie, const QString &optionId, const QString &label);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    AgentEntryDelegate *m_delegate = nullptr;
    AgentThread *m_thread = nullptr;
};

} // namespace Orbit
