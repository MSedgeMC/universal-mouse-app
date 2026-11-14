#include "core/storage.h"
#include "ui/window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QLockFile>
#include <QScreen>
#include <QTextStream>
#include <QTimer>
#include <unistd.h>
int main(int argc, char **argv) {
  bool cli = false;
  for (int i = 1; i < argc; ++i)
    if (QString(argv[i]) == "--cli")
      cli = true;
  std::unique_ptr<QCoreApplication> app;
  if (cli)
    app = std::make_unique<QCoreApplication>(argc, argv);
  else
    app = std::make_unique<QApplication>(argc, argv);
  if (!cli) {
    QApplication::setStyle("Fusion");
    static_cast<QApplication *>(app.get())->setFont(
        QFont("Ubuntu", 10, QFont::Medium));
  }
  QCoreApplication::setApplicationName("Orbit");
  QCoreApplication::setApplicationVersion("0.4.0");
  QCommandLineParser p;
  p.addHelpOption();
  p.addVersionOption();
  p.addOption({"mock", "Use simulated mouse; no hardware or desktop writes"});
  p.addOption({"cli", "Run headless diagnostics"});
  p.addOption({"action",
               "CLI: list, read, apply, restore, rgb, experimental, merge, "
               "desktop, desktop-apply, macro-validate",
               "action", "read"});
  p.addOption(
      {"verify-gui", "Capture and verify inside isolated screen", "directory"});
  p.addOption({"quit-after", "Quit after seconds (isolated smoke test only)",
               "seconds"});
  p.process(*app);
  try {
    mouse::require(geteuid() != 0, "Run as your normal desktop user");
    bool mock = p.isSet("mock");
    if (cli) {
      QString action = p.value("action");
      QJsonObject result;
      if (action == "list") {
        QJsonArray all;
        for (auto d : mouse::discover())
          all.append(d.json());
        result["devices"] = all;
      } else if (action == "desktop") {
        mouse::LinuxIntegration l(mock);
        result = l.desktopRead();
      } else {
        QJsonObject request;
        if (action != "read") {
          QFile input;
          input.open(stdin, QIODevice::ReadOnly);
          auto data = input.read(100001);
          mouse::require(data.size() <= 100000, "Request too large");
          QJsonParseError error;
          auto doc = QJsonDocument::fromJson(data, &error);
          mouse::require(error.error == QJsonParseError::NoError &&
                             doc.isObject(),
                         "Invalid request JSON");
          request = doc.object();
        }
        if (action == "desktop-apply") {
          mouse::require(
              request.keys() ==
                      QStringList({"enabled", "gain", "revision", "scroll"}) &&
                  request["gain"].isDouble() && request["scroll"].isDouble() &&
                  request["enabled"].isBool(),
              "Invalid desktop request");
          mouse::LinuxIntegration integration(mock);
          result = integration.desktopApply(
              request["revision"].toString(), request["gain"].toDouble(),
              request["scroll"].toDouble(), request["enabled"].toBool());
        } else if (action == "merge") {
          auto before = mouse::fromJson(request["image"].toObject());
          auto after = mouse::merge(before, request["changes"].toObject());
          result = {{"image", mouse::toJson(after)},
                    {"settings", mouse::decode(after)},
                    {"revision", mouse::revision(after)}};
        } else if (action == "macro-validate")
          result = {{"expression",
                     mouse::macroExpression(request["events"].toArray())}};
        else {
          mouse::Device d(mock);
          result = d.operation(action, request);
        }
      }
      QTextStream(stdout) << QJsonDocument(result).toJson(
                                 QJsonDocument::Compact)
                          << Qt::endl;
      return 0;
    }
    mouse::privateDir(mouse::configDir());
    QLockFile lock(mouse::configDir() + "/application.lock");
    lock.setStaleLockTime(30000);
    mouse::require(lock.tryLock(), "Orbit is already running");
    QApplication::setWindowIcon(QIcon(":/design/universal-mouse-app.svg"));
    mouse::Window window(mock);
    if (auto *screen = QGuiApplication::primaryScreen();
        screen && QGuiApplication::platformName() != "offscreen") {
      const auto available = screen->availableGeometry();
      window.resize(qMax(window.minimumWidth(),
                         qMin(window.width(), available.width() - 32)),
                    qMax(window.minimumHeight(),
                         qMin(window.height(), available.height() - 36)));
    }
    window.show();
    if (p.isSet("verify-gui")) {
      auto *timer = new QTimer(&window);
      QObject::connect(timer, &QTimer::timeout, &window, [&window, &p, timer] {
        if (window.ready()) {
          timer->stop();
          bool ok = window.verifyGui(p.value("verify-gui"));
          QCoreApplication::exit(ok ? 0 : 1);
        }
      });
      timer->start(150);
      QTimer::singleShot(30000, app.get(), [] { QCoreApplication::exit(2); });
    }
    if (p.isSet("quit-after")) {
      mouse::require(qEnvironmentVariable("KUBUNTU_VIRTUAL_SCREEN") == "1" ||
                         qEnvironmentVariable("UMA_ISOLATED_TEST") == "1",
                     "Timed quit is only for isolated testing");
      QTimer::singleShot(p.value("quit-after").toInt() * 1000, app.get(),
                         &QCoreApplication::quit);
    }
    return app->exec();
  } catch (const std::exception &e) {
    QTextStream(stderr) << e.what() << Qt::endl;
    return 1;
  }
}
