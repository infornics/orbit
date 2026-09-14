#pragma once

#include <QIcon>
#include <QPixmap>
#include <QColor>
#include <QString>

namespace Orbit {

class Icons {
public:
    static QIcon orbit(int size = 24, const QColor &color = QColor(0x4f, 0x8c, 0xf6));
    static QIcon folder(int size = 16, const QColor &color = QColor(0x8e, 0x8e, 0x9e));
    static QIcon folderOpen(int size = 16, const QColor &color = QColor(0x4f, 0x8c, 0xf6));
    static QIcon file(int size = 16, const QColor &color = QColor(0x9d, 0xa2, 0xb0));
    static QIcon save(int size = 16, const QColor &color = QColor(0x9d, 0xa2, 0xb0));
    static QIcon close(int size = 16, const QColor &color = QColor(0x9d, 0xa2, 0xb0));
    static QIcon plus(int size = 16, const QColor &color = QColor(0x9d, 0xa2, 0xb0));
    static QIcon check(int size = 14, const QColor &color = QColor(0x4f, 0x8c, 0xf6));
    static QIcon antigravity(int size = 16, const QColor &color = QColor(0x4f, 0x8c, 0xf6));
    static QIcon terminal(int size = 16, const QColor &color = QColor(0x4f, 0x8c, 0xf6));

    // File-type & Folder specific vector icon generators (Material UI / VS Code styled)
    static QIcon fileForPath(const QString &fileName, int size = 16);
    static QIcon folderForDir(const QString &dirName, bool isOpen = false, int size = 16);
};

} // namespace Orbit
