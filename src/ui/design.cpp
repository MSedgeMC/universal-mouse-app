// Original UI drawing and component code, Copyright 2026 MSedgeMC. MIT.
#include "design.h"
#include <QFile>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStyle>
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
  frame->setProperty("surface", "panel");
  *layout = new QVBoxLayout(frame);
  (*layout)->setContentsMargins(22, 18, 22, 18);
  (*layout)->setSpacing(12);
  if (!title.isEmpty())
    (*layout)->addWidget(designLabel(title.toUpper(), "section"));
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
  if (name == "brand") {
    p.save();
    p.translate(12, 12);
    p.rotate(-32);
    p.drawEllipse(QRectF(-10, -5, 20, 10));
    p.restore();
    p.drawEllipse(QRectF(6, 3, 12, 18));
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(20.2, 7.4), 2.1, 2.1);
  } else if (name == "mouse") {
    p.drawRoundedRect(QRectF(6, 2, 12, 20), 6, 6);
    p.drawLine(12, 3, 12, 8);
    p.drawRoundedRect(QRectF(10.5, 6, 3, 5), 1.5, 1.5);
  } else if (name == "layers") {
    p.drawPolygon(QPolygonF({{3, 8}, {12, 3}, {21, 8}, {12, 13}}));
    p.drawLine(3, 13, 12, 18);
    p.drawLine(12, 18, 21, 13);
    p.drawLine(3, 18, 12, 23);
    p.drawLine(12, 23, 21, 18);
  } else if (name == "close") {
    p.drawLine(6, 6, 18, 18);
    p.drawLine(18, 6, 6, 18);
  } else if (name == "more") {
    p.setBrush(color);
    for (int x : {5, 12, 19})
      p.drawEllipse(QPointF(x, 12), 1.2, 1.2);
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
LevelSpin::LevelSpin(bool brightness) : brightness(brightness) {
  setRange(0, 7);
  lineEdit()->setReadOnly(true);
}
QString LevelSpin::textFromValue(int value) const {
  if (brightness)
    return value <= 6 ? QString::number(qRound((6 - value) * 100. / 6)) + " %"
                      : "Unverified value";
  const QStringList names = {"Slowest",        "Slow",          "Balanced",
                             "Quick",          "Fast",          "Fastest",
                             "Unverified (6)", "Unverified (7)"};
  return names.value(value);
}
QSize MacroEventDelegate::sizeHint(const QStyleOptionViewItem &,
                                   const QModelIndex &) const {
  return {260, 62};
}
void MacroEventDelegate::paint(QPainter *p, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {
  p->save();
  p->setRenderHint(QPainter::Antialiasing);
  QRectF r = option.rect.adjusted(6, 2, -6, -2);
  bool selected = option.state & QStyle::State_Selected;
  p->setPen(Qt::NoPen);
  p->setBrush(QColor(selected ? "#16354e" : "#202124"));
  p->drawRoundedRect(r, 4, 4);
  QPointF dot(r.left() + 23, r.center().y());
  p->setBrush(QColor("#2a2c30"));
  p->drawEllipse(dot, 13, 13);
  p->setPen(QColor("#a8abb1"));
  QFont font = option.font;
  font.setPixelSize(11);
  p->setFont(font);
  p->drawText(QRectF(dot - QPointF(13, 13), QSizeF(26, 26)), Qt::AlignCenter,
              QString::number(index.row() + 1));
  QString type = index.data(Qt::UserRole).toString();
  QString key = index.data(Qt::UserRole + 1).toString();
  QString name = type == "delay" ? "Delay" : key;
  QString detail = type == "delay"  ? "Wait before the next event"
                   : type == "down" ? "Key press"
                                    : "Key release";
  font.setPixelSize(15);
  font.setWeight(QFont::Medium);
  p->setFont(font);
  p->setPen(QColor("#f0f1f3"));
  p->drawText(QRectF(r.left() + 49, r.top() + 8, r.width() - 145, 21),
              Qt::AlignLeft | Qt::AlignVCenter, name);
  font.setPixelSize(11);
  p->setFont(font);
  p->setPen(QColor("#90949d"));
  p->drawText(QRectF(r.left() + 49, r.top() + 31, r.width() - 145, 18),
              Qt::AlignLeft | Qt::AlignVCenter, detail);
  p->setPen(QColor(type == "down" ? "#159bff" : "#a3a8b2"));
  QString tail = type == "delay"
                     ? index.data(Qt::UserRole + 2).toString() + " ms"
                 : type == "down" ? "DOWN"
                                  : "UP";
  p->drawText(QRectF(r.right() - 93, r.top(), 76, r.height()),
              Qt::AlignRight | Qt::AlignVCenter, tail);
  if (option.state & QStyle::State_HasFocus) {
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(QColor("#159bff"), 1));
    p->drawRoundedRect(r, 4, 4);
  }
  p->restore();
}
} // namespace mouse
