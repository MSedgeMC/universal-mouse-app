#include "viewer.h"
#include <QFontMetrics>
#include <QJsonArray>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <QWheelEvent>
#include <QtMath>
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
      yaw = std::fmod(yaw + .7, 360.);
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
  yaw = 24;
  pitch = 60;
  zoom = 1;
  update();
}
void Viewer::setViewAngles(double horizontal, double vertical) {
  yaw = horizontal;
  pitch = qBound(-75., vertical, 90.);
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
  if (calloutsVisible)
    for (int n = 0; n < calloutRegions.size(); ++n)
      if (calloutRegions[n].contains(point))
        return n;
  if (threeD) {
    return modelCache.buttonAt(point);
  } else {
    for (int n = 0; n < centers.size(); ++n)
      if (QLineF(point, centers[n]).length() <= 8)
        return n;
    for (int n = 0; n < hitRegions.size(); ++n)
      if (hitRegions[n].contains(point))
        return n;
  }
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
    pitch = qMin(90., pitch + 6);
  else if (e->key() == Qt::Key_Down)
    pitch = qMax(-75., pitch - 6);
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
  // Load the immutable asset once, only when a 3D view is requested. The
  // ordinary photo and lighting pages do not duplicate a mesh in memory.
  static const MouseModel model;
  if (!model.valid()) {
    p.setPen(QColor("#aeb8c8"));
    p.drawText(rect(), Qt::AlignCenter, "GM320 model could not be loaded");
    return;
  }
  // Cache a supersampled frame so diagonal seams, diffusers and the printed
  // logo remain smooth without continuous sampling on an idle page.
  qreal dpr = qBound(1.0, devicePixelRatioF() * 2.0, 2.0);
  QSize pixels(qCeil(width() * dpr), qCeil(height() * dpr));
  if (modelCache.image.isNull() || modelSize != pixels || modelDpr != dpr ||
      modelYaw != yaw || modelPitch != pitch || modelZoom != zoom ||
      modelSelected != chosen || modelLight != glow ||
      modelLighting != displayLighting) {
    modelCache = model.render(pixels, dpr, yaw, pitch, zoom, chosen, glow,
                              displayLighting);
    modelSize = pixels;
    modelDpr = dpr;
    modelYaw = yaw;
    modelPitch = pitch;
    modelZoom = zoom;
    modelSelected = chosen;
    modelLight = glow;
    modelLighting = displayLighting;
  }
  p.drawImage(QPointF(0, 0), modelCache.image);
  for (int n = 0; n < 8; ++n)
    centers[n] = modelCache.anchors[n];
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
      bool hidden = threeD && !modelCache.visible[n];
      p.setPen(QPen(QColor(active ? "#159bff"
                           : hot  ? "#909baa"
                                  : "#535860"),
                    active ? 1.25 : .8, hidden ? Qt::DashLine : Qt::SolidLine));
      p.setBrush(Qt::NoBrush);
      QPainterPath line;
      line.moveTo(start);
      line.lineTo(elbow);
      line.lineTo(target);
      p.drawPath(line);
      p.setBrush(QColor(active ? "#159bff" : hidden ? "#535b69" : "#8b919b"));
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
    pitch = qBound(-75., pitch + delta.y() * .5, 90.);
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
