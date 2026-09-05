#include "agent/AgentTranscriptView.h"
#include "agent/AgentThread.h"
#include "ui/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QFileInfo>
#include <QtMath>
#include <QJsonArray>
#include <QJsonObject>

namespace Orbit {
namespace {

QColor statusColor(const QString &status) {
    if (status == QLatin1String("completed") || status == QLatin1String("completed_successfully")) {
        return QColor(0x7e, 0x9c, 0x6c);
    }
    if (status == QLatin1String("failed") || status == QLatin1String("error")) {
        return QColor(0xf0, 0x71, 0x78);
    }
    if (status == QLatin1String("in_progress") || status == QLatin1String("pending")) {
        return QColor(0x4f, 0x8c, 0xf6);
    }
    return QColor(0x8f, 0x92, 0xa4);
}

QString kindGlyph(const QString &kind) {
    if (kind == QLatin1String("read")) return QStringLiteral("Read");
    if (kind == QLatin1String("edit") || kind == QLatin1String("write")) return QStringLiteral("Edit");
    if (kind == QLatin1String("execute") || kind == QLatin1String("terminal")) return QStringLiteral("Run");
    if (kind == QLatin1String("search")) return QStringLiteral("Search");
    if (kind == QLatin1String("delete")) return QStringLiteral("Delete");
    if (kind == QLatin1String("think")) return QStringLiteral("Think");
    if (kind == QLatin1String("fetch")) return QStringLiteral("Fetch");
    return kind.isEmpty() ? QStringLiteral("Tool") : kind;
}

void drawRounded(QPainter *p, const QRect &rect, const QColor &fill, const QColor &stroke, int radius = 8) {
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    p->fillPath(path, fill);
    p->setPen(QPen(stroke, 1));
    p->drawPath(path);
}

QString optionLabel(const QJsonObject &opt) {
    const QString name = opt.value("name").toString();
    if (!name.isEmpty()) {
        return name;
    }
    return opt.value("optionId").toString();
}

} // namespace

AgentEntryDelegate::AgentEntryDelegate(AgentThread *thread, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_thread(thread) {
}

void AgentEntryDelegate::invalidateCache() {
    qDeleteAll(m_docs);
    m_docs.clear();
    m_docWidth.clear();
    m_docSource.clear();
}

QTextDocument *AgentEntryDelegate::documentFor(const AgentEntry &entry, int width) const {
    if (entry.type != AgentEntryType::Agent && entry.type != AgentEntryType::User) {
        return nullptr;
    }
    const int textWidth = qMax(80, width - 24);
    QTextDocument *doc = m_docs.value(entry.id);
    if (doc && m_docWidth.value(entry.id) == textWidth && m_docSource.value(entry.id) == entry.text) {
        return doc;
    }
    if (!doc) {
        doc = new QTextDocument;
        m_docs.insert(entry.id, doc);
    }
    m_docSource.insert(entry.id, entry.text);
    doc->setDefaultFont(Theme::uiFont(10));
    doc->setDocumentMargin(0);
    QString markdown = entry.text;
    if (entry.type == AgentEntryType::User) {
        doc->setPlainText(entry.text);
    } else {
        doc->setMarkdown(markdown);
    }
    QTextOption opt = doc->defaultTextOption();
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    doc->setDefaultTextOption(opt);
    doc->setDefaultStyleSheet(QStringLiteral(
        "body { color: #d6d6e2; } "
        "code { background: #24242d; color: #c8d4e8; } "
        "pre { background: #16161c; color: #c8d4e8; }"));
    doc->setTextWidth(textWidth);
    m_docWidth.insert(entry.id, textWidth);
    return doc;
}

int AgentEntryDelegate::toolExtraHeight(const AgentEntry &entry, int width) const {
    Q_UNUSED(width);
    if (!entry.tool.expanded) {
        return 0;
    }
    int extra = 0;
    if (!entry.tool.body.isEmpty()) {
        extra += qMin(160, 18 + entry.tool.body.count('\n') * 14);
    }
    extra += entry.tool.diffs.size() * 22;
    extra += entry.tool.locations.size() * 18;
    return extra;
}

AgentEntryDelegate::LayoutRects AgentEntryDelegate::layoutFor(const QStyleOptionViewItem &option, int row, int width) const {
    LayoutRects layout;
    if (!m_thread || row < 0 || row >= m_thread->count()) {
        return layout;
    }
    const AgentEntry &entry = m_thread->at(row);
    const QRect base = option.rect.adjusted(8, 2, -8, -2);
    layout.card = base;

    if (entry.type == AgentEntryType::Permission && !entry.permission.resolved) {
        const int btnY = base.bottom() - 30;
        layout.allow = QRect(base.left() + 10, btnY, 72, 24);
        layout.reject = QRect(layout.allow.right() + 8, btnY, 72, 24);
        const QJsonArray options = entry.permission.options;
        if (options.size() >= 2) {
            layout.allow = QRect(base.left() + 10, btnY, 88, 24);
            layout.reject = QRect(layout.allow.right() + 8, btnY, 88, 24);
        }
    }
    if (entry.type == AgentEntryType::ToolCall) {
        layout.expand = QRect(base.left(), base.top(), base.width(), 28);
        int y = base.top() + 32;
        if (entry.tool.expanded) {
            for (int i = 0; i < entry.tool.locations.size(); ++i) {
                layout.files.append(QRect(base.left() + 12, y, base.width() - 24, 16));
                y += 18;
            }
        }
    }
    Q_UNUSED(width);
    return layout;
}

QSize AgentEntryDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    if (!m_thread || !index.isValid()) {
        return QSize(option.rect.width(), 32);
    }
    const AgentEntry &entry = m_thread->at(index.row());
    const int width = option.widget ? option.widget->width() - 16 : qMax(option.rect.width(), 280);

    switch (entry.type) {
    case AgentEntryType::User: {
        QTextDocument *doc = documentFor(entry, width);
        return QSize(width, qCeil(doc->size().height()) + 20);
    }
    case AgentEntryType::Agent: {
        QTextDocument *doc = documentFor(entry, width);
        return QSize(width, qMax(28, qCeil(doc->size().height()) + 16));
    }
    case AgentEntryType::Thought:
        return QSize(width, 22 + (entry.text.size() / 60) * 14);
    case AgentEntryType::ToolCall:
        return QSize(width, 36 + toolExtraHeight(entry, width));
    case AgentEntryType::Plan:
        return QSize(width, 16 + entry.plan.size() * 22);
    case AgentEntryType::Permission:
        return QSize(width, entry.permission.resolved ? 36 : 78 + entry.permission.diffs.size() * 18);
    case AgentEntryType::System:
        return QSize(width, 28);
    }
    return QSize(width, 32);
}

void AgentEntryDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {
    if (!m_thread || !index.isValid()) {
        return;
    }
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const AgentEntry &entry = m_thread->at(index.row());
    const QRect rect = option.rect.adjusted(8, 3, -8, -3);

    switch (entry.type) {
    case AgentEntryType::User: {
        drawRounded(painter, rect, QColor(0x24, 0x24, 0x2d), QColor(0x32, 0x32, 0x3f), 10);
        QTextDocument *doc = documentFor(entry, option.rect.width());
        painter->translate(rect.left() + 10, rect.top() + 8);
        doc->drawContents(painter, QRectF(0, 0, rect.width() - 20, rect.height()));
        break;
    }
    case AgentEntryType::Agent: {
        QTextDocument *doc = documentFor(entry, option.rect.width());
        painter->translate(rect.left() + 4, rect.top() + 4);
        doc->drawContents(painter, QRectF(0, 0, rect.width() - 8, rect.height()));
        break;
    }
    case AgentEntryType::Thought: {
        painter->setPen(QColor(0x6e, 0x6e, 0x7e));
        QFont italic = Theme::uiFont(9);
        italic.setItalic(true);
        painter->setFont(italic);
        painter->drawText(rect.adjusted(4, 0, -4, 0), Qt::TextWordWrap, entry.text);
        break;
    }
    case AgentEntryType::ToolCall: {
        drawRounded(painter, rect, QColor(0x1c, 0x1c, 0x22), QColor(0x2a, 0x2a, 0x34), 8);
        QColor dot = statusColor(entry.tool.status);
        painter->setBrush(dot);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(QPoint(rect.left() + 14, rect.top() + 16), 4, 4);
        painter->setPen(QColor(0xc4, 0xc4, 0xd0));
        painter->setFont(Theme::uiFont(9));
        const QString label = QStringLiteral("%1  ·  %2")
                                  .arg(kindGlyph(entry.tool.kind), entry.tool.title);
        painter->drawText(rect.adjusted(24, 4, -8, 0),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                          QFontMetrics(painter->font()).elidedText(label, Qt::ElideRight, rect.width() - 36));
        if (entry.tool.expanded) {
            int y = rect.top() + 30;
            painter->setPen(QColor(0x8f, 0x92, 0xa4));
            painter->setFont(Theme::monoFont(8));
            for (const QString &path : entry.tool.locations) {
                painter->drawText(QRect(rect.left() + 14, y, rect.width() - 28, 16),
                                  Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                                  QFileInfo(path).fileName());
                y += 18;
            }
            for (const AgentDiff &diff : entry.tool.diffs) {
                const QString name = QFileInfo(diff.path).fileName();
                painter->setPen(QColor(0x4f, 0x8c, 0xf6));
                painter->drawText(QRect(rect.left() + 14, y, rect.width() - 28, 16),
                                  Qt::AlignLeft | Qt::AlignVCenter,
                                  QStringLiteral("Δ %1").arg(name));
                y += 18;
            }
            if (!entry.tool.body.isEmpty()) {
                painter->setPen(QColor(0x7a, 0x7a, 0x88));
                painter->setFont(Theme::monoFont(8));
                painter->drawText(QRect(rect.left() + 14, y, rect.width() - 28, rect.bottom() - y - 6),
                                  Qt::TextWordWrap,
                                  entry.tool.body.right(1200));
            }
        }
        break;
    }
    case AgentEntryType::Plan: {
        drawRounded(painter, rect, QColor(0x18, 0x1c, 0x24), QColor(0x2a, 0x34, 0x44), 8);
        int y = rect.top() + 8;
        painter->setFont(Theme::uiFont(9));
        for (const AgentPlanStep &step : entry.plan) {
            QColor c = statusColor(step.status);
            painter->setBrush(c);
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(QPoint(rect.left() + 14, y + 7), 3, 3);
            painter->setPen(QColor(0xc4, 0xc4, 0xd0));
            painter->drawText(QRect(rect.left() + 24, y, rect.width() - 34, 18),
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, step.content);
            y += 22;
        }
        break;
    }
    case AgentEntryType::Permission: {
        drawRounded(painter, rect, QColor(0x22, 0x1c, 0x14), QColor(0x5a, 0x4a, 0x2a), 8);
        painter->setPen(QColor(0xe8, 0xd5, 0xa8));
        painter->setFont(Theme::uiFont(10));
        QFont bold = painter->font();
        bold.setBold(true);
        painter->setFont(bold);
        painter->drawText(rect.adjusted(12, 8, -12, 0), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                          entry.permission.title);
        if (entry.permission.resolved) {
            painter->setPen(QColor(0x8f, 0x92, 0xa4));
            painter->setFont(Theme::uiFont(9));
            painter->drawText(rect.adjusted(12, 0, -12, -8), Qt::AlignLeft | Qt::AlignBottom,
                              entry.permission.resolution);
        } else {
            const LayoutRects lay = layoutFor(option, index.row(), rect.width());
            auto drawBtn = [&](const QRect &r, const QString &label, bool primary) {
                drawRounded(painter, r,
                            primary ? QColor(0x3b, 0x74, 0xdb) : QColor(0x26, 0x26, 0x32),
                            primary ? QColor(0x4f, 0x8c, 0xf6) : QColor(0x3c, 0x3c, 0x4c), 5);
                painter->setPen(Qt::white);
                painter->setFont(Theme::uiFont(9));
                painter->drawText(r, Qt::AlignCenter, label);
            };
            QString allow = QStringLiteral("Allow");
            QString deny = QStringLiteral("Reject");
            if (entry.permission.options.size() >= 1) {
                allow = optionLabel(entry.permission.options.at(0).toObject());
            }
            if (entry.permission.options.size() >= 2) {
                deny = optionLabel(entry.permission.options.at(1).toObject());
            }
            drawBtn(lay.allow, allow, true);
            drawBtn(lay.reject, deny, false);
        }
        break;
    }
    case AgentEntryType::System: {
        painter->setPen(QColor(0x7e, 0x7e, 0x8e));
        painter->setFont(Theme::uiFont(9));
        painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, entry.text);
        break;
    }
    }

