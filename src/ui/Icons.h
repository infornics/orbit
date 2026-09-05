#pragma once

#include <QIcon>
#include <QPixmap>
#include <QColor>

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
};

} // namespace Orbit
