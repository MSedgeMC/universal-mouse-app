#include "viewer.h"
#include <QFontMetrics>
#include <QJsonArray>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <QVector3D>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>
namespace mouse {
Viewer::Viewer(QWidget *p)
    : QWidget(p), artwork(":/design/gm320-top.png"),
      lightingArtwork(":/design/gm320-neutral.png"),
      lightingMask(":/design/gm320-light-mask.png") {
  setMinimumSize(310, 330);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setAccessibleName("Mouse preview and input tester");
  assignmentLabels = {"Left click",  "Right click", "Middle click",
                      "Forward",     "Back",        "DPI Loop",
                      "Color cycle", "Fire button"};
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
  chosen = qBound(0, i, 7);
  update();
}
void Viewer::setAssignments(const QStringList &labels) {
  if (labels.size() == 8 && assignmentLabels != labels) {
    assignmentLabels = labels;
    update();
  }
}
void Viewer::setCalloutsVisible(bool enabled) {
  calloutsVisible = enabled;
  update();
}
void Viewer::setDisplayLighting(bool enabled) {
  displayLighting = enabled;
  artworkCache = {};
  syncAnimation();
  update();
}
QPointF Viewer::calloutCenter(int n) const {
  return calloutRegions.value(n).center();
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
  bool lightingMoving =
      displayLighting && lightEffect != "Off" &&
      (lightEffect.contains("Breath", Qt::CaseInsensitive) ||
       lightEffect.contains("Cycle") || lightEffect.contains("Neon") ||
       lightEffect.contains("Water") || lightEffect.contains("Wave"));
  bool moving = (threeD && autoRotate) || lightingMoving;
  if (isVisible() && moving)
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
    if (QLineF(point, centers[n]).length() <= 8)
      return n;
  if (calloutsVisible)
    for (int n = 0; n < calloutRegions.size(); ++n)
      if (calloutRegions[n].contains(point))
        return n;
  if (threeD) {
    for (auto it = pickFaces.crbegin(); it != pickFaces.crend(); ++it)
      if (it->second >= 0 && it->first.containsPoint(point, Qt::OddEvenFill))
        return it->second;
  } else
    for (int n = 0; n < hitRegions.size(); ++n)
      if (hitRegions[n].contains(point))
        return n;
  return -1;
}
QPointF Viewer::buttonCenter(int n) const { return centers.value(n); }
void Viewer::keyPressEvent(QKeyEvent *e) {
  if (e->key() >= Qt::Key_1 && e->key() <= Qt::Key_8) {
    chosen = e->key() - Qt::Key_1;
    emit selected(chosen);
  } else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Space) {
    emit selected(chosen);
  } else if (e->key() == Qt::Key_Home)
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
  p.fillRect(rect(), QColor("#202124"));
  if (calloutsVisible) {
    p.setPen(Qt::NoPen);
    for (int y = 20; y < height() - 15; y += 24) {
      double fade = qMax(0., 1. - qAbs(y - height() * .44) / (height() * .55));
      for (int x = 18; x < width() - 15; x += 24) {
        double f =
            fade * qMax(.08, 1. - qAbs(x - width() * .5) / (width() * .53));
        p.setBrush(QColor(160, 169, 188, int(15 * f)));
        p.drawEllipse(QPointF(x, y), .85, .85);
      }
    }
  }
  hitRegions.fill({}, 8);
  calloutRegions.fill({}, 8);
  centers.fill({}, 8);
  pickFaces.clear();
  QColor glow = displayLighting ? previewColor() : QColor("#159bff");
  if (threeD)
    drawThreeD(p, glow);
  else
    drawTop(p, glow);
  if (calloutsVisible)
    drawCallouts(p);
  if (hasFocus()) {
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(21, 155, 255, 105), 1));
    p.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 4, 4);
  }
}
void Viewer::drawTop(QPainter &p, QColor glow) {
  double h = qMin(height() * .96, 640.);
  if (calloutsVisible)
    h = qMin(h, qMax(300., (width() - 220) * 1.45));
  double w = h * 517. / 793.;
  QRectF area((width() - w) / 2., (height() - h) / 2., w, h);
  double scale = h / 793.;
  p.save();
  p.translate(area.center().x(), area.bottom() - 24 * scale);
  p.scale(1, .30);
  QRadialGradient shadow(0, 0, area.width() * .51);
  shadow.setColorAt(0, QColor(0, 0, 0, 180));
  shadow.setColorAt(.5, QColor(0, 0, 0, 90));
  shadow.setColorAt(1, QColor(0, 0, 0, 0));
  p.setBrush(shadow);
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(0, 0), area.width() * .55, area.width() * .55);
  p.restore();
  QSize pixels(qCeil(w), qCeil(h));
  qreal dpr = devicePixelRatioF();
  if (artworkCache.isNull() || artworkSize != pixels || artworkDpr != dpr) {
    artworkSize = pixels;
    artworkDpr = dpr;
    artworkCache =
        QPixmap(qCeil(pixels.width() * dpr), qCeil(pixels.height() * dpr));
    artworkCache.setDevicePixelRatio(dpr);
    artworkCache.fill(Qt::transparent);
    QPainter cache(&artworkCache);
    cache.setRenderHint(QPainter::SmoothPixmapTransform);
    const auto &source = displayLighting ? lightingArtwork : artwork;
    cache.drawPixmap(QRectF(QPointF(0, 0), QSizeF(pixels)), source,
                     QRectF(source.rect()));
    lightingCache = {};
  }
  p.drawPixmap(area, artworkCache,
               QRectF(0, 0, artworkCache.width(), artworkCache.height()));
  if (displayLighting && lightEffect != "Off") {
    if (lightingCache.isNull() || lightingCacheColor != glow) {
      lightingCache = QPixmap(artworkCache.size());
      lightingCache.setDevicePixelRatio(dpr);
      lightingCache.fill(Qt::transparent);
      QPainter layer(&lightingCache);
      layer.setRenderHint(QPainter::SmoothPixmapTransform);
      layer.drawPixmap(QRectF(QPointF(0, 0), QSizeF(pixels)), lightingMask,
                       QRectF(lightingMask.rect()));
      layer.setCompositionMode(QPainter::CompositionMode_SourceIn);
      layer.fillRect(QRectF(QPointF(0, 0), QSizeF(pixels)), glow);
      lightingCacheColor = glow;
    }
    p.drawPixmap(area, lightingCache,
                 QRectF(0, 0, lightingCache.width(), lightingCache.height()));
  }
  QTransform transform;
  transform.translate(area.left(), area.top());
  transform.scale(scale, scale);
  const QVector<QPointF> physical = {{190, 162}, {380, 158}, {285, 173},
                                     {118, 385}, {121, 470}, {286, 311},
                                     {286, 362}, {104, 290}};
  const QVector<QRectF> regions = {{143, 55, 110, 185}, {332, 62, 100, 178},
                                   {265, 106, 42, 143}, {107, 345, 29, 78},
                                   {108, 431, 32, 83},  {267, 286, 39, 45},
                                   {267, 338, 39, 45},  {90, 261, 29, 72}};
  for (int n = 0; n < 8; ++n) {
    centers[n] = transform.map(physical[n]);
    QPainterPath r;
    r.addRoundedRect(regions[n], 7, 7);
    hitRegions[n] = transform.map(r);
  }
}
void Viewer::drawThreeD(QPainter &p, QColor glow) {
  struct Face {
    QPolygonF polygon;
    float depth;
    QColor color;
    int button;
  };
  QVector<Face> mesh;
  QMatrix4x4 rotation;
  rotation.rotate(pitch + 28, 1, 0, 0);
  rotation.rotate(yaw, 0, 1, 0);
  double scale = qMin(width() / 4.4, height() / 3.7) * zoom;
  auto project = [&](QVector3D point) {
    point = rotation.map(point);
    double perspective = 5. / (5. - point.z());
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
  // A closed, interpolated original shell avoids flat end caps and holes.
  const QVector<float> lengths = {-1.39f, -1.30f, -1.10f, -.60f, 0.f,
                                  .65f,   1.05f,  1.29f,  1.38f};
  const QVector<float> widths = {.22f, .44f, .61f, .74f, .81f,
                                 .82f, .66f, .35f, .025f};
  const QVector<float> heights = {.10f, .20f, .27f, .43f, .57f,
                                  .57f, .41f, .20f, .025f};
  auto shape = [&](float z) {
    int n = 0;
    while (n < lengths.size() - 2 && z > lengths[n + 1])
      ++n;
    float t =
        qBound(0.f, (z - lengths[n]) / (lengths[n + 1] - lengths[n]), 1.f);
    auto interpolate = [&](const QVector<float> &values) {
      float a = values[qMax(0, n - 1)], b = values[n], c = values[n + 1],
            d = values[qMin(n + 2, int(values.size() - 1))];
      return .5f *
             ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t +
              (-a + 3 * b - 3 * c + d) * t * t * t);
    };
    return qMakePair(qMax(.02f, interpolate(widths)),
                     qMax(.02f, interpolate(heights)));
  };
  auto topY = [&](float x, float z) {
    auto s = shape(z);
    float ratio = qBound(-1.f, x / s.first, 1.f);
    return .09f + s.second * std::sqrt(qMax(0.f, 1 - ratio * ratio));
  };
  auto shade = [&](const QVector<QVector3D> &points, QColor base) {
    auto normal = rotation.mapVector(
        QVector3D::normal(points[1] - points[0], points[2] - points[0]));
    auto key = QVector3D(-.45f, .65f, .60f).normalized();
    float diffuse = qMax(0.f, QVector3D::dotProduct(normal, key));
    float sheen =
        std::pow(qMax(0.f, QVector3D::dotProduct(
                               normal, QVector3D(-.25f, .3f, 1).normalized())),
                 18);
    float level = .46f + .65f * diffuse;
    return QColor::fromRgbF(qMin(1., base.redF() * level + .07 * sheen),
                            qMin(1., base.greenF() * level + .07 * sheen),
                            qMin(1., base.blueF() * level + .07 * sheen));
  };
  constexpr int rings = 48, segments = 64;
  auto shellPoint = [&](int ring, int segment) {
    float z =
        lengths.front() + (lengths.back() - lengths.front()) * ring / rings;
    auto s = shape(z);
    float theta = 6.2831853f * segment / segments, sine = std::sin(theta);
    return QVector3D(-s.first * std::cos(theta),
                     .09f + (sine >= 0 ? s.second : .18f) * sine, z);
  };
  QPointF floor = project({0, -.10f, .30f}) + QPointF(0, 23);
  p.save();
  p.translate(floor);
  p.scale(1, .36);
  QRadialGradient shadow(0, 0, scale * 1.30);
  shadow.setColorAt(0, QColor(0, 0, 0, 185));
  shadow.setColorAt(.6, QColor(0, 0, 0, 75));
  shadow.setColorAt(1, QColor(0, 0, 0, 0));
  p.setBrush(shadow);
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(0, 0), scale * 1.30, scale * 1.30);
  p.restore();
  for (int i = 0; i < rings; ++i)
    for (int j = 0; j < segments; ++j) {
      QVector<QVector3D> points = {shellPoint(i, j), shellPoint(i + 1, j),
                                   shellPoint(i + 1, j + 1),
                                   shellPoint(i, j + 1)};
      auto center = (points[0] + points[2]) / 2;
      bool top = j < segments / 2;
      bool front = top && center.z() < -.25f;
      int id = front && center.x() < -.07f  ? 0
               : front && center.x() > .07f ? 1
                                            : -1;
      bool seam = top && ((front && qAbs(center.x()) < .025f) ||
                          qAbs(center.z() + .25f) < .020f);
      QColor material(seam ? "#090a0c" : top ? "#34373c" : "#17191d");
      if (id == chosen)
        material = QColor("#364757");
      face(points, shade(points, material), id);
    }
  for (int end : {0, rings}) {
    QVector<QVector3D> cap;
    for (int j = 0; j < segments; ++j)
      cap << shellPoint(end, j);
    face(cap, QColor("#17191d"));
  }
  auto roundedOutline = [](float a, float b, float radius) {
    QVector<QPointF> outline;
    for (int corner = 0; corner < 4; ++corner) {
      float cx = (corner == 0 || corner == 3) ? a - radius : -a + radius,
            cy = corner < 2 ? b - radius : -b + radius;
      for (int step = 0; step <= 5; ++step) {
        float angle = (corner * 90 + step * 18) * 3.14159265f / 180;
        outline << QPointF(cx + radius * std::cos(angle),
                           cy + radius * std::sin(angle));
      }
    }
    return outline;
  };
  auto topControl = [&](float x, float z, float halfWidth, float halfLength,
                        int id) {
    auto outline = roundedOutline(halfWidth, halfLength,
                                  qMin(halfWidth, halfLength) * .55f);
    QVector3D center(x, topY(x, z) + .027f, z);
    for (int n = 0; n < outline.size(); ++n) {
      auto a = outline[n], b = outline[(n + 1) % outline.size()];
      auto point = [&](QPointF offset, float lift) {
        float px = x + offset.x(), pz = z + offset.y();
        return QVector3D(px, topY(px, pz) + lift, pz);
      };
      auto pa = point(a, .024f), pb = point(b, .024f);
      QVector<QVector3D> top = {center, pb, pa};
      face(top, shade(top, QColor(id == chosen ? "#3a586e" : "#353941")), id);
      face({pa, pb, point(b, .004f), point(a, .004f)}, QColor("#090b0e"), id);
    }
  };
  topControl(0, -.84f, .13f, .25f, 2);
  topControl(0, -.12f, .095f, .105f, 5);
  topControl(0, .23f, .095f, .105f, 6);
  topControl(-.43f, -.95f, .065f, .16f, 7);
  float wheelZ = -.84f, wheelY = topY(0, wheelZ) - .015f;
  constexpr int wheelSegments = 48;
  auto wheelPoint = [&](int segment, float x) {
    float angle = 6.2831853f * segment / wheelSegments;
    return QVector3D(x, wheelY + .14f * std::sin(angle),
                     wheelZ + .14f * std::cos(angle));
  };
  for (int n = 0; n < wheelSegments; ++n) {
    QVector<QVector3D> tread = {wheelPoint(n, -.07f), wheelPoint(n, .07f),
                                wheelPoint(n + 1, .07f),
                                wheelPoint(n + 1, -.07f)};
    face(tread, shade(tread, QColor(n % 3 == 0 ? "#777d86" : "#292d34")), 2);
  }
  for (float side : {-.07f, .07f}) {
    QVector<QVector3D> cap;
    for (int n = 0; n < wheelSegments; ++n)
      cap << wheelPoint(n, side);
    face(cap, QColor("#15171b"), 2);
  }
  auto sideX = [&](float y, float z) {
    auto s = shape(z);
    float ratio = qBound(-1.f, (y - .09f) / s.second, 1.f);
    return -s.first * std::sqrt(qMax(0.f, 1 - ratio * ratio)) - .026f;
  };
  for (int id : {3, 4}) {
    float z = id == 3 ? -.35f : .10f;
    auto outline = roundedOutline(.16f, .062f, .04f);
    QVector3D center(sideX(.22f, z), .22f, z);
    for (int n = 0; n < outline.size(); ++n) {
      auto point = [&](QPointF offset) {
        float py = .22f + offset.y(), pz = z + offset.x();
        return QVector3D(sideX(py, pz), py, pz);
      };
      QVector<QVector3D> surface = {center, point(outline[n]),
                                    point(outline[(n + 1) % outline.size()])};
      face(surface,
           shade(surface, QColor(id == chosen ? "#426780" : "#555b65")), id);
    }
  }
  std::sort(mesh.begin(), mesh.end(),
            [](const Face &a, const Face &b) { return a.depth < b.depth; });
  p.setPen(Qt::NoPen);
  for (const auto &f : mesh) {
    p.setBrush(f.color);
    p.setPen(QPen(f.color, .5));
    p.drawPolygon(f.polygon);
    pickFaces.append({f.polygon, f.button});
    if (f.button >= 0)
      hitRegions[f.button].addPolygon(f.polygon);
  }
  QColor rim = glow;
  rim.setAlpha(150);
  p.setPen(QPen(rim, 1.1));
  for (int side : {-1, 1})
    for (int i = 0; i < rings; ++i) {
      float a = lengths.front() +
                (lengths.back() - lengths.front()) * i / rings,
            b = lengths.front() +
                (lengths.back() - lengths.front()) * (i + 1) / rings;
      p.drawLine(project({side * shape(a).first, .075f, a}),
                 project({side * shape(b).first, .075f, b}));
    }
  QVector<QVector3D> controls = {{-.30f, topY(-.30f, -.76f), -.76f},
                                 {.30f, topY(.30f, -.76f), -.76f},
                                 {0, wheelY + .14f, wheelZ},
                                 {sideX(.22f, -.35f), .22f, -.35f},
                                 {sideX(.22f, .10f), .22f, .10f},
                                 {0, topY(0, -.12f) + .027f, -.12f},
                                 {0, topY(0, .23f) + .027f, .23f},
                                 {-.43f, topY(-.43f, -.95f) + .027f, -.95f}};
  for (int n = 0; n < controls.size(); ++n)
    centers[n] = project(controls[n]);
}
void Viewer::drawCallouts(QPainter &p) {
  const QVector<int> left = {0, 7, 3, 4}, right = {1, 2, 5, 6};
  QStringList physicalNames = {"Left button",    "Right button", "Scroll click",
                               "Forward button", "Back button",  "Top button 1",
                               "Top button 2",   "Fire button"};
  double column = qBound(108., width() * .23, 240.);
  double labelTop = qMax(22., height() * .20),
         step = qBound(53., height() * .14, 85.);
  for (int side = 0; side < 2; ++side) {
    auto order = side == 0 ? left : right;
    for (int row = 0; row < order.size(); ++row) {
      int n = order[row];
      QRectF r(side == 0 ? width() * .035 : width() - width() * .035 - column,
               labelTop + row * step, column, 45);
      calloutRegions[n] = r;
      double y = r.top() + 29;
      QPointF start(side == 0 ? r.right() : r.left(), y);
      QPointF target = centers[n];
      QPointF elbow(side == 0 ? target.x() - 36 : target.x() + 36, y);
      bool active = n == chosen, hot = n == hovered;
      p.setPen(QPen(QColor(active ? "#159bff"
                           : hot  ? "#909baa"
                                  : "#535860"),
                    active ? 1.25 : .8));
      p.setBrush(Qt::NoBrush);
      QPainterPath line;
      line.moveTo(start);
      line.lineTo(elbow);
      line.lineTo(target);
      p.drawPath(line);
      p.setBrush(QColor(active ? "#159bff" : "#8b919b"));
      p.setPen(Qt::NoPen);
      p.drawEllipse(start, 2, 2);
      p.drawEllipse(target, active ? 4 : 3, active ? 4 : 3);
      if (active) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(21, 155, 255, 90), 1));
        p.drawEllipse(target, 8, 8);
      }
      QFont font = p.font();
      font.setPixelSize(13);
      font.setWeight(QFont::Medium);
      p.setFont(font);
      p.setPen(QColor(active ? "#f4f7fc" : hot ? "#e6ebf3" : "#b9bec8"));
      auto label = assignmentLabels.value(n, physicalNames[n]);
      if (label == "Middle click")
        label = "Scroll click";
      QFontMetrics metrics(font);
      label = metrics.elidedText(label, Qt::ElideRight, int(r.width() - 17));
      auto flags =
          (side == 0 ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
      p.drawText(r.adjusted(side == 0 ? 0 : 14, -4, side == 0 ? -14 : 0, -20),
                 flags, label);
      font.setPixelSize(10);
      p.setFont(font);
      p.setPen(QColor(n == 7 ? "#7d8b9e" : "#747c89"));
      p.drawText(r.adjusted(side == 0 ? 0 : 14, 19, side == 0 ? -14 : 0, 1),
                 flags, n == 7 ? "Remapping unavailable" : physicalNames[n]);
    }
  }
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
  if (!dragging) {
    int n = buttonAt(e->position());
    if (hovered != n) {
      hovered = n;
      setCursor(n >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
      update();
    }
  }
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
