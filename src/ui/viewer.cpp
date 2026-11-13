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
  for (int n = 0; n < centers.size(); ++n)
    if (QLineF(point, centers[n]).length() <= 11)
      return n;
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
  p.fillRect(rect(), QColor("#15191f"));
  hitRegions.fill({}, 8);
  centers.fill({}, 8);
  pickFaces.clear();
  QColor glow = previewColor();
  auto badge = [&](int n, QPointF center) {
    QPainterPath path;
    path.addEllipse(center, 11, 11);
    hitRegions[n].addPath(path);
    centers[n] = center;
    p.setPen(QPen(n == chosen ? QColor("#b5f0d4") : QColor("#566475"), 1.2));
    p.setBrush(n == chosen ? QColor("#284e3c") : QColor("#202a35"));
    p.drawPath(path);
    p.setPen(n == chosen ? QColor("#d7fae9") : QColor("#b6c2d2"));
    QFont font = p.font();
    font.setPointSizeF(8.5);
    font.setWeight(QFont::Medium);
    p.setFont(font);
    p.drawText(QRectF(center - QPointF(11, 11), QSizeF(22, 22)),
               Qt::AlignCenter, QString::number(n + 1));
  };
  // Original schematic geometry. It represents physical placement, not a vendor
  // asset.
  if (!threeD) {
    double scale = qMin(width() / 330., height() / 375.);
    p.translate(width() / 2., height() / 2. - 5);
    p.scale(scale, scale);
    QPainterPath body;
    body.moveTo(-45, -132);
    body.lineTo(45, -132);
    body.quadTo(72, -126, 78, -100);
    body.lineTo(90, -26);
    body.lineTo(95, 66);
    body.quadTo(91, 119, 54, 137);
    body.quadTo(0, 150, -54, 137);
    body.quadTo(-91, 119, -95, 66);
    body.lineTo(-90, -26);
    body.lineTo(-78, -100);
    body.quadTo(-72, -126, -45, -132);
    body.closeSubpath();
    QPainterPath shadow = body.translated(0, 9);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 85));
    p.drawPath(shadow);
    QLinearGradient shell(-95, -70, 85, 110);
    shell.setColorAt(0, QColor("#4a5664"));
    shell.setColorAt(.36, QColor("#333e4c"));
    shell.setColorAt(1, QColor("#202832"));
    p.setBrush(shell);
    p.setPen(QPen(QColor("#5a6879"), 1.4));
    p.drawPath(body);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(glow.red(), glow.green(), glow.blue(), 105), 5));
    p.drawPath(body);
    p.setPen(QPen(glow, 1.2));
    p.drawPath(body);
    p.setPen(QPen(QColor("#11171f"), 3));
    p.drawLine(-72, -27, 72, -27);
    p.drawLine(0, -126, 0, -27);
    p.setBrush(QColor("#0c1117"));
    p.setPen(QPen(QColor("#657183"), 1));
    p.drawRoundedRect(QRectF(-10, -112, 20, 47), 8, 8);
    p.setPen(QPen(QColor("#435262"), 2));
    for (int n = 0; n < 7; ++n)
      p.drawLine(-6, -106 + n * 5, 6, -106 + n * 5);
    p.setPen(QPen(QColor("#617185"), 1));
    p.setBrush(QColor("#222d38"));
    p.drawRoundedRect(QRectF(-10, -12, 20, 20), 3, 3);
    p.drawRoundedRect(QRectF(-10, 20, 20, 20), 3, 3);
    p.drawRoundedRect(QRectF(-100, -27, 14, 29), 4, 4);
    p.drawRoundedRect(QRectF(-100, 10, 14, 29), 4, 4);
    QPainterPath fire;
    fire.moveTo(-77, -106);
    fire.lineTo(-56, -102);
    fire.lineTo(-56, -67);
    fire.lineTo(-83, -72);
    fire.closeSubpath();
    p.setBrush(QColor("#c35660"));
    p.setPen(QPen(QColor("#dd9096"), 1));
    p.drawPath(fire);
    // Recessed LED side accents and our own center mark.
    p.setPen(QPen(glow, 2));
    p.drawLine(-72, 65, -64, 96);
    p.drawLine(72, 65, 64, 96);
    p.setPen(QPen(QColor("#7e8c9d"), 2));
    p.drawLine(-9, 86, -9, 98);
    p.drawArc(QRectF(-9, 90, 18, 20), 180 * 16, 180 * 16);
    p.drawLine(9, 86, 9, 98);
    QVector<QPointF> physical = {{-35, -69}, {35, -69}, {0, -91}, {-93, -12},
                                 {-93, 24},  {0, -2},   {0, 30},  {-68, -85}};
    QVector<QRectF> regions = {{-51, -114, 30, 72}, {18, -114, 47, 72},
                               {-12, -115, 24, 52}, {-101, -28, 17, 33},
                               {-101, 9, 17, 32},   {-12, -13, 24, 23},
                               {-12, 18, 24, 25},   {-82, -108, 25, 43}};
    for (int n = 0; n < 8; ++n) {
      QPainterPath path;
      path.addRect(regions[n]);
      hitRegions[n] = p.transform().map(path);
    }
    p.resetTransform();
    for (int n = 0; n < 8; ++n)
      badge(n, QPointF(width() / 2. + physical[n].x() * scale,
                       height() / 2. - 5 + physical[n].y() * scale));
  } else {
    struct Face {
      QPolygonF polygon;
      float depth;
      QColor color;
      int button;
    };
    QVector<Face> mesh;
    QMatrix4x4 rotation;
    rotation.rotate(pitch + 15, 1, 0, 0);
    rotation.rotate(yaw, 0, 1, 0);
    double scale = qMin(width() / 3.1, height() / 3.3) * zoom;
    auto project = [&](QVector3D point) {
      point = rotation.map(point);
      double perspective = 5. / (5. + point.z());
      return QPointF(width() / 2. + point.x() * scale * perspective,
                     height() / 2. - point.y() * scale * perspective + 24);
    };
    auto face = [&](QVector<QVector3D> points, QColor color, int id = -1) {
      QPolygonF poly;
      float depth = 0;
      for (auto point : points) {
        poly << project(point);
        depth += rotation.map(point).z();
      }
      mesh.append({poly, depth / points.size(), color, id});
    };
    const QVector<float> z = {-1.25f, -1.1f, -.6f, 0.f, .65f, 1.15f, 1.28f};
    const QVector<float> width = {.40f, .61f, .74f, .81f, .82f, .57f, .30f};
    const QVector<float> height = {.18f, .27f, .43f, .57f, .57f, .32f, .19f};
    auto point = [&](int ring, int step) {
      float theta = 3.14159265f * step / 16;
      return QVector3D(-width[ring] * std::cos(theta),
                       .1f + height[ring] * std::sin(theta), z[ring]);
    };
    for (int i = 0; i < z.size() - 1; ++i)
      for (int j = 0; j < 16; ++j) {
        QVector<QVector3D> points = {point(i, j), point(i + 1, j),
                                     point(i + 1, j + 1), point(i, j + 1)};
        auto normal =
            QVector3D::normal(points[1] - points[0], points[2] - points[0]);
        float shade = .58f + .30f * qAbs(QVector3D::dotProduct(
                                        rotation.mapVector(normal),
                                        QVector3D(-.3, 1, .5).normalized()));
        int id = i < 2 ? (j < 7 ? 0 : j > 9 ? 1 : 2) : -1;
        QColor base = QColor::fromRgbF(.22 * shade, .28 * shade, .35 * shade);
        if (id == chosen)
          base = QColor::fromRgbF(.27 * shade, .40 * shade, .39 * shade);
        face(points, base, id);
      }
    // Closed end caps avoid holes at the nose and palm end.
    for (int ring : {0, int(z.size() - 1)}) {
      QVector<QVector3D> cap;
      for (int j = 0; j <= 16; ++j)
        cap << point(ring, j);
      cap << QVector3D(width[ring], -.1f, z[ring])
          << QVector3D(-width[ring], -.1f, z[ring]);
      face(cap, QColor("#303b47"));
    }
    for (int i = 0; i < z.size() - 1; ++i)
      for (int side : {-1, 1})
        face({{side * width[i], .1f, z[i]},
              {side * width[i + 1], .1f, z[i + 1]},
              {side * width[i + 1], -.10f, z[i + 1]},
              {side * width[i], -.10f, z[i]}},
             QColor("#28313d"));
    auto plate = [&](float x, float y, float z, float w, float length,
                     QColor color, int id) {
      face({{x - w, y, z - length},
            {x + w, y, z - length},
            {x + w, y, z + length},
            {x - w, y, z + length}},
           color, id);
    };
    plate(0, .43f, -.82f, .08f, .22f, QColor("#0d1219"), 2);
    plate(0, .61f, -.17f, .10f, .09f, QColor("#637482"), 5);
    plate(0, .69f, .18f, .10f, .09f, QColor("#637482"), 6);
    plate(-.49f, .35f, -.92f, .10f, .21f, QColor("#c35660"), 7);
    for (int n = 0; n < 2; ++n) {
      float position = -.28f + n * .37f;
      face({{-.83f, .13f, position - .14f},
            {-.83f, .29f, position - .14f},
            {-.83f, .29f, position + .14f},
            {-.83f, .13f, position + .14f}},
           QColor("#566577"), n + 3);
    }
    std::sort(mesh.begin(), mesh.end(),
              [](const Face &a, const Face &b) { return a.depth > b.depth; });
    p.setPen(Qt::NoPen);
    for (const auto &f : mesh) {
      p.setBrush(f.color);
      p.drawPolygon(f.polygon);
      pickFaces.append({f.polygon, f.button});
      if (f.button >= 0)
        hitRegions[f.button].addPolygon(f.polygon);
    }
    p.setPen(QPen(glow, 1.4));
    for (int side : {-1, 1})
      for (int i = 0; i < z.size() - 1; ++i)
        p.drawLine(project({side * width[i], .08f, z[i]}),
                   project({side * width[i + 1], .08f, z[i + 1]}));
    QVector<QVector3D> controls = {{-.29f, .38f, -.76f}, {.29f, .38f, -.76f},
                                   {0, .44f, -.82f},     {-.83f, .23f, -.28f},
                                   {-.83f, .23f, .09f},  {0, .62f, -.17f},
                                   {0, .7f, .18f},       {-.49f, .36f, -.92f}};
    for (int n = 0; n < controls.size(); ++n)
      badge(n, project(controls[n]));
  }
  p.resetTransform();
  p.setPen(QColor("#697789"));
  QFont font = p.font();
  font.setPointSizeF(8);
  p.setFont(font);
  p.drawText(QRect(8, height() - 26, width() - 16, 20), Qt::AlignCenter,
             threeD ? "Drag to orbit · Scroll to zoom"
                    : "8 controls · Fire button shown in red");
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