    painter->restore();
}

bool AgentEntryDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                     const QStyleOptionViewItem &option, const QModelIndex &index) {
    Q_UNUSED(model);
    if (!m_thread || !index.isValid() || event->type() != QEvent::MouseButtonRelease) {
        return false;
    }
    auto *mouse = static_cast<QMouseEvent *>(event);
    const AgentEntry &entry = m_thread->at(index.row());
    const LayoutRects lay = layoutFor(option, index.row(), option.rect.width());

    if (entry.type == AgentEntryType::Permission && !entry.permission.resolved) {
        if (lay.allow.contains(mouse->pos())) {
            QString id = QStringLiteral("allow");
            QString label = QStringLiteral("Allowed");
            if (!entry.permission.options.isEmpty()) {
                const QJsonObject opt = entry.permission.options.at(0).toObject();
                id = opt.value("optionId").toString(id);
                label = optionLabel(opt);
            }
            emit permissionChosen(entry.permission.cookie, id, label);
            return true;
        }
        if (lay.reject.contains(mouse->pos())) {
            QString id = QStringLiteral("reject");
            QString label = QStringLiteral("Rejected");
            if (entry.permission.options.size() >= 2) {
                const QJsonObject opt = entry.permission.options.at(1).toObject();
                id = opt.value("optionId").toString(id);
                label = optionLabel(opt);
            }
            emit permissionChosen(entry.permission.cookie, id, label);
            return true;
        }
    }

    if (entry.type == AgentEntryType::ToolCall) {
        if (entry.tool.expanded) {
            int y = option.rect.top() + 32;
            for (const QString &path : entry.tool.locations) {
                QRect r(option.rect.left() + 20, y, option.rect.width() - 40, 16);
                if (r.contains(mouse->pos())) {
                    emit openFileRequested(path);
                    return true;
                }
                y += 18;
            }
            for (const AgentDiff &diff : entry.tool.diffs) {
                QRect r(option.rect.left() + 20, y, option.rect.width() - 40, 16);
                if (r.contains(mouse->pos())) {
                    emit openFileRequested(diff.path);
                    return true;
                }
                y += 18;
            }
        }
        if (lay.expand.contains(mouse->pos()) || option.rect.contains(mouse->pos())) {
            emit toolToggled(index.row());
            return true;
        }
    }
    return false;
}

