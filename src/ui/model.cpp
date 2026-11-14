#include "model.h"
#include "core/protocol.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace mouse {
namespace {
quint32 word(const QByteArray &data, qsizetype offset) {
  require(offset >= 0 && offset + 4 <= data.size(), "Invalid GLB offset");
  return qFromLittleEndian<quint32>(data.constData() + offset);
}
float real(const QByteArray &data, qsizetype offset) {
  float value = std::bit_cast<float>(word(data, offset));
  require(std::isfinite(value), "Nonfinite model vertex");
  return value;
}
QVector3D vector(const QJsonArray &a) {
  require(a.size() >= 3, "Invalid model vector");
  return {float(a[0].toDouble()), float(a[1].toDouble()),
          float(a[2].toDouble())};
}
float edge(QPointF a, QPointF b, QPointF c) {
  return float((c.x() - a.x()) * (b.y() - a.y()) -
               (c.y() - a.y()) * (b.x() - a.x()));
}
QVector3D multiply(QVector3D a, QVector3D b) {
  return {a.x() * b.x(), a.y() * b.y(), a.z() * b.z()};
}
QRgb pixel(QVector3D c) {
  return qRgb(qBound(0, qRound(c.x() * 255), 255),
              qBound(0, qRound(c.y() * 255), 255),
              qBound(0, qRound(c.z() * 255), 255));
}
} // namespace

int ModelRender::buttonAt(QPointF point) const {
  if (image.isNull())
    return -1;
  int x = int(std::floor(point.x() * image.devicePixelRatio())),
      y = int(std::floor(point.y() * image.devicePixelRatio()));
  if (x < 0 || y < 0 || x >= image.width() || y >= image.height())
    return -1;
  return buttons[size_t(y) * image.width() + x];
}

