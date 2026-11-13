#include "core/storage.h"
#include "ui/window.h"
#include <QTemporaryDir>
#include <QtTest>
namespace mouse {
class WindowTest : public QObject {
  Q_OBJECT
private slots:
  void settingsAndTransactions() {
    QTemporaryDir dir;
    qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
    Window w(true);
    w.show();
    QTRY_VERIFY(w.ready());
    QTRY_VERIFY(!w.busy);
    auto original = w.current;
    QVERIFY(!w.applyButton->isEnabled());
    auto preset = w.profile();
    auto setting = preset["settings"].toObject();
    setting["polling"] = 500;
    preset["settings"] = setting;
    w.setProfile(preset);
    w.render();
    w.saveProfiles();
    QCOMPARE(w.device.mockImage, fixture());
    QVERIFY(w.applyButton->isEnabled());
    w.loadMouse();
    QTRY_VERIFY(!w.busy);
    QCOMPARE(w.profile()["settings"].toObject()["polling"].toInt(), 500);
    w.hardwareApply();
    QTRY_VERIFY(!w.busy);
    QCOMPARE(w.current["polling"].toInt(), 500);
    QVERIFY(!w.applyButton->isEnabled());
    QVERIFY(w.restoreButton->isEnabled());
    w.restoreButton->click();
    QTRY_VERIFY(!w.busy);
    QCOMPARE(w.current, original);
    // Saves operate on the selected preset, leaving other edits staged.
    w.profileCombo->setCurrentIndex(1);
    auto p = w.profile();
    auto s = p["settings"].toObject();
    s["polling"] = 250;
    p["settings"] = s;
    w.setProfile(p);
    w.profileCombo->setCurrentIndex(0);
    w.saveProfiles();
    QCOMPARE(w.saved[1].toObject()["settings"].toObject()["polling"].toInt(),
             1000);
    // Off suppresses firmware responses without destroying desired choices.
    p = w.profile();
    p["live"] =
        QJsonObject{{"effect", "Off"}, {"color", "#ff0000"}, {"speed", 3}};
    w.setProfile(p);
    w.render();
    QJsonValue wanted =
        p["settings"].toObject()["lighting"].toObject()["response_enabled"];
    w.hardwareApply();
    QTRY_VERIFY(!w.busy);
    QCOMPARE(w.profile()["settings"]
                 .toObject()["lighting"]
                 .toObject()["response_enabled"],
             wanted);
    for (auto v :
         w.current["lighting"].toObject()["response_enabled"].toArray())
      QVERIFY(!v.toBool());
    QVERIFY(!w.applyButton->isEnabled());
    p = w.profile();
    p["live"] = QJsonObject();
    w.setProfile(p);
    w.render();
    w.hardwareApply();
    QTRY_VERIFY(!w.busy);
    QCOMPARE(w.current["lighting"].toObject()["response_enabled"], wanted);
    // Switching preset renders but does not write the mouse.
    auto before = w.device.mockImage;
    w.profileCombo->setCurrentIndex(1);
    QCoreApplication::processEvents();
    QCOMPARE(w.device.mockImage, before);
    w.profiles = w.saved;
    w.close();
    QTRY_VERIFY(!w.busy);
  }
  void savedStartup() {
    QTemporaryDir dir;
    qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
    auto p = defaultProfile();
    auto s = p["settings"].toObject();
    s["polling"] = 500;
    p["settings"] = s;
    saveJson(configDir() + "/profiles.json",
             QJsonObject{{"version", 2}, {"profiles", QJsonArray{p, p, p, p}}});
    Window w(true);
    w.show();
    QTRY_VERIFY(w.ready());
    QTRY_VERIFY(!w.busy);
    QCOMPARE(w.profile(), p);
    QCOMPARE(w.current["polling"].toInt(), 1000);
    w.close();
    QTRY_VERIFY(!w.busy);
  }
  void viewerPicking() {
    Viewer v;
    v.resize(400, 400);
    v.show();
    QTest::qWait(50);
    for (int n = 0; n < 7; ++n)
      QCOMPARE(v.buttonAt(v.buttonCenter(n)), n);
    v.setThreeD(true);
    QTest::qWait(50);
    // Each slot has a projected independent region; frontmost visible faces
    // win.
    for (int n = 0; n < 7; ++n)
      QVERIFY(!v.buttonCenter(n).isNull());
    QSignalSpy selected(&v, &Viewer::selected);
    v.setThreeD(false);
    QTest::qWait(50);
    QTest::mouseClick(&v, Qt::LeftButton, Qt::NoModifier,
                      v.buttonCenter(3).toPoint());
    QCOMPARE(selected.size(), 1);
    QCOMPARE(selected[0][0].toInt(), 3);
  }
};
} // namespace mouse
QTEST_MAIN(mouse::WindowTest)
#include "window_test.moc"
