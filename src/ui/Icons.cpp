#include "ui/Icons.h"
#include <QPainter>
#include <QPainterPath>

namespace Orbit {

QIcon Icons::orbit(int size, const QColor &color) {
    QIcon svgIcon(":/icons/orbit.svg");
    if (!svgIcon.isNull() && !svgIcon.pixmap(size, size).isNull()) {
        return svgIcon;
    }

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal center = size / 2.0;
    QColor strokeColor = color.isValid() ? color : QColor(0, 242, 254);
    QPen pen(strokeColor, qMax(2.0, size * 0.08));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    // Main Circle
    painter.drawEllipse(QPointF(center, center), size * 0.30, size * 0.30);

    // Tilted Orbital Ring
    painter.save();
    painter.translate(center, center);
    painter.rotate(-35.0);
    painter.drawEllipse(QPointF(0, 0), size * 0.42, size * 0.18);
    painter.restore();

    // Code Bracket Angle (<)
    QPainterPath path;
    path.moveTo(center + size * 0.08, center - size * 0.14);
    path.lineTo(center - size * 0.10, center);
    path.lineTo(center + size * 0.08, center + size * 0.14);
    painter.drawPath(path);

    return QIcon(pixmap);
}

QIcon Icons::folder(int size, const QColor &color) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, qMax(1.4, size * 0.09));
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const qreal m = size * 0.12;
    const qreal w = size - 2 * m;
    const qreal h = size - 2 * m;

    QPainterPath path;
    path.moveTo(m, m + h * 0.2);
    path.lineTo(m + w * 0.38, m + h * 0.2);
    path.lineTo(m + w * 0.48, m + h * 0.36);
    path.lineTo(m + w, m + h * 0.36);
    path.lineTo(m + w, m + h);
    path.lineTo(m, m + h);
    path.closeSubpath();

    painter.drawPath(path);
    return QIcon(pixmap);
}

QIcon Icons::folderOpen(int size, const QColor &color) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, qMax(1.4, size * 0.09));
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const qreal m = size * 0.12;
    const qreal w = size - 2 * m;
    const qreal h = size - 2 * m;

    // Back folder flap
    QPainterPath back;
    back.moveTo(m, m + h * 0.85);
    back.lineTo(m, m + h * 0.2);
    back.lineTo(m + w * 0.38, m + h * 0.2);
    back.lineTo(m + w * 0.48, m + h * 0.36);
    back.lineTo(m + w * 0.85, m + h * 0.36);
    back.lineTo(m + w * 0.85, m + h * 0.55);
    painter.drawPath(back);

    // Front open flap
    QPainterPath front;
    front.moveTo(m + w * 0.12, m + h * 0.52);
    front.lineTo(m + w, m + h * 0.52);
    front.lineTo(m + w * 0.85, m + h);
    front.lineTo(m, m + h);
    front.closeSubpath();
    painter.drawPath(front);

    return QIcon(pixmap);
}

QIcon Icons::file(int size, const QColor &color) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, qMax(1.4, size * 0.09));
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const qreal mx = size * 0.20;
    const qreal my = size * 0.10;
    const qreal w = size - 2 * mx;
    const qreal h = size - 2 * my;
    const qreal fold = w * 0.36;

    QPainterPath path;
    path.moveTo(mx, my);
    path.lineTo(mx + w - fold, my);
    path.lineTo(mx + w, my + fold);
    path.lineTo(mx + w, my + h);
    path.lineTo(mx, my + h);
    path.closeSubpath();
    painter.drawPath(path);

    // Fold line
    painter.drawLine(QPointF(mx + w - fold, my), QPointF(mx + w - fold, my + fold));
    painter.drawLine(QPointF(mx + w - fold, my + fold), QPointF(mx + w, my + fold));

    return QIcon(pixmap);
}

QIcon Icons::save(int size, const QColor &color) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, qMax(1.4, size * 0.09));
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const qreal m = size * 0.15;
    const qreal w = size - 2 * m;
    const qreal h = size - 2 * m;
    const qreal notch = w * 0.25;

    // Body
    QPainterPath path;
    path.moveTo(m, m);
    path.lineTo(m + w - notch, m);
    path.lineTo(m + w, m + notch);
    path.lineTo(m + w, m + h);
    path.lineTo(m, m + h);
    path.closeSubpath();
    painter.drawPath(path);

    // Top metal slide
    painter.drawRect(QRectF(m + w * 0.25, m, w * 0.50, h * 0.32));

    // Bottom label window
    painter.drawRect(QRectF(m + w * 0.20, m + h * 0.55, w * 0.60, h * 0.45));

    return QIcon(pixmap);
}