MouseModel::MouseModel() {
  try {
    QFile file(":/design/gm320-model.glb");
    require(file.open(QIODevice::ReadOnly), "Missing GM320 model");
    QByteArray data = file.readAll();
    require(data.size() < 8 * 1024 * 1024 && word(data, 0) == 0x46546c67 &&
                word(data, 4) == 2 && word(data, 8) == quint32(data.size()),
            "Invalid GLB header");
    auto jsonSize = word(data, 12);
    require(word(data, 16) == 0x4e4f534a &&
                jsonSize <= quint32(data.size() - 20),
            "Invalid GLB JSON chunk");
    auto doc = QJsonDocument::fromJson(data.mid(20, jsonSize));
    require(doc.isObject(), "Invalid GLB JSON");
    auto root = doc.object();
    qsizetype binHeader = 20 + jsonSize;
    require(word(data, binHeader + 4) == 0x004e4942,
            "Invalid GLB binary chunk");
    auto binSize = word(data, binHeader);
    require(binSize <= quint32(data.size() - binHeader - 8),
            "Invalid GLB binary size");
    QByteArray bin = data.mid(binHeader + 8, binSize);
    auto views = root["bufferViews"].toArray();
    auto accessors = root["accessors"].toArray();
    auto accessor = [&](int i, int component, const QString &type) {
      require(i >= 0 && i < accessors.size(), "Invalid GLB accessor");
      auto a = accessors[i].toObject();
      require(a["componentType"].toInt() == component && a["type"] == type &&
                  !a.contains("sparse") && !a["normalized"].toBool(),
              "Unsupported GLB accessor");
      int view = a["bufferView"].toInt(-1), count = a["count"].toInt();
      require(view >= 0 && view < views.size() && count > 0 && count <= 100000,
              "Invalid GLB buffer view");
      auto v = views[view].toObject();
      int components = type == "VEC4"   ? 4
                       : type == "VEC3" ? 3
                       : type == "VEC2" ? 2
                                        : 1;
      int stride = v["byteStride"].toInt(components * 4),
          viewOffset = v["byteOffset"].toInt(),
          offset = viewOffset + a["byteOffset"].toInt(),
          length = v["byteLength"].toInt();
      require(v["buffer"].toInt() == 0 && stride >= components * 4 &&
                  stride <= 256 && offset >= viewOffset && viewOffset >= 0 &&
                  qint64(offset) + qint64(count - 1) * stride +
                          components * 4 <=
                      qint64(viewOffset) + length &&
                  qint64(viewOffset) + length <= bin.size(),
              "Invalid GLB accessor bounds");
      return std::array<int, 3>{offset, stride, count};
    };
    auto meshes = root["meshes"].toArray();
    require(meshes.size() == 1, "Invalid GM320 mesh count");
    auto mesh = meshes[0].toObject();
    auto primitives = mesh["primitives"].toArray();
    require(!primitives.isEmpty(), "Empty GM320 mesh");
    auto attrs = primitives[0].toObject()["attributes"].toObject();
    auto pos = accessor(attrs["POSITION"].toInt(-1), 5126, "VEC3"),
         norm = accessor(attrs["NORMAL"].toInt(-1), 5126, "VEC3"),
         uv = accessor(attrs["TEXCOORD_0"].toInt(-1), 5126, "VEC2"),
         col = accessor(attrs["COLOR_0"].toInt(-1), 5126, "VEC4");
    require(pos[2] == norm[2] && pos[2] == uv[2] && pos[2] == col[2],
            "Inconsistent GM320 vertices");
    auto readVector = [&](auto a, int n) {
      qsizetype at = a[0] + n * a[1];
      return QVector3D(real(bin, at), real(bin, at + 4), real(bin, at + 8));
    };
    vertices.reserve(pos[2]);
    for (int n = 0; n < pos[2]; ++n) {
      qsizetype at = uv[0] + n * uv[1];
      auto position = readVector(pos, n);
      require(position.length() < 4, "Invalid GM320 extent");
      vertices.push_back({position, readVector(norm, n).normalized(),
                          QPointF(real(bin, at), real(bin, at + 4)),
                          readVector(col, n)});
    }
    for (auto value : root["materials"].toArray()) {
      auto m = value.toObject(), pbr = m["pbrMetallicRoughness"].toObject();
      materials.push_back({vector(pbr["baseColorFactor"].toArray()),
                           float(pbr["roughnessFactor"].toDouble(.5)),
                           vector(m["emissiveFactor"].toArray()).length() > 0,
                           pbr.contains("baseColorTexture"),
                           m["extras"].toObject()["button"].toInt(-1)});
      require(materials.back().button >= -1 && materials.back().button < 8,
              "Invalid GM320 button ID");
    }
    for (auto value : primitives) {
      auto p = value.toObject();
      require(p["mode"].toInt(4) == 4 && p["attributes"].toObject() == attrs,
              "Unsupported GM320 primitive");
      auto indices = accessor(p["indices"].toInt(-1), 5125, "SCALAR");
      int m = p["material"].toInt(-1);
      require(indices[2] % 3 == 0 && m >= 0 && m < int(materials.size()),
              "Invalid GM320 primitive");
      for (int n = 0; n < indices[2]; n += 3) {
        Triangle t;
        t.material = m;
        for (int j = 0; j < 3; ++j) {
          t.indices[j] = word(bin, indices[0] + (n + j) * indices[1]);
          require(t.indices[j] < vertices.size(), "Invalid GM320 index");
        }
        triangles.push_back(t);
      }
    }
    auto points = mesh["extras"].toObject()["buttonAnchors"].toArray();
    require(points.size() == 8, "Missing GM320 control anchors");
    for (int n = 0; n < 8; ++n)
      anchors[n] = vector(points[n].toArray());
    auto images = root["images"].toArray();
    require(images.size() == 1, "Invalid GM320 image count");
    auto image = images[0].toObject();
    int view = image["bufferView"].toInt(-1);
    require(view >= 0 && view < views.size() &&
                image["mimeType"] == "image/png",
            "Invalid GM320 logo image");
    auto v = views[view].toObject();
    int start = v["byteOffset"].toInt(), length = v["byteLength"].toInt();
    require(start >= 0 && length > 0 && qint64(start) + length <= bin.size(),
            "Invalid GM320 logo bounds");
    logo = QImage::fromData(bin.mid(start, length))
               .convertToFormat(QImage::Format_ARGB32);
    require(!logo.isNull(), "Cannot decode GM320 logo");
  } catch (const std::exception &e) {
    qWarning("Cannot load bundled GM320 model: %s", e.what());
    vertices.clear();
    materials.clear();
    triangles.clear();
  }
}

