#include "ExplorerPanel.h"
#include "Icons.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileSystemModel>
#include <QStackedWidget>
#include <QFileInfo>
#include <QDir>
#include <QIcon>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>

namespace Orbit {

ExplorerTreeView::ExplorerTreeView(QWidget *parent)
    : QTreeView(parent) {
    setMouseTracking(true);
    if (viewport()) {
        viewport()->setMouseTracking(true);
    }
}

void ExplorerTreeView::mouseMoveEvent(QMouseEvent *event) {
    QModelIndex index = indexAt(event->pos());
    if (index != m_hoveredIndex) {
        m_hoveredIndex = index;
        viewport()->update();
    }
    QTreeView::mouseMoveEvent(event);
}

void ExplorerTreeView::leaveEvent(QEvent *event) {
    if (m_hoveredIndex.isValid()) {
        m_hoveredIndex = QModelIndex();
        viewport()->update();
    }
    QTreeView::leaveEvent(event);
}

void ExplorerTreeView::wheelEvent(QWheelEvent *event) {
    QTreeView::wheelEvent(event);
    if (viewport()) {
        m_hoveredIndex = indexAt(viewport()->mapFromGlobal(QCursor::pos()));
        viewport()->update();
    }
}

void ExplorerTreeView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            setCurrentIndex(index);
            if (selectionModel()) {
                selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
        }
    }
    QTreeView::mousePressEvent(event);
}

void ExplorerTreeView::drawRow(QPainter *painter, const QStyleOptionViewItem &options, const QModelIndex &index) const {
    if (!index.isValid()) return;

    bool isSelected = selectionModel() && selectionModel()->isSelected(index);
    bool isHovered = (index == m_hoveredIndex);

    // 1. Draw unified full-row highlight across entire viewport width
    if (isSelected || isHovered) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);

        if (isSelected) {
            painter->setBrush(hasFocus() ? QColor(0x2a, 0x2e, 0x40) : QColor(0x24, 0x28, 0x36));
        } else {
            painter->setBrush(QColor(0x22, 0x22, 0x29));
        }
        QRect rowHighlight(2, options.rect.top() + 1, viewport()->width() - 4, options.rect.height() - 2);
        painter->drawRoundedRect(rowHighlight, 4, 4);
        painter->restore();
    }

    // 2. Draw branch chevron
    QRect itemRect = visualRect(index);
    QRect branchRect(0, options.rect.top(), itemRect.left(), options.rect.height());
    drawBranches(painter, branchRect, index);

    // 3. Draw item (icon and label) via delegate
    QStyleOptionViewItem opt = options;
    opt.rect = itemRect;
    opt.state &= ~QStyle::State_HasFocus;
    opt.state &= ~QStyle::State_Selected;

    if (isSelected) {
        opt.palette.setColor(QPalette::Text, Qt::white);
        opt.palette.setColor(QPalette::WindowText, Qt::white);
    }

    auto *del = itemDelegateForIndex(index);
    if (del) {
        del->paint(painter, opt, index);
    }
}