QIcon Icons::close(int size, const QColor &color) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, qMax(1.5, size * 0.10));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    const qreal m = size * 0.26;
    painter.drawLine(QPointF(m, m), QPointF(size - m, size - m));
    painter.drawLine(QPointF(size - m, m), QPointF(m, size - m));

    return QIcon(pixmap);
}

QIcon Icons::plus(int size, const QColor &color) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, qMax(1.5, size * 0.10));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    const qreal m = size * 0.22;
    const qreal mid = size / 2.0;
    painter.drawLine(QPointF(m, mid), QPointF(size - m, mid));
    painter.drawLine(QPointF(mid, m), QPointF(mid, size - m));

    return QIcon(pixmap);
}

QIcon Icons::check(int size, const QColor &color) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, qMax(1.8, size * 0.12));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);

    QPainterPath path;
    path.moveTo(size * 0.18, size * 0.52);
    path.lineTo(size * 0.42, size * 0.78);
    path.lineTo(size * 0.84, size * 0.26);
    painter.drawPath(path);

    return QIcon(pixmap);
}

QIcon Icons::antigravity(int size, const QColor &color) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);

    QPainterPath bolt;
    bolt.moveTo(size * 0.58, size * 0.08);
    bolt.lineTo(size * 0.22, size * 0.54);
    bolt.lineTo(size * 0.48, size * 0.54);
    bolt.lineTo(size * 0.40, size * 0.92);
    bolt.lineTo(size * 0.80, size * 0.42);
    bolt.lineTo(size * 0.54, size * 0.42);
    bolt.closeSubpath();
    painter.drawPath(bolt);

    return QIcon(pixmap);
}

QIcon Icons::terminal(int size, const QColor &color) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, qMax(1.4, size * 0.10));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);

    // Prompt prompt >
    QPainterPath prompt;
    prompt.moveTo(size * 0.18, size * 0.28);
    prompt.lineTo(size * 0.44, size * 0.50);
    prompt.lineTo(size * 0.18, size * 0.72);
    painter.drawPath(prompt);

    // Underscore cursor _
    painter.drawLine(QPointF(size * 0.52, size * 0.72), QPointF(size * 0.82, size * 0.72));

    return QIcon(pixmap);
}

QIcon Icons::fileForPath(const QString &fileName, int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QString nameLower = fileName.toLower();
    QString ext;
    int dotIdx = nameLower.lastIndexOf('.');
    if (dotIdx != -1) {
        ext = nameLower.mid(dotIdx + 1);
    }

    QColor fileColor("#94a3b8"); // default slate
    QString label;

    if (nameLower == "cmakelists.txt" || ext == "cmake") {
        fileColor = QColor("#34d399"); // Emerald Green
        label = "CM";
    } else if (ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "c") {
        fileColor = QColor("#38bdf8"); // Cyan Blue
        label = "C+";
    } else if (ext == "h" || ext == "hpp" || ext == "hh") {
        fileColor = QColor("#c084fc"); // Violet Purple
        label = "H";
    } else if (ext == "md" || ext == "markdown") {
        fileColor = QColor("#60a5fa"); // Sky Blue
        label = "M↓";
    } else if (ext == "json") {
        fileColor = QColor("#fbbf24"); // Amber Gold
        label = "{}";
    } else if (ext == "py" || ext == "ipynb") {
        fileColor = QColor("#3b82f6"); // Python Blue
        label = "PY";
    } else if (ext == "js" || ext == "jsx" || ext == "mjs") {
        fileColor = QColor("#facc15"); // Yellow
        label = "JS";
    } else if (ext == "ts" || ext == "tsx") {
        fileColor = QColor("#3b82f6"); // TS Blue
        label = "TS";
    } else if (ext == "html" || ext == "htm") {
        fileColor = QColor("#f97316"); // Orange
        label = "<>";
    } else if (ext == "css" || ext == "scss" || ext == "sass" || ext == "less") {
        fileColor = QColor("#ec4899"); // Hot Pink
        label = "#";
    } else if (nameLower.startsWith(".git") || ext == "yml" || ext == "yaml" || ext == "env") {
        fileColor = QColor("#f97316"); // Git Coral
        label = "⚙";
    } else if (ext == "sh" || ext == "bash" || ext == "zsh" || ext == "fish") {
        fileColor = QColor("#22c55e"); // Terminal Green
        label = ">_";
    } else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "svg" || ext == "gif" || ext == "webp") {
        fileColor = QColor("#f43f5e"); // Rose Pink
        label = "IMG";
    }

    // Sheet background
    const qreal m = size * 0.12;
    const qreal w = size - 2 * m;
    const qreal h = size - 2 * m;

    QPainterPath sheet;
    qreal fold = w * 0.32;
    sheet.moveTo(m, m);
    sheet.lineTo(m + w - fold, m);
    sheet.lineTo(m + w, m + fold);
    sheet.lineTo(m + w, m + h);
    sheet.lineTo(m, m + h);
    sheet.closeSubpath();

    QPen pen(fileColor, qMax(1.3, size * 0.08));
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    QColor fillCol = fileColor;
    fillCol.setAlpha(35);
    painter.setBrush(fillCol);
    painter.drawPath(sheet);

    // Folded corner line
    QPainterPath foldPath;
    foldPath.moveTo(m + w - fold, m);
    foldPath.lineTo(m + w - fold, m + fold);
    foldPath.lineTo(m + w, m + fold);
    painter.drawPath(foldPath);

    // Label or Badge
    if (!label.isEmpty()) {
        painter.setFont(QFont("sans-serif", qMax(6, int(size * 0.38)), QFont::Bold));
        painter.setPen(fileColor);
        painter.drawText(QRectF(m, m + h * 0.25, w, h * 0.7), Qt::AlignCenter, label);
    }

    return QIcon(pixmap);
}

