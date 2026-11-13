#include "viewer.h"
#include <QJsonArray>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVector3D>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
namespace mouse {
Viewer::Viewer(QWidget *p) : QWidget(p) {
  setMinimumSize(310, 330);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setAccessibleName("Mouse preview and input tester");
  elapsed.start();
  animation.setInterval(50);
  connect(&animation, &QTimer::timeout, this, [this] {
    if (autoRotate && threeD)
      yaw += .7;
    update();
  });
}
void Viewer::setThreeD(bool v) {
  threeD = v;
  syncAnimation();
  update();
}
void Viewer::setColor(QColor c) {
  light = c;
  update();
}
void Viewer::setSelected(int i) {
  chosen = i;
  update();
}
void Viewer::resetView() {
  yaw = -25;
  pitch = 25;
  zoom = 1;
  update();
}
void Viewer::setAutoRotate(bool enabled) {
  autoRotate = enabled;
  syncAnimation();
}
void Viewer::setTestEnabled(bool enabled) {
  testEnabled = enabled;
  if (enabled) {
    counts.fill(0);
    wheelUp = wheelDown = 0;
  }
  emit tested(enabled ? "Listening inside the preview · " + testSummary()
                      : "Test complete · " + testSummary());
}
QString Viewer::testSummary() const {
  return QString(
             "L %1 · R %2 · M %3 · Back %4 · Forward %5 · Wheel ↑ %6 / ↓ %7")
      .arg(counts[0])
      .arg(counts[1])
      .arg(counts[2])
      .arg(counts[3])
      .arg(counts[4])
      .arg(wheelUp)
      .arg(wheelDown);
}
void Viewer::setLighting(const QJsonObject &settings, const QJsonObject &live) {
  auto l = settings["lighting"].toObject();
  lightEffect =
      live.isEmpty() ? l["effect"].toString() : live["effect"].toString();
  lightSpeed = live.isEmpty() ? l["speed_raw"].toInt() : live["speed"].toInt();
  luminosity = qBound(0., (6. - settings["brightness_raw"].toInt()) / 6., 1.);
  palette.clear();
  for (int n = 0; n < 7; ++n) {
    if (!l["enabled"].toArray()[n].toBool())
      continue;
    auto a = l["palette"].toArray()[n].toArray();
    palette << QColor((15 - a[0].toInt()) * 17, (15 - a[1].toInt()) * 17,
                      (15 - a[2].toInt()) * 17);
  }
  if (!live.isEmpty())
    palette = {QColor(live["color"].toString())};
  if (palette.isEmpty())
    palette << QColor("#263442");
  light = palette[0];
  syncAnimation();
  update();
}
void Viewer::syncAnimation() {
  bool moving = (threeD && autoRotate) ||
                lightEffect.contains("Breath", Qt::CaseInsensitive) ||
                lightEffect.contains("Cycle") || lightEffect.contains("Neon") ||
                lightEffect.contains("Water") || lightEffect.contains("Wave");
  if (isVisible() && moving && (lightEffect != "Off" || (autoRotate && threeD)))
    animation.start();
  else
    animation.stop();
}
void Viewer::showEvent(QShowEvent *) { syncAnimation(); }
void Viewer::hideEvent(QHideEvent *) { animation.stop(); }
QColor Viewer::previewColor() const {
  if (lightEffect == "Off")
    return QColor("#263442");
  double seconds = elapsed.elapsed() / 1000.,
         period = 4. - qMin(lightSpeed, 5) * .5;
  QColor color = light;
  if (palette.size() > 1 &&
      (lightEffect.contains("Cycle") || lightEffect.contains("Water") ||
       lightEffect.contains("Wave"))) {
    double slot = std::fmod(seconds / period, double(palette.size()));
    int n = int(slot);
    double fraction = slot - n;
    auto a = palette[n], b = palette[(n + 1) % palette.size()];
    color =
        QColor::fromRgbF(a.redF() * (1 - fraction) + b.redF() * fraction,
                         a.greenF() * (1 - fraction) + b.greenF() * fraction,
                         a.blueF() * (1 - fraction) + b.blueF() * fraction);
  }
  if (lightEffect.contains("Neon"))
    color = QColor::fromHsvF(std::fmod(seconds / period / 3, 1.), .85, 1.);
  double value = luminosity;
  if (lightEffect.contains("Breath", Qt::CaseInsensitive))
    value *= .2 + .8 * (std::sin(seconds / period * 6.283) + 1) / 2;
  return QColor::fromRgbF(color.redF() * value, color.greenF() * value,
                          color.blueF() * value);
}
int Viewer::buttonAt(QPointF point) const {
  if (threeD) {
    for (auto it = pickFaces.crbegin(); it != pickFaces.crend(); ++it)
      if (it->first.containsPoint(point, Qt::OddEvenFill))
        return it->second;
  } else
    for (int n = 0; n < hitRegions.size(); ++n)
      if (hitRegions[n].contains(point))
        return n;
  return -1;
}
QPointF Viewer::buttonCenter(int n) const { return centers.value(n); }
void Viewer::keyPressEvent(QKeyEvent *e) {
  if (e->key() == Qt::Key_Home)
    resetView();
  else if (e->key() == Qt::Key_Left)
    yaw -= 6;
  else if (e->key() == Qt::Key_Right)
    yaw += 6;
  else if (e->key() == Qt::Key_Up)
    pitch = qMin(70., pitch + 6);
  else if (e->key() == Qt::Key_Down)
    pitch = qMax(-70., pitch - 6);
  else if (e->key() == Qt::Key_Plus || e->key() == Qt::Key_Equal)
    zoom = qMin(1.7, zoom + .1);
  else if (e->key() == Qt::Key_Minus)
    zoom = qMax(.6, zoom - .1);
  else {
    QWidget::keyPressEvent(e);
    return;
  }
  update();
  e->accept();
}
void Viewer::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), QColor("#151b22"));
  hitRegions.fill({}, 7);
  centers.fill({}, 7);
  pickFaces.clear();
  QColor glow = previewColor();
  if (!threeD) {
    p.translate(width() / 2., height() / 2. - 8);
    double scale = qMin(width() / 240., height() / 330.);
    p.scale(scale, scale);
    QPainterPath body;
    body.moveTo(0, -115);
    body.cubicTo(65, -115, 82, -65, 80, 28);
    body.cubicTo(78, 115, 49, 133, 0, 133);
    body.cubicTo(-49, 133, -78, 115, -80, 28);
    body.cubicTo(-82, -65, -65, -115, 0, -115);
    QLinearGradient g(-80, 0, 80, 0);
    g.setColorAt(0, QColor("#252e38"));
    g.setColorAt(.5, QColor("#3a4654"));
    g.setColorAt(1, QColor("#202832"));
    p.setBrush(g);
    p.setPen(QPen(glow, 3));
    p.drawPath(body);
    p.setPen(QPen(QColor("#10151b"), 3));
    p.drawLine(0, -110, 0, -22);
    p.drawLine(-69, -23, 69, -23);
    p.setBrush(QColor("#111820"));
    p.drawRoundedRect(QRectF(-9, -92, 18, 43), 7, 7);
    p.setBrush(chosen == 5 ? light : QColor("#202b35"));
    p.drawRoundedRect(QRectF(-11, -10, 22, 19), 4, 4);
    p.setBrush(chosen == 6 ? light : QColor("#202b35"));
    p.drawRoundedRect(QRectF(-11, 17, 22, 19), 4, 4);
    p.setBrush(QColor("#202b35"));
    p.drawRoundedRect(QRectF(-91, -28, 20, 28), 4, 4);
    p.drawRoundedRect(QRectF(-91, 9, 20, 28), 4, 4);
    QVector<QRectF> regions = {{-65, -98, 45, 62}, {20, -98, 45, 62},
                               {-13, -95, 26, 48}, {-91, -28, 20, 28},
                               {-91, 9, 20, 28},   {-15, -14, 30, 25},
                               {-15, 13, 30, 27}};
    for (int n = 0; n < 7; ++n) {
      QPolygonF polygon(regions[n]);
      QPainterPath path;
      path.addPolygon(polygon);
      hitRegions[n] = p.transform().map(path);
      centers[n] = p.transform().map(regions[n].center());
      p.setPen(chosen == n ? QColor("#fff0f0") : QColor("#d2dce6"));
      p.drawText(regions[n], Qt::AlignCenter, QString::number(n + 1));
    }
  } else {
    struct Face {
      QPolygonF poly;
      float z;
      QColor color;
      int id;
    };
    QVector<Face> faces;
    QMatrix4x4 rotation;
    rotation.rotate(pitch, 1, 0, 0);
    rotation.rotate(yaw, 0, 1, 0);
    double scale = qMin(width() / 2.8, height() / 3.0) * zoom;
    auto project = [&](QVector3D v) {
      v = rotation.map(v);
      float perspective = 4.5 / (4.5 + v.z());
      return QPointF(width() / 2. + v.x() * scale * perspective,
                     height() / 2. - v.y() * scale * perspective);
    };
    auto widthAt = [](double z) {
      return .65 * std::pow(qMax(0., 1 - z * z / 1.21), .35);
    };
    auto surface = [&](double z, double u) {
      double h = .12 + .5 * std::pow(qMax(0., 1 - z * z / 1.4), .7) *
                           std::pow(qMax(0., 1 - u * u), .62);
      return QVector3D(widthAt(z) * u, h, z);
    };
    for (int i = 0; i < 32; ++i)
      for (int j = 0; j < 24; ++j) {
        double z = -1.09 + 2.18 * i / 32, u = -1. + 2. * j / 24;
        QVector<QVector3D> vertices = {surface(z, u), surface(z + 2.18 / 32, u),
                                       surface(z + 2.18 / 32, u + 2. / 24),
                                       surface(z, u + 2. / 24)};
        float depth = 0;
        QPolygonF polygon;
        for (auto v : vertices) {
          depth += rotation.map(v).z();
          polygon << project(v);
        }
        QVector3D norm = QVector3D::normal(vertices[1] - vertices[0],
                                           vertices[2] - vertices[0]);
        float shading = .48 + .45 * qAbs(QVector3D::dotProduct(
                                        rotation.mapVector(norm),
                                        QVector3D(.3, .8, 1).normalized()));
        int id = z < -.25                   ? (u < -.15  ? 0
                                               : u > .15 ? 1
                                                         : 2)
                 : qAbs(u) < .18 && z < .35 ? (z < .05 ? 5 : 6)
                                            : -1;
        QColor color =
            id == chosen
                ? light
                : QColor::fromRgbF(.17 * shading, .22 * shading, .28 * shading);
        faces << Face{polygon, depth / 4, color, id};
      }
    // Model the two side buttons as independent raised plates.
    for (int n = 0; n < 2; ++n) {
      double z = -.08 + n * .38;
      QVector<QVector3D> points = {{-.68f, .12f, float(z - .13)},
                                   {-.68f, .31f, float(z - .13)},
                                   {-.68f, .31f, float(z + .13)},
                                   {-.68f, .12f, float(z + .13)}};
      QPolygonF poly;
      float depth = 0;
      for (auto point : points) {
        poly << project(point);
        depth += rotation.map(point).z();
      }
      faces << Face{poly, depth / 4,
                    chosen == n + 3 ? light : QColor("#526170"), n + 3};
    }
    std::sort(faces.begin(), faces.end(),
              [](auto &a, auto &b) { return a.z > b.z; });
    p.setPen(Qt::NoPen);
    for (auto f : faces) {
      p.setBrush(f.color);
      p.drawPolygon(f.poly);
      pickFaces.append({f.poly, f.id});
      if (f.id >= 0) {
        hitRegions[f.id].addPolygon(f.poly);
        centers[f.id] = hitRegions[f.id].boundingRect().center();
      }
    }
    p.setPen(QPen(glow, 2));
    p.drawLine(project({0, .2f, -1.1f}), project({0, .1f, -1.5f}));
    p.setPen(QColor("#c8d6e5"));
    p.drawText(QRect(8, 8, width() - 16, 30), Qt::AlignCenter,
               "Drag to rotate · scroll to zoom");
  }
  p.resetTransform();
  p.setPen(QColor("#90a1b4"));
  p.drawText(QRect(8, height() - 38, width() - 16, 32), Qt::AlignCenter,
             threeD ? "Generic native 3D model"
                    : "Generic mouse · select a numbered button");
}
void Viewer::mousePressEvent(QMouseEvent *e) {
  last = e->position();
  dragging = e->button() == Qt::LeftButton;
  moved = false;
  QString name = e->button() == Qt::LeftButton     ? "Left"
                 : e->button() == Qt::RightButton  ? "Right"
                 : e->button() == Qt::MiddleButton ? "Middle"
                 : e->button() == Qt::BackButton   ? "Back"
                                                   : "Forward";
  int index = e->button() == Qt::LeftButton      ? 0
              : e->button() == Qt::RightButton   ? 1
              : e->button() == Qt::MiddleButton  ? 2
              : e->button() == Qt::BackButton    ? 3
              : e->button() == Qt::ForwardButton ? 4
                                                 : -1;
  if (testEnabled && index >= 0)
    ++counts[index];
  emit tested(name + " pressed inside the preview" +
              (testEnabled ? " · " + testSummary() : ""));
}
void Viewer::mouseMoveEvent(QMouseEvent *e) {
  if (threeD && dragging) {
    auto delta = e->position() - last;
    if (delta.manhattanLength() > 1)
      moved = true;
    yaw += delta.x() * .7;
    pitch = qBound(-70., pitch + delta.y() * .5, 70.);
    last = e->position();
    update();
  }
}
void Viewer::mouseReleaseEvent(QMouseEvent *e) {
  if (dragging && !moved) {
    int n = buttonAt(e->position());
    if (n >= 0) {
      chosen = n;
      emit selected(n);
      update();
    }
  }
  dragging = false;
}
void Viewer::wheelEvent(QWheelEvent *e) {
  if (threeD) {
    zoom = qBound(.6, zoom + e->angleDelta().y() / 1200., 1.7);
    update();
  }
  if (testEnabled) {
    if (e->angleDelta().y() > 0)
      ++wheelUp;
    else
      ++wheelDown;
  }
  emit tested(QString("Wheel %1 inside the preview")
                  .arg(e->angleDelta().y() > 0 ? "up" : "down") +
              (testEnabled ? " · " + testSummary() : ""));
  e->accept();
}
} // namespace mouse
