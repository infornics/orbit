#include "agent/AgentComposer.h"
#include "ui/Icons.h"

#include <QPlainTextEdit>
#include <QListWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QDirIterator>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QUrl>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QRegularExpression>
#include <QTimer>
#include <QTextCursor>
#include <functional>

namespace Orbit {
namespace {

QString mimeForPath(const QString &path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "png") return QStringLiteral("image/png");
    if (suffix == "jpg" || suffix == "jpeg") return QStringLiteral("image/jpeg");
    if (suffix == "gif") return QStringLiteral("image/gif");
    if (suffix == "webp") return QStringLiteral("image/webp");
    if (suffix == "svg") return QStringLiteral("image/svg+xml");
    return QStringLiteral("text/plain");
}

bool isImagePath(const QString &path) {
    return mimeForPath(path).startsWith(QLatin1String("image/"))
        && QFileInfo(path).suffix().toLower() != QLatin1String("svg");
}

bool skipDirName(const QString &name) {
    return name == ".git" || name == "build" || name == "node_modules"
        || name == ".cache" || name == "CMakeFiles" || name == "dist"
        || name == "target" || name == ".idea";
}

class ComposerInput : public QPlainTextEdit {
public:
    std::function<bool(QKeyEvent *)> intercept;
    using QPlainTextEdit::QPlainTextEdit;

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (intercept && intercept(event)) {
            return;
        }
        QPlainTextEdit::keyPressEvent(event);
    }
};

} // namespace