ModelRender MouseModel::render(QSize pixels, qreal dpr, double yaw,
                               double pitch, double zoom, int selected,
                               QColor light, bool displayLighting) const {
  ModelRender result;
  if (!valid() || pixels.isEmpty())
    return result;
  const int width = pixels.width(), height = pixels.height();
  const size_t count = size_t(width) * height;
  result.image = QImage(pixels, QImage::Format_RGB32);
  result.image.setDevicePixelRatio(dpr);
  result.image.fill(QColor("#202124"));
  result.buttons.assign(count, -1);
  std::vector<float> depths(count, std::numeric_limits<float>::infinity());
  QMatrix4x4 rotation;
  rotation.rotate(float(pitch), 1, 0, 0);
  rotation.rotate(float(yaw), 0, 1, 0);
  float scale = float(qMin(width / 4.0, height / 3.25) * zoom);
  constexpr float distance = 6;
  QVector3D key = QVector3D(-.55, .80, .70).normalized(),
            fill = QVector3D(.75, .05, .5).normalized(),
            half = (key + QVector3D(0, 0, 1)).normalized();
  struct Projected {
    QPointF point, uv;
    QVector3D color;
    float inverseDepth;
  };
  auto project = [&](QVector3D original) {
    auto p = rotation.map(original - QVector3D(0, .40, 0));
    float inv = 1 / (distance - p.z());
    return Projected{QPointF(width / 2. + p.x() * distance * inv * scale,
                             height * .51 - p.y() * distance * inv * scale),
                     {},
                     {},
                     inv};
  };
  std::vector<Projected> projected;
  projected.reserve(vertices.size());
  struct Shade {
    float diffuse, specular, satin;
  };
  std::vector<Shade> shades;
  shades.reserve(vertices.size());
  for (auto &v : vertices) {
    auto p = project(v.position);
    p.uv = v.uv;
    p.color = v.color;
    projected.push_back(p);
    auto normal = rotation.mapVector(v.normal).normalized();
    float diffuse = qMax(0.f, QVector3D::dotProduct(normal, key)),
          secondary = qMax(0.f, QVector3D::dotProduct(normal, fill)),
          angle = qMax(0.f, QVector3D::dotProduct(normal, half));
    shades.push_back({.42f + .82f * diffuse + .12f * secondary,
                      std::pow(angle, 36.f), std::pow(angle, 5.f)});
  }
  // Perspective-correct interpolation and a depth/ID buffer keep picking
  // accurate across overlaps and hidden faces. No OpenGL context is needed.
  for (auto &t : triangles) {
    auto &a = projected[t.indices[0]], &b = projected[t.indices[1]],
         &c = projected[t.indices[2]];
    float area = edge(a.point, b.point, c.point);
    if (area <= .001f)
      continue;
    int x0 = qMax(0, int(std::floor(
                         qMin(a.point.x(), qMin(b.point.x(), c.point.x()))))),
        x1 = qMin(
            width - 1,
            int(std::ceil(qMax(a.point.x(), qMax(b.point.x(), c.point.x()))))),
        y0 = qMax(0, int(std::floor(
                         qMin(a.point.y(), qMin(b.point.y(), c.point.y()))))),
        y1 = qMin(
            height - 1,
            int(std::ceil(qMax(a.point.y(), qMax(b.point.y(), c.point.y())))));
    const auto &m = materials[t.material];
    QVector3D base = m.color;
    if (m.button == selected)
      base = base * .98 + QVector3D(.005, .022, .04);
    auto shade = [&](quint32 i) {
      auto s = shades[i];
      float highlight =
          (1 - m.roughness) * (.12f * s.specular + .13f * s.satin);
      return base * s.diffuse + QVector3D(1, 1, 1) * highlight;
    };
    auto ca = shade(t.indices[0]), cb = shade(t.indices[1]),
         cc = shade(t.indices[2]);
    if (m.emissive) {
      QVector3D tint = displayLighting ? QVector3D(light.redF(), light.greenF(),
                                                   light.blueF())
                                       : QVector3D(1, 1, 1);
      ca = multiply(tint, displayLighting ? QVector3D(1, 1, 1)
                                          : multiply(m.color, a.color));
      cb = multiply(tint, displayLighting ? QVector3D(1, 1, 1)
                                          : multiply(m.color, b.color));
      cc = multiply(tint, displayLighting ? QVector3D(1, 1, 1)
                                          : multiply(m.color, c.color));
    }
    for (int y = y0; y <= y1; ++y) {
      auto *line = reinterpret_cast<QRgb *>(result.image.scanLine(y));
      for (int x = x0; x <= x1; ++x) {
        QPointF p(x + .5, y + .5);
        float u = edge(b.point, c.point, p) / area,
              v = edge(c.point, a.point, p) / area, w = 1 - u - v;
        if (u < -.00001f || v < -.00001f || w < -.00001f)
          continue;
        float inv =
            u * a.inverseDepth + v * b.inverseDepth + w * c.inverseDepth;
        float depth = 1 / inv;
        size_t index = size_t(y) * width + x;
        if (depth >= depths[index])
          continue;
        u *= a.inverseDepth / inv;
        v *= b.inverseDepth / inv;
        w *= c.inverseDepth / inv;
        auto color = ca * u + cb * v + cc * w;
        if (m.textured) {
          auto uv = a.uv * u + b.uv * v + c.uv * w;
          float tx = qBound(0., uv.x() * logo.width() - .5, logo.width() - 1.),
                ty =
                    qBound(0., uv.y() * logo.height() - .5, logo.height() - 1.);
          int x0 = int(tx), y0 = int(ty);
          float fx = tx - x0, fy = ty - y0, alpha = 0;
          QVector3D ink;
          // Bilinear premultiplied sampling preserves the original printed
          // markings without dark fringes at transparent logo edges.
          for (int j = 0; j < 2; ++j)
            for (int i = 0; i < 2; ++i) {
              QRgb texel = logo.pixel(qMin(x0 + i, logo.width() - 1),
                                      qMin(y0 + j, logo.height() - 1));
              float coverage =
                  (i ? fx : 1 - fx) * (j ? fy : 1 - fy) * qAlpha(texel) / 255.f;
              ink += QVector3D(qRed(texel) / 255.f, qGreen(texel) / 255.f,
                               qBlue(texel) / 255.f) *
                     coverage;
              alpha += coverage;
            }
          if (alpha < .03f)
            continue;
          QRgb under = line[x];
          color = ink + QVector3D(qRed(under) / 255.f, qGreen(under) / 255.f,
                                  qBlue(under) / 255.f) *
                            (1 - alpha);
        }
        line[x] = pixel(color);
        depths[index] = depth;
        if (!m.textured)
          result.buttons[index] = qint8(m.button);
      }
    }
  }
  for (int n = 0; n < 8; ++n) {
    auto p = project(anchors[n]);
    result.anchors[n] = p.point / dpr;
    int x = qRound(p.point.x()), y = qRound(p.point.y());
    float nearest = std::numeric_limits<float>::infinity();
    for (int j = qMax(0, y - 2); j <= qMin(height - 1, y + 2); ++j)
      for (int i = qMax(0, x - 2); i <= qMin(width - 1, x + 2); ++i)
        nearest = qMin(nearest, depths[size_t(j) * width + i]);
    result.visible[n] =
        std::isfinite(nearest) && 1 / p.inverseDepth <= nearest + .07;
  }
  return result;
}
} // namespace mouse
