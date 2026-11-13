#pragma once
#include <QColor>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QPainterPath>
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
  void resetView();
  void setLighting(const QJsonObject &settings, const QJsonObject &live);
  void setAutoRotate(bool);
  void setTestEnabled(bool);
  int buttonAt(QPointF) const;
  QPointF buttonCenter(int) const;
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
  double yaw = -25, pitch = 25, zoom = 1;
  int chosen = 0;
  QColor light = QColor("#de5364");
  QPointF last;
  QVector<QPainterPath> hitRegions;
  QVector<QPair<QPolygonF, int>> pickFaces;
  QVector<QPointF> centers;
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
};
} // namespace mouse
