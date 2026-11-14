// Development render/export/benchmark tool. Run only on Box or in an
// explicitly isolated local GUI context. Never opens a window or sends input.
#include "ui/model.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <algorithm>

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addOption({"output", "Write isolated model-view images", "directory"});
  parser.addOption(
      {"frames", "Measure N different camera frames (max 120)", "count", "30"});
  parser.process(app);
  mouse::MouseModel model;
  if (!model.valid())
    return 1;
  const QVector<QPointF> angles = {{24, 60},  {0, 90},   {90, 12},
                                   {-90, 12}, {150, 45}, {0, -60}};
  const QStringList names = {"perspective", "top",   "left",
                             "right",       "front", "underside"};
  if (parser.isSet("output")) {
    QString root = parser.value("output");
    QDir().mkpath(root);
    for (int n = 0; n < angles.size(); ++n) {
      auto r = model.render({1600, 1400}, 2, angles[n].x(), angles[n].y(), 1,
                            -1, QColor("#159bff"), false);
      if (!r.image.save(root + "/gm320-" + names[n] + ".png"))
        return 2;
    }
  }
  std::vector<double> times;
  int frames = qBound(1, parser.value("frames").toInt(), 120);
  for (int n = 0; n < frames; ++n) {
    QElapsedTimer timer;
    timer.start();
    auto r = model.render({1440, 900}, 2, n * 360. / frames, 60, 1, -1,
                          QColor("#159bff"), false);
    times.push_back(timer.nsecsElapsed() / 1000000.);
    if (r.image.isNull())
      return 3;
  }
  std::sort(times.begin(), times.end());
  QJsonObject result{{"vertices", model.vertexCount()},
                     {"triangles", model.triangleCount()},
                     {"pixels", QJsonArray{1440, 900}},
                     {"frames", frames},
                     {"median_ms", times[times.size() / 2]},
                     {"p95_ms", times[size_t((times.size() - 1) * .95)]},
                     {"hardware_writes", 0},
                     {"input_injection", 0}};
  QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact)
                      << Qt::endl;
}
