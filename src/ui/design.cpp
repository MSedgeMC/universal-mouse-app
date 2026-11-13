// Original UI drawing and component code, Copyright 2026 MSedgeMC. MIT.
#include "design.h"
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
namespace mouse {
QLabel *designLabel(const QString &text, const QString &role) {
  auto *label = new QLabel(text);
  label->setProperty("role", role);
  if (role == "muted")
    label->setWordWrap(true);
  return label;
}
QFrame *designCard(QVBoxLayout **layout, const QString &title,
                   const QString &subtitle) {
  auto *frame = new QFrame;
  frame->setProperty("surface", "card");
  *layout = new QVBoxLayout(frame);
  (*layout)->setContentsMargins(20, 18, 20, 18);
  (*layout)->setSpacing(13);
  if (!title.isEmpty())
    (*layout)->addWidget(designLabel(title, "section"));
  if (!subtitle.isEmpty())
    (*layout)->addWidget(designLabel(subtitle, "muted"));
  return frame;
}
void applyDesign(QWidget *widget) {
  QFile f(":/design/theme.qss");
  if (f.open(QIODevice::ReadOnly))
    widget->setStyleSheet(QString::fromUtf8(f.readAll()));
}
QIcon designIcon(const QString &name, QColor color, int size) {
  QPixmap image(size * 2, size * 2);
  image.setDevicePixelRatio(2);
  image.fill(Qt::transparent);
  QPainter p(&image);
  p.setRenderHint(QPainter::Antialiasing);
  p.scale(size / 24., size / 24.);
  p.setPen(QPen(color, 1.65, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  if (name == "mouse" || name == "brand") {
    p.drawRoundedRect(QRectF(6, 2, 12, 20), 6, 6);
    p.drawLine(12, 3, 12, 8);
    p.drawRoundedRect(QRectF(10.5, 6, 3, 5), 1.5, 1.5);
    if (name == "brand") {
      p.drawLine(2, 6, 2, 11);
      p.drawLine(22, 14, 22, 19);
    }
  } else if (name == "buttons") {
    p.drawRoundedRect(QRectF(4, 4, 7, 7), 2, 2);
    p.drawRoundedRect(QRectF(14, 4, 6, 7), 2, 2);
    p.drawRoundedRect(QRectF(4, 14, 16, 6), 2, 2);
  } else if (name == "performance") {
    p.drawArc(QRectF(3, 4, 18, 18), 0, 180 * 16);
    p.drawLine(12, 13, 17, 7);
    p.drawLine(4, 19, 20, 19);
    p.drawEllipse(QPointF(12, 13), 1.4, 1.4);
  } else if (name == "lighting") {
    QPainterPath path;
    path.moveTo(12, 2);
    path.lineTo(8, 9);
    path.lineTo(3, 9);
    path.lineTo(9, 14);
    path.lineTo(7, 22);
    path.lineTo(13, 16);
    path.lineTo(19, 18);
    path.lineTo(16, 11);
    path.lineTo(21, 5);
    path.lineTo(14, 7);
    path.closeSubpath();
    p.drawPath(path);
  } else if (name == "macro") {
    p.drawRoundedRect(QRectF(3, 4, 18, 16), 3, 3);
    p.drawLine(7, 9, 11, 12);
    p.drawLine(11, 12, 7, 15);
    p.drawLine(14, 15, 17, 15);
  } else if (name == "desktop") {
    p.drawRoundedRect(QRectF(2, 3, 20, 14), 2, 2);
    p.drawLine(12, 17, 12, 21);
    p.drawLine(8, 21, 16, 21);
  } else if (name == "refresh") {
    p.drawArc(QRectF(4, 4, 16, 16), 45 * 16, 280 * 16);
    p.drawLine(19, 3, 19, 9);
    p.drawLine(19, 9, 13, 9);
  } else if (name == "save") {
    p.drawRoundedRect(QRectF(4, 3, 16, 18), 2, 2);
    p.drawRect(QRectF(8, 3, 8, 6));
    p.drawRect(QRectF(8, 13, 8, 8));
  } else if (name == "restore") {
    p.drawArc(QRectF(4, 4, 16, 16), -60 * 16, 270 * 16);
    p.drawLine(3, 4, 3, 10);
    p.drawLine(3, 10, 9, 10);
    p.drawLine(12, 8, 12, 13);
    p.drawLine(12, 13, 16, 15);
  } else if (name == "arrow") {
    p.drawLine(4, 12, 20, 12);
    p.drawLine(15, 7, 20, 12);
    p.drawLine(20, 12, 15, 17);
  } else {
    p.drawEllipse(QRectF(4, 4, 16, 16));
    p.drawLine(12, 9, 12, 16);
    p.drawPoint(QPointF(12, 6));
  }
  return QIcon(image);
}
} // namespace mouse
