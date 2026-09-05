#include "ui/Icons.h"
#include <QPainter>
#include <QPainterPath>

namespace Orbit {

QIcon Icons::orbit(int size, const QColor &color) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Center dot
    const qreal center = size / 2.0;
    const qreal dotRadius = size * 0.16;
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(center, center), dotRadius, dotRadius);

    // Orbit ellipse
    painter.save();
    painter.translate(center, center);
    painter.rotate(-28.0);
    QPen pen(color, qMax(1.5, size * 0.08));
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(0, 0), size * 0.42, size * 0.20);
    painter.restore();

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

} // namespace Orbit