AgentComposer::AgentComposer(QWidget *parent)
    : QWidget(parent) {
    setAcceptDrops(true);
    setObjectName("agentComposerRoot");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 10);
    root->setSpacing(6);

    m_chipBar = new QWidget(this);
    m_chipBar->setVisible(false);
    m_chipLayout = new QHBoxLayout(m_chipBar);
    m_chipLayout->setContentsMargins(0, 0, 0, 0);
    m_chipLayout->setSpacing(6);
    m_chipLayout->addStretch();

    m_input = new ComposerInput(this);
    m_input->setObjectName("agentComposer");
    m_input->setPlaceholderText(tr("Message Antigravity…  @ files   / commands   drop images"));
    m_input->setFixedHeight(78);
    m_input->installEventFilter(this);
    static_cast<ComposerInput *>(m_input)->intercept = [this](QKeyEvent *event) {
        if (m_popup && m_popup->isVisible()) {
            if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Up
                || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
                || event->key() == Qt::Key_Escape || event->key() == Qt::Key_Tab) {
                QApplication::sendEvent(m_popupList, event);
                return true;
            }
        }
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            && !(event->modifiers() & Qt::ShiftModifier)) {
            if (!m_prompting) {
                const QString text = m_input->toPlainText().trimmed();
                if (!text.isEmpty() || !m_attachments.isEmpty()) {
                    emit sendRequested(buildPrompt(text), text);
                    m_input->clear();
                    m_attachments.clear();
                    rebuildChips();
                }
            }
            return true;
        }
        return false;
    };

    auto *toolbar = new QWidget(this);
    toolbar->setStyleSheet("background: transparent; border: none;");
    auto *bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(6);

    m_modeCombo = new QComboBox(toolbar);
    m_modeCombo->setVisible(false);
    m_modeCombo->setCursor(Qt::PointingHandCursor);
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) {
            return;
        }
        emit modeChangeRequested(m_modeCombo->itemData(index).toString());
    });

    m_attachButton = new QPushButton(toolbar);
    m_attachButton->setIcon(Icons::plus(14));
    m_attachButton->setFixedSize(28, 26);
    m_attachButton->setToolTip(tr("Attach file or image"));
    m_attachButton->setCursor(Qt::PointingHandCursor);
    m_attachButton->setFlat(true);
    connect(m_attachButton, &QPushButton::clicked, this, [this]() {
        const QString start = m_workspace.isEmpty() ? QDir::homePath() : m_workspace;
        const QString path = QFileDialog::getOpenFileName(this, tr("Attach"), start);
        if (!path.isEmpty()) {
            if (isImagePath(path)) {
                addImage(path);
            } else {
                addFile(path);
            }
        }
    });

    bar->addWidget(m_modeCombo);
    bar->addWidget(m_attachButton);
    bar->addStretch();

    m_stopButton = new QPushButton(tr("Stop"), toolbar);
    m_stopButton->setVisible(false);
    m_stopButton->setCursor(Qt::PointingHandCursor);
    connect(m_stopButton, &QPushButton::clicked, this, &AgentComposer::stopRequested);

    m_sendButton = new QPushButton(tr("Send"), toolbar);
    m_sendButton->setObjectName("primaryButton");
    m_sendButton->setCursor(Qt::PointingHandCursor);
    connect(m_sendButton, &QPushButton::clicked, this, [this]() {
        const QString text = m_input->toPlainText().trimmed();
        if (text.isEmpty() && m_attachments.isEmpty()) {
            return;
        }
        emit sendRequested(buildPrompt(text), text);
        m_input->clear();
        m_attachments.clear();
        rebuildChips();
    });

    bar->addWidget(m_stopButton);
    bar->addWidget(m_sendButton);

    root->addWidget(m_chipBar);
    root->addWidget(m_input);
    root->addWidget(toolbar);

    m_popup = new QFrame(this, Qt::ToolTip | Qt::FramelessWindowHint);
    m_popup->setObjectName("agentMentionPopup");
    m_popup->setMinimumWidth(260);
    auto *popupLayout = new QVBoxLayout(m_popup);
    popupLayout->setContentsMargins(0, 0, 0, 0);
    m_popupList = new QListWidget(m_popup);
    m_popupList->setFrameShape(QFrame::NoFrame);
    popupLayout->addWidget(m_popupList);
    m_popup->hide();
    connect(m_popupList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (item) {
            applyMention(item->data(Qt::UserRole).toString());
        }
    });

    connect(m_input, &QPlainTextEdit::textChanged, this, [this]() {
        const QTextCursor cursor = m_input->textCursor();
        const QString block = m_input->toPlainText().left(cursor.position());
        if (block.endsWith('@') || block.contains(QRegularExpression(QStringLiteral("@[\\w./-]*$")))) {
            const int at = block.lastIndexOf('@');
            if (at >= 0 && (at == 0 || block[at - 1].isSpace() || block[at - 1] == '\n')) {
                showMentionPopup(false);
                return;
            }
        }
        if (block.endsWith('/') || QRegularExpression(QStringLiteral("(^|\\s)/[\\w-]*$")).match(block).hasMatch()) {
            showMentionPopup(true);
            return;
        }
        hidePopup();
    });
}

void AgentComposer::setWorkspace(const QString &root) {
    if (m_workspace == root) {
        return;
    }
    m_workspace = root;
    m_workspaceFiles.clear();
}

void AgentComposer::setCurrentFile(const QString &path) {
    m_currentFile = path;
}

void AgentComposer::setSelection(const QString &text) {
    m_selection = text;
}

void AgentComposer::setSelectionProvider(std::function<QString()> fn) {
    m_selectionProvider = std::move(fn);
}

void AgentComposer::setCommands(const QVector<AgentCommand> &commands) {
    m_commands = commands;
}

void AgentComposer::setModes(const QVector<AgentMode> &modes, const QString &currentId) {
    m_modeCombo->blockSignals(true);
    m_modeCombo->clear();
    for (const AgentMode &mode : modes) {
        m_modeCombo->addItem(mode.name, mode.id);
        m_modeCombo->setItemData(m_modeCombo->count() - 1, mode.description, Qt::ToolTipRole);
        if (mode.id == currentId) {
            m_modeCombo->setCurrentIndex(m_modeCombo->count() - 1);
        }
    }
    m_modeCombo->setVisible(!modes.isEmpty());
    m_modeCombo->blockSignals(false);
}

