#pragma once

#include <QWidget>
#include <QString>
#include <QModelIndex>

class QFileSystemModel;
class QTreeView;
class QStackedWidget;
class QLabel;
class QPushButton;

namespace Orbit {

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
    QTreeView *m_treeView;
    QStackedWidget *m_stackedWidget;
    QLabel *m_folderTitleLabel;
    QPushButton *m_openFolderBtn;
};

} // namespace Orbit
