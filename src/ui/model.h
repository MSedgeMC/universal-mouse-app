// Original native GM320 model loader and CPU renderer, Copyright 2026 MSedgeMC.
// MIT. The embedded official product logo has separate terms.
#pragma once
#include <QColor>
#include <QImage>
#include <QMatrix4x4>
#include <QPointF>
#include <QSize>
#include <QVector3D>
#include <array>
#include <vector>

namespace mouse {
struct ModelRender {
  QImage image;
  std::vector<qint8> buttons;
  std::array<QPointF, 8> anchors{};
  std::array<bool, 8> visible{};
  int buttonAt(QPointF point) const;
};

class MouseModel {
public:
  MouseModel();
  bool valid() const { return !triangles.empty(); }
  int vertexCount() const { return int(vertices.size()); }
  int triangleCount() const { return int(triangles.size()); }
  ModelRender render(QSize pixels, qreal dpr, double yaw, double pitch,
                     double zoom, int selected, QColor light,
                     bool displayLighting) const;

private:
  struct Vertex {
    QVector3D position, normal;
    QPointF uv;
    QVector3D color;
  };
  struct Material {
    QVector3D color;
    float roughness = .5;
    bool emissive = false, textured = false;
    int button = -1;
  };
  struct Triangle {
    std::array<quint32, 3> indices{};
    int material = 0;
  };
  std::vector<Vertex> vertices;
  std::vector<Material> materials;
  std::vector<Triangle> triangles;
  std::array<QVector3D, 8> anchors{};
  QImage logo;
};
} // namespace mouse
