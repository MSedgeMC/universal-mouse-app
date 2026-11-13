#include "linux.h"
#include "core/storage.h"
#include "drivers/device.h"
#include <QCryptographicHash>
#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QThread>
#include <cmath>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
namespace mouse {
QString run(const QString &exe, const QStringList &args, int timeout) {
  QProcess p;
  p.start(exe, args);
  require(p.waitForStarted(2000), "Cannot start " + QFileInfo(exe).fileName());
  if (!p.waitForFinished(timeout)) {
    p.kill();
    p.waitForFinished();
    throw Error(QFileInfo(exe).fileName() + " timed out");
  }
  QString detail =
      QString::fromUtf8(p.readAllStandardError()).trimmed().left(700);
  require(p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0,
          detail.isEmpty() ? QFileInfo(exe).fileName() + " failed" : detail);
  auto out = p.readAllStandardOutput();
  require(out.size() <= 32768, "Unexpected subprocess output size");
  return QString::fromUtf8(out);
}
static QVariant dbus(const QString &service, const QString &path,
                     const QString &iface, const QString &method,
                     const QList<QVariant> &args = {}, bool system = false) {
  QDBusInterface i(service, path, iface,
                   system ? QDBusConnection::systemBus()
                          : QDBusConnection::sessionBus());
  i.setTimeout(4000);
  auto r = i.callWithArgumentList(QDBus::Block, method, args);
  require(r.type() != QDBusMessage::ErrorMessage, r.errorMessage());
  return r.arguments().isEmpty() ? QVariant() : r.arguments()[0];
}
static QVariantMap props(const QString &path) {
  return qdbus_cast<QVariantMap>(dbus("org.kde.KWin", path,
                                      "org.freedesktop.DBus.Properties",
                                      "GetAll", {"org.kde.KWin.InputDevice"}));
}
static QString kdePointer() {
  QString root = "/org/kde/KWin/InputDevice";
  QStringList names = qdbus_cast<QStringList>(dbus(
      "org.kde.KWin", root, "org.kde.KWin.InputDeviceManager", "ListPointers"));
  QStringList matches;
  for (auto n : names) {
    if (!QRegularExpression("^event[0-9]+$").match(n).hasMatch())
      continue;
    auto p = props(root + "/" + n);
    if (p["vendor"].toInt() == 0x30fa && p["product"].toInt() == 0x1440 &&
        p["pointer"].toBool() &&
        QStringList({"INSTANT USB GAMING MOUSE", "FANTECH FANTECH-X16v2"})
            .contains(p["name"].toString().trimmed()))
      matches << root + "/" + n;
  }
  require(matches.size() == 1,
          "KWin must expose exactly one matching mouse pointer");
  return matches[0];
}
static QString hash(const QJsonObject &j) {
  return QCryptographicHash::hash(
             QJsonDocument(j).toJson(QJsonDocument::Compact),
             QCryptographicHash::Sha256)
      .toHex();
}
QString LinuxIntegration::wheelSchema() const {
  QString d = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
  return d.contains("cinnamon", Qt::CaseInsensitive)
             ? "org.cinnamon.desktop.peripherals.mouse"
         : d.contains("gnome", Qt::CaseInsensitive) ||
                 d.contains("unity", Qt::CaseInsensitive)
             ? "org.gnome.desktop.peripherals.mouse"
             : "";
}
static QString wheelConfig(int n) {
  QString text = QString("# Resa wheel settings v1\n\".*\"\nNone, Up, Button4, "
                         "%1\nNone, Down, Button5, %1\n")
                     .arg(n);
  for (auto key : QStringList({"Control_L", "Shift_L", "Alt_L", "Super_L"}))
    text +=
        QString("%1, Up, %1|Button4, 1\n%1, Down, %1|Button5, 1\n").arg(key);
  return text;
}
static QString managedWheel() {
  QString p = QDir::homePath() + "/.imwheelrc";
  int fd = open(QFile::encodeName(p).constData(),
                O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    require(errno == ENOENT, "Cannot safely read imwheel configuration");
    return {};
  }
  struct stat st {};
  bool safe = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
              st.st_uid == getuid() && !(st.st_mode & 0022) &&
              st.st_size <= 4096;
  if (!safe) {
    close(fd);
    throw Error("Existing imwheel configuration is not managed by this app");
  }
  QFile f;
  if (!f.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
    close(fd);
    throw Error("Cannot read imwheel config");
  }
  QString text = QString::fromUtf8(f.read(4097));
  for (int n = 1; n <= 11; ++n)
    if (text == wheelConfig(n))
      return text;
  throw Error(
      "Existing ~/.imwheelrc will be kept; it is not managed by this app");
}
static void replaceWheel(const QString &expected, const QString &text) {
  require(managedWheel() == expected,
          "Wheel config changed; reload before applying");
  QString path = QDir::homePath() + "/.imwheelrc";
  if (text.isEmpty()) {
    require(expected.isEmpty() || QFile::remove(path),
            "Cannot remove managed wheel configuration");
    return;
  }
  auto permissions = expected.isEmpty()
                         ? QFileDevice::ReadOwner | QFileDevice::WriteOwner
                         : QFile::permissions(path);
  QSaveFile f(path);
  f.setDirectWriteFallback(false);
  require(f.open(QIODevice::WriteOnly) && f.setPermissions(permissions),
          "Cannot save wheel configuration");
  auto bytes = text.toUtf8();
  require(f.write(bytes) == bytes.size(),
          "Incomplete wheel configuration write");
  require(managedWheel() == expected, "Wheel config changed during save");
  require(f.commit(), "Cannot commit wheel configuration");
}
static void startWheel(pid_t &pid) {
  require(pid <= 0, "Wheel helper already tracked");
  char exe[] = "/usr/bin/imwheel", a[] = "-d", b[] = "-b", c[] = "45";
  char *argv[] = {exe, a, b, c, nullptr};
  require(posix_spawn(&pid, exe, nullptr, nullptr, argv, environ) == 0,
          "Cannot start wheel helper");
  QThread::msleep(180);
  if (waitpid(pid, nullptr, WNOHANG) != 0) {
    pid = -1;
    throw Error("Wheel helper exited before ready");
  }
}
static void stopWheel(pid_t &pid) {
  if (pid <= 0)
    return;
  int state = waitpid(pid, nullptr, WNOHANG);
  if (state == pid || (state < 0 && errno == ECHILD)) {
    pid = -1;
    return;
  }
  require(kill(pid, SIGTERM) == 0, "Cannot stop wheel helper");
  for (int n = 0; n < 15; ++n) {
    if (waitpid(pid, nullptr, WNOHANG) == pid) {
      pid = -1;
      return;
    }
    QThread::msleep(100);
  }
  throw Error("Wheel helper did not stop; no replacement started");
}
QJsonObject LinuxIntegration::desktopRead() {
  if (mock) {
    QJsonObject j = {{"gain", 0},
                     {"scroll", 1},
                     {"pointer", true},
                     {"wheel", true},
                     {"native_wheel", true},
                     {"note", "Demo desktop controls; no desktop writes"}};
    j["revision"] = hash(j);
    return j;
  }
  QJsonObject out = {{"gain", 0},
                     {"scroll", 1},
                     {"pointer", false},
                     {"wheel", false},
                     {"native_wheel", false},
                     {"note", "Desktop controls unavailable in this session"}};
  if (qEnvironmentVariable("XDG_CURRENT_DESKTOP")
          .contains("KDE", Qt::CaseInsensitive)) {
    try {
      QString path = kdePointer();
      auto p = props(path);
      double gain = p["pointerAcceleration"].toDouble(),
             scroll = p["scrollFactor"].toDouble();
      require(std::isfinite(gain) && qAbs(gain) <= 1 && std::isfinite(scroll) &&
                  scroll > 0,
              "Invalid KDE settings");
      out = {{"path", path},
             {"gain", gain},
             {"scroll", scroll},
             {"pointer", true},
             {"wheel", true},
             {"native_wheel", true},
             {"note", "Applies only to this GM320 pointer. Stored by KWin."}};
    } catch (const std::exception &e) {
      out["note"] = QString::fromUtf8(e.what());
    }
  } else {
    QString schema = wheelSchema();
    if (!schema.isEmpty()) {
      try {
        bool ok;
        double gain = run("/usr/bin/gsettings", {"get", schema, "speed"})
                          .trimmed()
                          .toDouble(&ok);
        require(ok && qAbs(gain) <= 1, "Invalid desktop pointer speed");
        out["gain"] = gain;
        out["pointer"] =
            run("/usr/bin/gsettings", {"writable", schema, "speed"})
                .trimmed() == "true";
        out["note"] =
            "Pointer speed applies to all mice in this desktop session.";
      } catch (const std::exception &e) {
        out["note"] = QString::fromUtf8(e.what());
      }
    }
    try {
      QString cfg = managedWheel();
      require(qEnvironmentVariable("XDG_SESSION_TYPE") == "x11" &&
                  !qEnvironmentVariable("DISPLAY").isEmpty(),
              "Wheel multiplier requires X11 outside KDE");
      require(QFileInfo("/usr/bin/imwheel").isExecutable(),
              "Install optional imwheel for X11 scrolling");
      QProcess p;
      p.start("/usr/bin/pgrep",
              {"-u", QString::number(getuid()), "-x", "imwheel"});
      require(p.waitForFinished(3000) &&
                  p.exitStatus() == QProcess::NormalExit &&
                  (p.exitCode() == 0 || p.exitCode() == 1),
              "Cannot check wheel helper");
      for (auto s : QString::fromUtf8(p.readAllStandardOutput())
                        .split('\n', Qt::SkipEmptyParts))
        require(s.toLongLong() == wheelPid,
                "Another imwheel process is running; it will be kept");
      out["wheel"] = true;
      out["wheel_enabled"] = wheelPid > 0;
      out["config"] = cfg;
      int n = 1;
      for (int x = 1; x <= 11; ++x)
        if (cfg == wheelConfig(x))
          n = x;
      out["scroll"] = n;
      out["wheel_note"] =
          "X11 wheel events while this app is open; modifiers remain 1x.";
    } catch (const std::exception &e) {
      out["wheel_note"] = QString::fromUtf8(e.what());
    }
  }
  out["revision"] = hash(out);
  return out;
}
QJsonObject LinuxIntegration::desktopApply(const QString &expected, double gain,
                                           double scroll, bool enabled) {
  require(std::isfinite(gain) && gain >= -1 && gain <= 1 &&
              std::isfinite(scroll) && scroll >= .25 && scroll <= 11,
          "Desktop setting outside supported range");
  const auto before = desktopRead();
  require(before["revision"] == expected,
          "Desktop settings changed. Reload before applying");
  require(before["pointer"].toBool() || gain == before["gain"].toDouble(),
          before["note"].toString());
  require(before["wheel"].toBool() || scroll == before["scroll"].toDouble(),
          before["wheel_note"].toString());
  if (mock)
    return before;
  bool kde = before["native_wheel"].toBool();
  QString path = before["path"].toString(), schema = wheelSchema(),
          cfg = before["config"].toString();
  bool pointerWritten = false, wheelWritten = false, wasRunning = wheelPid > 0;
  QString writtenConfig = cfg;
  auto set = [&](QString key, double value) {
    dbus("org.kde.KWin", path, "org.freedesktop.DBus.Properties", "Set",
         {"org.kde.KWin.InputDevice", key,
          QVariant::fromValue(QDBusVariant(value))});
  };
  try {
    if (kde) {
      require(scroll <= 2.5, "KDE scroll factor outside supported range");
      pointerWritten = true;
      set("pointerAcceleration", gain);
      wheelWritten = true;
      set("scrollFactor", scroll);
    } else {
      if (before["pointer"].toBool()) {
        pointerWritten = true;
        run("/usr/bin/gsettings",
            {"set", schema, "speed", QString::number(gain, 'g', 16)});
      }
      if (before["wheel"].toBool()) {
        require(managedWheel() == cfg, "Wheel config changed");
        stopWheel(wheelPid);
        if (enabled) {
          require(scroll == int(scroll) && scroll >= 1,
                  "Choose an integer X11 wheel multiplier");
          writtenConfig = wheelConfig(int(scroll));
          replaceWheel(cfg, writtenConfig);
          wheelWritten = true;
          startWheel(wheelPid);
        }
      }
    }
    auto after = desktopRead();
    require(!pointerWritten || qAbs(after["gain"].toDouble() - gain) < 1e-8,
            "Desktop pointer readback differs");
    require(!kde || qAbs(after["scroll"].toDouble() - scroll) < 1e-8,
            "KDE scroll readback differs");
    require(
        kde || !before["wheel"].toBool() ||
            (after["wheel"].toBool() &&
             after["wheel_enabled"].toBool() == enabled &&
             (!enabled || qAbs(after["scroll"].toDouble() - scroll) < 1e-8)),
        "X11 wheel readback differs");
    return after;
  } catch (const std::exception &e) {
    QString error = QString::fromUtf8(e.what()), recovery;
    try {
      if (kde) {
        if (wheelWritten)
          set("scrollFactor", before["scroll"].toDouble());
        if (pointerWritten)
          set("pointerAcceleration", before["gain"].toDouble());
      } else {
        stopWheel(wheelPid);
        if (wheelWritten)
          replaceWheel(writtenConfig, cfg);
        if (pointerWritten)
          run("/usr/bin/gsettings",
              {"set", schema, "speed",
               QString::number(before["gain"].toDouble(), 'g', 16)});
        if (wasRunning)
          startWheel(wheelPid);
      }
      auto restored = desktopRead();
      require(qAbs(restored["gain"].toDouble() - before["gain"].toDouble()) <
                  1e-8,
              "Pointer restoration readback differs");
      require(!wheelWritten || (qAbs(restored["scroll"].toDouble() -
                                     before["scroll"].toDouble()) < 1e-8 &&
                                (kde || restored["wheel_enabled"] ==
                                            before["wheel_enabled"])),
              "Wheel restoration readback differs");
    } catch (const std::exception &r) {
      recovery =
          " Recovery could not be confirmed: " + QString::fromUtf8(r.what());
    }
    throw Error(error + recovery);
  }
}
QString LinuxIntegration::macroGroup() {
  if (mock)
    return "Demo mouse";
  require(QFileInfo("/usr/bin/input-remapper-control").isExecutable(),
          "Install Input Remapper for Linux macros");
  int supported = 0;
  for (const auto &device : discover())
    if (device.supported)
      ++supported;
  require(supported == 1,
          "Connect exactly one supported GM320 before enabling macros");
  QStringList matches;
  for (auto name :
       run("/usr/bin/input-remapper-control", {"--list-devices"}).split('\n')) {
    if (name.endsWith('\r'))
      name.chop(1);
    if (QRegularExpression(
            "^(?:INSTANT USB GAMING MOUSE|USB GAMING MOUSE)(?:\\s+\\d+)?\\s*$")
            .match(name)
            .hasMatch())
      matches << name;
  }
  require(matches.size() == 1, "Input Remapper must detect exactly one GM320");
  return matches[0];
}
QString LinuxIntegration::macroState(const QString &group) {
  if (mock)
    return activeGroup.isEmpty() ? "STOPPED" : "RUNNING";
  QString s = dbus("inputremapper.Control", "/inputremapper/Control",
                   "inputremapper.Control", "get_state", {group}, true)
                  .toString();
  require(QStringList({"UNKNOWN", "STARTING", "RUNNING", "FAILED", "STOPPED",
                       "NO_GRAB", "UPGRADE_EVDEV"})
              .contains(s),
          "Invalid remapper state");
  return s;
}
void LinuxIntegration::macroStop() {
  if (activeGroup.isEmpty())
    return;
  if (!mock)
    run("/usr/bin/input-remapper-control",
        {"--command", "stop", "--device", activeGroup, "--config-dir",
         configDir() + "/input-remapper"});
  QString state = mock ? "STOPPED" : macroState(activeGroup);
  for (int n = 0; n < 12 && (state == "RUNNING" || state == "STARTING"); ++n) {
    QThread::msleep(100);
    state = macroState(activeGroup);
  }
  require(state != "RUNNING" && state != "STARTING",
          "Input Remapper is still active. Try Stop again");
  activeGroup.clear();
  activeProfile = -1;
}
QJsonObject LinuxIntegration::macroApply(int profile, int trigger,
                                         const QJsonArray &events) {
  require(profile >= 0 && profile < 4 &&
              QVector<int>({274, 275, 276}).contains(trigger),
          "Invalid macro profile/trigger");
  QString expression = macroExpression(events);
  QString group = macroGroup(), root = configDir() + "/input-remapper",
          preset = QString("Universal Mouse Profile %1").arg(profile + 1);
  if (!mock) {
    require(macroState(group) != "RUNNING" || activeGroup == group,
            "Another remapping session is active for this mouse; stop it in "
            "its owning app first");
    privateDir(root);
    saveJson(root + "/config.json",
             QJsonObject{{"version", "2.2.0"}, {"autoload", QJsonObject()}});
    saveJson(root + "/presets/" + group + "/" + preset + ".json",
             QJsonArray{QJsonObject{
                 {"input_combination",
                  QJsonArray{QJsonObject{{"type", 1}, {"code", trigger}}}},
                 {"target_uinput", "keyboard"},
                 {"output_symbol", expression},
                 {"macro_key_sleep_ms", 0}}});
  }
  activeGroup = group;
  activeProfile = profile;
  try {
    if (!mock)
      run("/usr/bin/input-remapper-control",
          {"--command", "start", "--device", group, "--preset", preset,
           "--config-dir", root});
    QString state = macroState(group);
    for (int n = 0; n < 12 && (state == "UNKNOWN" || state == "STARTING");
         ++n) {
      QThread::msleep(100);
      state = macroState(group);
    }
    require(state == "RUNNING", "Input Remapper did not start (" + state + ")");
  } catch (const std::exception &e) {
    QString reason = QString::fromUtf8(e.what());
    try {
      macroStop();
    } catch (const std::exception &r) {
      throw Error(reason + " " + QString::fromUtf8(r.what()));
    }
    throw Error(reason + " Normal button behavior was restored.");
  }
  return {{"status", "active"},
          {"note", mock ? "Demo macro active; no input sent"
                        : "Linux macro enabled. It is not stored onboard."}};
}
void LinuxIntegration::close() {
  QString error;
  try {
    macroStop();
  } catch (const std::exception &e) {
    error = QString::fromUtf8(e.what());
  }
  try {
    stopWheel(wheelPid);
  } catch (const std::exception &e) {
    error += " " + QString::fromUtf8(e.what());
  }
  require(error.isEmpty(), error);
}
} // namespace mouse