QIcon Icons::folderForDir(const QString &dirName, bool isOpen, int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QString nameLower = dirName.toLower();
    QColor folderColor("#4f8cf6"); // Default Orbit Blue

    if (nameLower == "src" || nameLower == "app" || nameLower == "lib" || nameLower == "core") {
        folderColor = QColor("#38bdf8"); // Cyan
    } else if (nameLower == "test" || nameLower == "tests" || nameLower == "spec") {
        folderColor = QColor("#c084fc"); // Purple
    } else if (nameLower == "doc" || nameLower == "docs") {
        folderColor = QColor("#34d399"); // Emerald
    } else if (nameLower == "build" || nameLower == "dist" || nameLower == "out" || nameLower == "target") {
        folderColor = QColor("#fbbf24"); // Amber
    } else if (nameLower == "ui" || nameLower == "components" || nameLower == "views") {
        folderColor = QColor("#ec4899"); // Pink
    } else if (nameLower == "resources" || nameLower == "assets" || nameLower == "public" || nameLower == "icons") {
        folderColor = QColor("#06b6d4"); // Cyan
    } else if (nameLower.startsWith(".git") || nameLower == ".agents" || nameLower == ".gemini") {
        folderColor = QColor("#f97316"); // Coral
    }

    QPen pen(folderColor, qMax(1.4, size * 0.09));
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    QColor fillCol = folderColor;
    fillCol.setAlpha(isOpen ? 45 : 30);
    painter.setBrush(fillCol);

    const qreal m = size * 0.10;
    const qreal w = size - 2 * m;
    const qreal h = size - 2 * m;

    if (!isOpen) {
        // Closed Folder
        QPainterPath path;
        path.moveTo(m, m + h * 0.2);
        path.lineTo(m + w * 0.38, m + h * 0.2);
        path.lineTo(m + w * 0.48, m + h * 0.36);
        path.lineTo(m + w, m + h * 0.36);
        path.lineTo(m + w, m + h);
        path.lineTo(m, m + h);
        path.closeSubpath();
        painter.drawPath(path);
    } else {
        // Open Folder with dynamic front flap
        QPainterPath back;
        back.moveTo(m, m + h * 0.85);
        back.lineTo(m, m + h * 0.2);
        back.lineTo(m + w * 0.38, m + h * 0.2);
        back.lineTo(m + w * 0.48, m + h * 0.36);
        back.lineTo(m + w * 0.85, m + h * 0.36);
        back.lineTo(m + w * 0.85, m + h * 0.55);
        painter.drawPath(back);

        QPainterPath front;
        front.moveTo(m, m + h * 0.48);
        front.lineTo(m + w * 0.85, m + h * 0.48);
        front.lineTo(m + w, m + h);
        front.lineTo(m + w * 0.15, m + h);
        front.closeSubpath();
        fillCol.setAlpha(60);
        painter.setBrush(fillCol);
        painter.drawPath(front);
    }

    return QIcon(pixmap);
}

} // namespace Orbit
