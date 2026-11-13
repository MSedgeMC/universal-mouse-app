#include "core/profile.h"
#include "core/protocol.h"
#include "core/storage.h"
#include "platform/linux.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <sys/stat.h>
using namespace mouse;
struct Sim : Memory {
  Image image = fixture();
  Parts writes;
  int failOn = 0, finals = 0;
  bool recoveryFail = false;
  Image snapshot() override { return image; }
  void pause(int) override {}
  void finalize() override { ++finals; }
  void write(const QVector<int> &p, const Image &i) override {
    writes << p;
    for (int a : p)
      image[a] = i[a];
    if (writes.size() == failOn)
      throw Error("commit failed after taking effect");
    if (recoveryFail && writes.size() > failOn) {
      image[p[0]] ^= 1;
      throw Error("rollback failed");
    }
  }
};
class CoreTest : public QObject {
  Q_OBJECT
private slots:
  void decodeFixture() {
    auto s = decode(fixture());
    QCOMPARE(s["polling"].toInt(), 1000);
    QCOMPARE(s["button1"].toString(), "Left click");
    QCOMPARE(s["dpi"].toArray().size(), 6);
  }
  void pollingPreserves() {
    for (int rate : {125, 250, 500, 1000}) {
      auto b = fixture(), a = merge(b, {{"polling", rate}});
      QCOMPARE(decode(a)["polling"].toInt(), rate);
      for (int ad : masks.keys())
        if (ad != 0x1e)
          QCOMPARE(a[ad], b[ad]);
    }
    QVERIFY_EXCEPTION_THROWN(merge(fixture(), {{"polling", true}}), Error);
    QVERIFY_EXCEPTION_THROWN(merge(fixture(), {{"polling", 2000}}), Error);
  }
  void allButtons() {
    for (int n = 0; n < 7; ++n)
      for (int flags : {0, 32, 96, 224})
        for (auto action : actions) {
          auto b = fixture();
          b[buttonAddresses[n]] = flags | 5;
          b[n == 0 ? 0x46 : 0x44] = 1;
          auto a = merge(b, {{"button" + QString::number(n + 1), action}});
          QCOMPARE(a[buttonAddresses[n]] & 0xe0, flags);
          QCOMPARE(a[buttonAddresses[n]] & 31, actionCodes[action]);
          for (int ad : masks.keys())
            if (ad != buttonAddresses[n])
              QCOMPARE(a[ad], b[ad]);
        }
  }
  void lastLeft() {
    QVERIFY_EXCEPTION_THROWN(merge(fixture(), {{"button1", "Disabled"}}),
                             Error);
    Sim s;
    auto before = s.image, after = merge(before, {{"button1", "Disabled"},
                                                  {"button7", "Left click"}});
    QVERIFY(transact(s, before, after));
    QVERIFY(s.writes[0].contains(0x4a));
    QCOMPARE(s.image, after);
  }
  void rollback() {
    Sim s;
    s.failOn = 1;
    auto before = s.image, after = merge(before, {{"polling", 500}});
    try {
      transact(s, before, after);
      QFAIL("Expected failure");
    } catch (const Error &e) {
      QVERIFY(QString(e.what()).contains("original settings restored"));
    }
    QCOMPARE(s.image, before);
    QCOMPARE(s.finals, 1);
  }
  void failedRollback() {
    Sim s;
    s.failOn = 1;
    s.recoveryFail = true;
    auto before = s.image, after = merge(before, {{"polling", 500}});
    try {
      transact(s, before, after);
      QFAIL("Expected failure");
    } catch (const Error &e) {
      QVERIFY(QString(e.what()).contains("restoration could not be verified"));
    }
  }
  void selectiveRestore() {
    auto before = fixture(),
         after = merge(before, {{"lighting", QJsonObject{{"speed_raw", 1}}}});
    auto current =
        merge(after, {{"lighting", QJsonObject{{"effect", "Neon"}}}});
    auto restored = restore(current, before, after);
    QCOMPARE(decode(restored)["lighting"].toObject()["effect"].toString(),
             "Neon");
    QCOMPARE(restored[0x1c] & 7, before[0x1c] & 7);
  }
  void backupPrivate() {
    QTemporaryDir d;
    QString p = d.path() + "/backup.json";
    auto before = fixture(), after = merge(before, {{"polling", 500}});
    saveBackup(p, before, after);
    auto pair = loadBackup(p, after);
    QCOMPARE(pair.first, before);
    QCOMPARE(pair.second, after);
    QVERIFY(!(QFile::permissions(p) &
              (QFileDevice::ReadGroup | QFileDevice::ReadOther)));
    QFile::remove(p);
    QFile::link(d.path() + "/target", p);
    QVERIFY_EXCEPTION_THROWN(loadBackup(p, after), Error);
  }
  void macroValidation() {
    QJsonArray events = {QJsonObject{{"type", "down"}, {"key", "Ctrl"}},
                         QJsonObject{{"type", "down"}, {"key", "C"}},
                         QJsonObject{{"type", "delay"}, {"ms", 100}},
                         QJsonObject{{"type", "up"}, {"key", "C"}},
                         QJsonObject{{"type", "up"}, {"key", "Ctrl"}}};
    QCOMPARE(macroExpression(events),
             "key_down(KEY_LEFTCTRL).key_down(KEY_C).wait(100).key_up(KEY_C)."
             "key_up(KEY_LEFTCTRL)");
    events.removeLast();
    QVERIFY_EXCEPTION_THROWN(macroExpression(events), Error);
    QCOMPARE(macroKeys.size(), 67);
  }
  void profiles() {
    auto p = defaultProfile();
    validateProfile(p);
    QJsonArray a = {p, p, p, p};
    QCOMPARE(importProfiles({{"version", 2}, {"profiles", a}}), a);
    auto bad = p;
    bad["trigger"] = 272;
    QVERIFY_EXCEPTION_THROWN(validateProfile(bad), Error);
  }
  void settingsRebase() {
    auto before = decode(fixture()), desired = before;
    desired["dpi"] = QJsonArray{800, 1600, 3200, 4000, 5600, 800};
    auto fresh = before;
    fresh["enabled"] = QJsonArray{true, false, false, false, false, false};
    fresh["stage"] = 0;
    auto result = rebaseSettings(desired, before, fresh);
    QCOMPARE(result["enabled"], before["enabled"]);
    QCOMPARE(result["stage"], before["stage"]);
    auto p = defaultProfile();
    p["live"] =
        QJsonObject{{"effect", "Off"}, {"color", "#ff0000"}, {"speed", 3}};
    auto off = merge(fixture(), hardwareChanges(p, before));
    QVERIFY(hardwareChanges(p, decode(off)).isEmpty());
    p["live"] = QJsonObject();
    QCOMPARE(hardwareChanges(p, decode(off))["lighting"]
                 .toObject()["response_enabled"],
             before["lighting"].toObject()["response_enabled"]);
  }
  void linuxMockLifecycle() {
    LinuxIntegration l(true);
    auto desktop = l.desktopRead();
    QCOMPARE(l.desktopApply(desktop["revision"].toString(), 0, 1, true),
             desktop);
    QVERIFY_EXCEPTION_THROWN(l.desktopApply("stale", 0, 1, true), Error);
    QJsonArray a = {QJsonObject{{"type", "down"}, {"key", "A"}},
                    QJsonObject{{"type", "up"}, {"key", "A"}}};
    QCOMPARE(l.macroApply(0, 275, a)["status"].toString(), "active");
    QCOMPARE(l.macroState(l.activeGroup), "RUNNING");
    l.close();
    QVERIFY(l.activeGroup.isEmpty());
    QVERIFY_EXCEPTION_THROWN(l.macroApply(0, 272, a), Error);
  }
  void restrictiveUmask() {
    QTemporaryDir d;
    mode_t previous = umask(0002);
    privateDir(d.path() + "/new");
    umask(previous);
    QCOMPARE(QFile::permissions(d.path() + "/new") &
                 (QFileDevice::WriteGroup | QFileDevice::WriteOther),
             QFileDevice::Permissions());
  }
  void lightingLimits() {
    QVERIFY_EXCEPTION_THROWN(
        merge(fixture(), {{"lighting", QJsonObject{{"effect", "Ambilight"}}}}),
        Error);
    QVERIFY_EXCEPTION_THROWN(
        merge(fixture(), {{"lighting", QJsonObject{{"speed_raw", 7}}}}), Error);
    QVERIFY_EXCEPTION_THROWN(
        merge(fixture(),
              {{"lighting", QJsonObject{{"effect", "Mono Water Effect"},
                                        {"direction", "Overwrite"}}}}),
        Error);
    QCOMPARE(rgbReports({{"effect", "Off"}, {"color", "#ff0000"}, {"speed", 3}})
                 .size(),
             7);
  }
};
QTEST_GUILESS_MAIN(CoreTest)
#include "core_test.moc"
