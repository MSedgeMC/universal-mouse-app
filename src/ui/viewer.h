#pragma once
#include "model.h"
#include <QColor>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QPainterPath>
#include <QPixmap>
#include <QPointF>
#include <QTimer>
#include <QWidget>
namespace mouse {
class Viewer : public QWidget {
  Q_OBJECT
public:
  explicit Viewer(QWidget *parent = nullptr);
  void setThreeD(bool);
  void setColor(QColor);
  void setSelected(int);
  void setAssignments(const QStringList &);
  void setCalloutsVisible(bool);
  void setDisplayLighting(bool);
  void resetView();
  void setViewAngles(double yaw, double pitch);
  void setLighting(const QJsonObject &settings, const QJsonObject &live);
  void setAutoRotate(bool);
  void setTestEnabled(bool);
  int buttonAt(QPointF) const;
  QPointF buttonCenter(int) const;
  QPointF calloutCenter(int) const;
signals:
  void selected(int);
  void tested(QString);

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void wheelEvent(QWheelEvent *) override;
  void keyPressEvent(QKeyEvent *) override;
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

private:
  bool threeD = false, dragging = false, moved = false;
  double yaw = 24, pitch = 60, zoom = 1;
  int chosen = 0;
  QColor light = QColor("#a9e9cc");
  QPointF last;
  QVector<QPainterPath> hitRegions;
  ModelRender modelCache;
  QSize modelSize;
  qreal modelDpr = 0;
  double modelYaw = 0, modelPitch = 0, modelZoom = 0;
  int modelSelected = -2;
  QColor modelLight;
  bool modelLighting = false;
  QVector<QPointF> centers;
  QVector<QRectF> calloutRegions;
  QStringList assignmentLabels;
  bool calloutsVisible = true, displayLighting = false;
  int hovered = -1;
  QPixmap artwork, lightingArtwork, lightingMask, lightingCache;
  QColor lightingCacheColor;
  QPixmap artworkCache;
  QSize artworkSize;
  qreal artworkDpr = 0;
  QTimer animation;
  QElapsedTimer elapsed;
  QString lightEffect = "Light On";
  QVector<QColor> palette;
  double luminosity = 1;
  int lightSpeed = 3;
  bool autoRotate = false, testEnabled = false;
  QVector<int> counts = QVector<int>(5, 0);
  int wheelUp = 0, wheelDown = 0;
  void syncAnimation();
  QColor previewColor() const;
  QString testSummary() const;
  void drawTop(QPainter &, QColor);
  void drawThreeD(QPainter &, QColor);
  void drawCallouts(QPainter &);
};
} // namespace mouse