void AgentComposer::setPrompting(bool prompting) {
    m_prompting = prompting;
    m_sendButton->setEnabled(!prompting);
    m_stopButton->setVisible(prompting);
    m_input->setReadOnly(prompting);
}

void AgentComposer::focusInput() {
    m_input->setFocus();
}

QString AgentComposer::currentModeId() const {
    return m_modeCombo->currentData().toString();
}

void AgentComposer::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage()) {
        event->acceptProposedAction();
    }
}

void AgentComposer::dropEvent(QDropEvent *event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            const QString path = url.toLocalFile();
            if (path.isEmpty()) {
                continue;
            }
            if (isImagePath(path)) {
                addImage(path);
            } else {
                addFile(path);
            }
        }
        event->acceptProposedAction();
    }
}

void AgentComposer::rebuildChips() {
    while (QLayoutItem *item = m_chipLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    for (int i = 0; i < m_attachments.size(); ++i) {
        const AgentAttachment &att = m_attachments[i];
        auto *chip = new QPushButton(att.name, m_chipBar);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setToolTip(tr("Remove %1").arg(att.name));
        chip->setStyleSheet(R"(
            QPushButton {
                background: #24242d;
                color: #c4c4d0;
                border: 1px solid #32323f;
                border-radius: 11px;
                padding: 2px 10px;
                font-size: 11px;
            }
            QPushButton:hover { border-color: #4f8cf6; color: #ffffff; }
        )");
        connect(chip, &QPushButton::clicked, this, [this, i]() {
            if (i >= 0 && i < m_attachments.size()) {
                m_attachments.removeAt(i);
                rebuildChips();
            }
        });
        m_chipLayout->addWidget(chip);
    }
    m_chipLayout->addStretch();
    m_chipBar->setVisible(!m_attachments.isEmpty());
}

void AgentComposer::addFile(const QString &path) {
    AgentAttachment att;
    att.kind = AgentAttachment::Kind::File;
    att.path = path;
    att.name = QFileInfo(path).fileName();
    att.mime = mimeForPath(path);
    m_attachments.append(att);
    rebuildChips();
}

void AgentComposer::addImage(const QString &path) {
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.size() > 4 * 1024 * 1024) {
        return;
    }
    AgentAttachment att;
    att.kind = AgentAttachment::Kind::Image;
    att.path = path;
    att.name = QFileInfo(path).fileName();
    att.mime = mimeForPath(path);
    att.imageData = bytes;
    m_attachments.append(att);
    rebuildChips();
}

void AgentComposer::showMentionPopup(bool slash) {
    m_slashPopup = slash;
    m_popupList->clear();

    const QTextCursor cursor = m_input->textCursor();
    const QString left = m_input->toPlainText().left(cursor.position());
    QString query;
    if (slash) {
        const int slashPos = left.lastIndexOf('/');
        query = left.mid(slashPos + 1).toLower();
        for (const AgentCommand &cmd : m_commands) {
            if (query.isEmpty() || cmd.name.toLower().startsWith(query) || cmd.name.toLower().contains(query)) {
                auto *item = new QListWidgetItem(QStringLiteral("/%1  %2").arg(cmd.name, cmd.description));
                item->setData(Qt::UserRole, QStringLiteral("/") + cmd.name);
                m_popupList->addItem(item);
            }
        }
        if (m_popupList->count() == 0) {
            hidePopup();
            return;
        }
    } else {
        const int at = left.lastIndexOf('@');
        query = left.mid(at + 1).toLower();
        if (m_workspaceFiles.isEmpty()) {
            indexWorkspace();
        }
        int added = 0;
        for (const QString &path : m_workspaceFiles) {
            const QString name = QFileInfo(path).fileName().toLower();
            if (query.isEmpty() || name.contains(query) || path.toLower().contains(query)) {
                auto *item = new QListWidgetItem(QFileInfo(path).fileName() + QStringLiteral("  ") + path);
                item->setData(Qt::UserRole, path);
                m_popupList->addItem(item);
                if (++added >= 12) {
                    break;
                }
            }
        }
        if (added == 0) {
            hidePopup();
            return;
        }
    }

    if (m_popupList->count() > 0) {
        m_popupList->setCurrentRow(0);
    }
    const QPoint pos = m_input->mapToGlobal(QPoint(0, -8 - m_popup->sizeHint().height()));
    m_popup->move(pos);
    m_popup->resize(qMax(260, m_input->width()), qMin(220, m_popupList->sizeHintForRow(0) * m_popupList->count() + 8));
    m_popup->show();
}

void AgentComposer::hidePopup() {
    m_popup->hide();
}

void AgentComposer::applyMention(const QString &token) {
    hidePopup();
    QTextCursor cursor = m_input->textCursor();
    QString block = m_input->toPlainText().left(cursor.position());
    if (m_slashPopup) {
        const int slashPos = block.lastIndexOf('/');
        cursor.setPosition(slashPos, QTextCursor::KeepAnchor);
        cursor.setPosition(m_input->textCursor().position(), QTextCursor::KeepAnchor);
        cursor.insertText(token + QLatin1Char(' '));
    } else {
        const int at = block.lastIndexOf('@');
        cursor.setPosition(at, QTextCursor::KeepAnchor);
        cursor.setPosition(m_input->textCursor().position(), QTextCursor::KeepAnchor);
        cursor.insertText(QStringLiteral("@") + QFileInfo(token).fileName() + QLatin1Char(' '));
        addFile(token);
    }
    m_input->setTextCursor(cursor);
}

QJsonArray AgentComposer::buildPrompt(const QString &text) const {
    QJsonArray prompt;
    if (!text.isEmpty()) {
        prompt.append(QJsonObject{{"type", "text"}, {"text", text}});
    }
    QString selection = m_selection;
    if (m_selectionProvider) {
        const QString live = m_selectionProvider();
        if (!live.isEmpty()) {
            selection = live;
        }
    }
    if (!selection.isEmpty()) {
        prompt.append(QJsonObject{
            {"type", "resource"},
            {"resource", QJsonObject{
                {"uri", QUrl::fromLocalFile(m_currentFile).toString()},
                {"mimeType", "text/plain"},
                {"text", selection}
            }}
        });
    }

    bool attachedCurrent = false;
    for (const AgentAttachment &att : m_attachments) {
        if (att.kind == AgentAttachment::Kind::Image) {
            prompt.append(QJsonObject{
                {"type", "image"},
                {"mimeType", att.mime},
                {"data", QString::fromLatin1(att.imageData.toBase64())}
            });
        } else {
            prompt.append(QJsonObject{
                {"type", "resource_link"},
                {"uri", QUrl::fromLocalFile(att.path).toString()},
                {"name", att.name},
                {"mimeType", att.mime}
            });
            if (att.path == m_currentFile) {
                attachedCurrent = true;
            }
        }
    }

    if (!m_currentFile.isEmpty() && !attachedCurrent && m_selection.isEmpty()) {
        prompt.append(QJsonObject{
            {"type", "resource_link"},
            {"uri", QUrl::fromLocalFile(m_currentFile).toString()},
            {"name", QFileInfo(m_currentFile).fileName()}
        });
    }
    return prompt;
}

void AgentComposer::indexWorkspace() {
    m_workspaceFiles.clear();
    if (m_workspace.isEmpty()) {
        return;
    }
    QDirIterator it(m_workspace, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString path = it.filePath();
        QString rel = QDir(m_workspace).relativeFilePath(path);
        bool skip = false;
        const QStringList parts = rel.split('/');
        for (const QString &part : parts) {
            if (skipDirName(part)) {
                skip = true;
                break;
            }
        }
        if (skip) {
            continue;
        }
        m_workspaceFiles.append(path);
        if (m_workspaceFiles.size() >= 400) {
            break;
        }
    }
}

bool AgentComposer::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_input && event->type() == QEvent::FocusOut) {
        QTimer::singleShot(150, this, [this]() {
            if (m_popup && !m_popupList->hasFocus()) {
                hidePopup();
            }
        });
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace Orbit
