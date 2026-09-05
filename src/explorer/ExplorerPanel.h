#pragma once

#include <QWidget>
#include <QTreeView>
#include <QString>
#include <QModelIndex>

class QFileSystemModel;
class QStackedWidget;
class QLabel;
class QPushButton;

namespace Orbit {

class ExplorerTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit ExplorerTreeView(QWidget *parent = nullptr);

protected:
    void drawRow(QPainter *painter, const QStyleOptionViewItem &options, const QModelIndex &index) const override;
    void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QModelIndex m_hoveredIndex;
};

class ExplorerPanel : public QWidget {
    Q_OBJECT

public:
    explicit ExplorerPanel(QWidget *parent = nullptr);

    void setRootFolder(const QString &folderPath);
    QString currentFolderPath() const;

signals:
    void fileActivated(const QString &filePath);
    void requestOpenFolder();

private slots:
    void onItemActivated(const QModelIndex &index);
    void onItemClicked(const QModelIndex &index);

private:
    void setupUi();

    QString m_currentFolderPath;
    QFileSystemModel *m_model;
    ExplorerTreeView *m_treeView;
    QStackedWidget *m_stackedWidget;
    QLabel *m_folderTitleLabel;
    QPushButton *m_openFolderBtn;
};

} // namespace Orbit