AgentTranscriptView::AgentTranscriptView(QWidget *parent)
    : QListView(parent) {
    setObjectName("agentTranscript");
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(ScrollPerPixel);
    setSelectionMode(NoSelection);
    setSpacing(4);
    setMouseTracking(true);
    setResizeMode(Adjust);
    setUniformItemSizes(false);
    viewport()->setAttribute(Qt::WA_Hover);
    setStyleSheet(QStringLiteral(
        "QListView#agentTranscript { background: #18181c; border: none; }"
        "QListView#agentTranscript::item { background: transparent; }"));
}

void AgentTranscriptView::setThread(AgentThread *thread) {
    m_thread = thread;
    setModel(thread);
    if (m_delegate) {
        m_delegate->deleteLater();
    }
    m_delegate = new AgentEntryDelegate(thread, this);
    setItemDelegate(m_delegate);
    connect(m_delegate, &AgentEntryDelegate::openFileRequested, this, &AgentTranscriptView::openFileRequested);
    connect(m_delegate, &AgentEntryDelegate::permissionChosen, this, &AgentTranscriptView::permissionChosen);
    connect(m_delegate, &AgentEntryDelegate::toolToggled, this, [this, thread](int row) {
        thread->toggleToolExpanded(row);
        scheduleDelayedItemsLayout();
    });
    connect(thread, &QAbstractItemModel::rowsInserted, this, [this]() {
        scrollToLatest();
    });
    connect(thread, &QAbstractItemModel::dataChanged, this, [this]() {
        scheduleDelayedItemsLayout();
        scrollToLatest();
    });
}

void AgentTranscriptView::scrollToLatest() {
    QTimer::singleShot(0, this, [this]() {
        if (verticalScrollBar()) {
            verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        }
    });
}

void AgentTranscriptView::resizeEvent(QResizeEvent *event) {
    QListView::resizeEvent(event);
    if (m_delegate) {
        m_delegate->invalidateCache();
        scheduleDelayedItemsLayout();
    }
}

} // namespace Orbit