void ExplorerTreeView::drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const {
    if (!index.isValid()) return;

    auto *fsModel = qobject_cast<const QFileSystemModel*>(model());
    bool isDir = fsModel ? fsModel->isDir(index) : model()->hasChildren(index);

    if (!isDir) return;

    int ind = indentation();
    int arrowAreaLeft = rect.right() - ind + 1;
    int centerX = arrowAreaLeft + ind / 2 + 1;
    int centerY = rect.top() + rect.height() / 2;

    bool isSelected = selectionModel() && selectionModel()->isSelected(index);
    bool isHovered = (index == m_hoveredIndex);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QColor arrowColor = (isSelected || isHovered) ? QColor(0xea, 0xea, 0xf0) : QColor(0x72, 0x72, 0x82);
    QPen pen(arrowColor, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    if (isExpanded(index)) {
        // Down chevron (expanded / open)
        QPainterPath path;
        path.moveTo(centerX - 3.5, centerY - 1.5);
        path.lineTo(centerX, centerY + 2.5);
        path.lineTo(centerX + 3.5, centerY - 1.5);
        painter->drawPath(path);
    } else {
        // Right chevron (collapsed / closed)
        QPainterPath path;
        path.moveTo(centerX - 1.5, centerY - 3.5);
        path.lineTo(centerX + 2.5, centerY);
        path.lineTo(centerX - 1.5, centerY + 3.5);
        painter->drawPath(path);
    }

    painter->restore();
}

ExplorerPanel::ExplorerPanel(QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_treeView(nullptr)
    , m_stackedWidget(nullptr)
    , m_folderTitleLabel(nullptr)
    , m_openFolderBtn(nullptr) {

    setupUi();
}

void ExplorerPanel::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Sidebar Header
    auto *headerWidget = new QWidget(this);
    headerWidget->setFixedHeight(36);
    headerWidget->setStyleSheet(QString(R"(
        QWidget {
            background-color: #18181c;
            border-bottom: 1px solid #24242b;
        }
    )"));

    auto *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(12, 0, 8, 0);
    headerLayout->setSpacing(8);

    m_folderTitleLabel = new QLabel(tr("EXPLORER"), headerWidget);
    m_folderTitleLabel->setStyleSheet(QString(R"(
        QLabel {
            color: #8b8b9a;
            font-size: 11px;
            font-weight: 700;
            letter-spacing: 0.5px;
            border: none;
        }
    )"));

    m_openFolderBtn = new QPushButton(headerWidget);
    m_openFolderBtn->setIcon(Icons::folderOpen(16));
    m_openFolderBtn->setIconSize(QSize(16, 16));
    m_openFolderBtn->setToolTip(tr("Open Folder (Ctrl+Shift+O)"));
    m_openFolderBtn->setFixedSize(26, 26);
    m_openFolderBtn->setCursor(Qt::PointingHandCursor);
    m_openFolderBtn->setStyleSheet(QString(R"(
        QPushButton {
            background: transparent;
            border: none;
            border-radius: 4px;
            padding: 2px;
        }
        QPushButton:hover {
            background-color: #262630;
        }
        QPushButton:pressed {
            background-color: #30303c;
        }
    )"));
    connect(m_openFolderBtn, &QPushButton::clicked, this, &ExplorerPanel::requestOpenFolder);

    headerLayout->addWidget(m_folderTitleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_openFolderBtn);

    mainLayout->addWidget(headerWidget);

    // Stacked Widget: Page 0 = Empty State, Page 1 = Tree View
    m_stackedWidget = new QStackedWidget(this);

    // --- Page 0: Empty State ---
    auto *emptyWidget = new QWidget(m_stackedWidget);
    emptyWidget->setStyleSheet("background-color: #18181c;");
    auto *emptyLayout = new QVBoxLayout(emptyWidget);
    emptyLayout->setContentsMargins(20, 40, 20, 20);
    emptyLayout->setSpacing(14);
    emptyLayout->setAlignment(Qt::AlignCenter);

    auto *emptyIconLabel = new QLabel(emptyWidget);
    emptyIconLabel->setPixmap(Icons::folder(48).pixmap(48, 48));
    emptyIconLabel->setAlignment(Qt::AlignCenter);

    auto *emptyTitleLabel = new QLabel(tr("No Folder Open"), emptyWidget);
    emptyTitleLabel->setAlignment(Qt::AlignCenter);
    emptyTitleLabel->setStyleSheet("color: #e2e2ea; font-size: 13px; font-weight: 600; border: none;");

    auto *emptySubLabel = new QLabel(tr("Open a project folder to browse and edit files."), emptyWidget);
    emptySubLabel->setAlignment(Qt::AlignCenter);
    emptySubLabel->setWordWrap(true);
    emptySubLabel->setStyleSheet("color: #727282; font-size: 11px; border: none;");

    auto *emptyOpenBtn = new QPushButton(tr("Open Folder"), emptyWidget);
    emptyOpenBtn->setObjectName("primaryButton");
    emptyOpenBtn->setCursor(Qt::PointingHandCursor);
    emptyOpenBtn->setFixedWidth(140);
    connect(emptyOpenBtn, &QPushButton::clicked, this, &ExplorerPanel::requestOpenFolder);

    emptyLayout->addWidget(emptyIconLabel);
    emptyLayout->addWidget(emptyTitleLabel);
    emptyLayout->addWidget(emptySubLabel);
    emptyLayout->addSpacing(6);
    emptyLayout->addWidget(emptyOpenBtn, 0, Qt::AlignCenter);
    emptyLayout->addStretch();

    m_stackedWidget->addWidget(emptyWidget);

    // --- Page 1: Tree View ---
    m_model = new QFileSystemModel(this);
    m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

    m_treeView = new ExplorerTreeView(m_stackedWidget);
    m_treeView->setModel(m_model);
    m_treeView->setHeaderHidden(true);
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(20);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setExpandsOnDoubleClick(false);

    // Hide extra columns (size, type, date)
    for (int col = 1; col < m_model->columnCount(); ++col) {
        m_treeView->hideColumn(col);
    }

    connect(m_treeView, &QTreeView::activated, this, &ExplorerPanel::onItemActivated);
    connect(m_treeView, &QTreeView::clicked, this, &ExplorerPanel::onItemClicked);

    m_stackedWidget->addWidget(m_treeView);
    mainLayout->addWidget(m_stackedWidget);

    // Default to empty state
    m_stackedWidget->setCurrentIndex(0);
}

void ExplorerPanel::setRootFolder(const QString &folderPath) {
    if (folderPath.isEmpty() || !QDir(folderPath).exists()) {
        m_currentFolderPath.clear();
        m_folderTitleLabel->setText(tr("EXPLORER"));
        m_stackedWidget->setCurrentIndex(0);
        return;
    }

    m_currentFolderPath = folderPath;
    QDir dir(folderPath);
    m_folderTitleLabel->setText(dir.dirName().toUpper());

    const QModelIndex rootIndex = m_model->setRootPath(folderPath);
    m_treeView->setRootIndex(rootIndex);

    // Ensure columns are hidden after root path change
    for (int col = 1; col < m_model->columnCount(); ++col) {
        m_treeView->hideColumn(col);
    }

    m_stackedWidget->setCurrentIndex(1);
}

QString ExplorerPanel::currentFolderPath() const {
    return m_currentFolderPath;
}

void ExplorerPanel::onItemActivated(const QModelIndex &index) {
    if (!index.isValid()) return;

    QFileInfo fileInfo = m_model->fileInfo(index);
    if (fileInfo.isFile()) {
        emit fileActivated(fileInfo.absoluteFilePath());
    }
}

void ExplorerPanel::onItemClicked(const QModelIndex &index) {
    if (!index.isValid()) return;

    QFileInfo fileInfo = m_model->fileInfo(index);
    if (fileInfo.isFile()) {
        emit fileActivated(fileInfo.absoluteFilePath());
    } else if (fileInfo.isDir()) {
        m_treeView->setExpanded(index, !m_treeView->isExpanded(index));
    }
}

} // namespace Orbit
