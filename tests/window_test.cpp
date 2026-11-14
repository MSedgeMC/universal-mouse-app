#include "core/storage.h"
#include "ui/window.h"
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QtTest>
#include <set>
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
  void originalInterfaceAndFireButton() {
    QTemporaryDir dir;
    qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
    Window w(true);
    w.show();
    QTRY_VERIFY(w.ready());
    QTRY_VERIFY(!w.busy);
    QCOMPARE(w.navigation.size(), 5);
    QCOMPARE(w.assignmentRows.size(), 8);
    auto *fire = w.findChild<QComboBox *>("button8");
    QVERIFY(fire);
    QVERIFY(!fire->isEnabled());
    QCOMPARE(w.buttons.size(), 7);
    QVERIFY(!w.profile()["settings"].toObject().contains("button8"));
    auto image = w.device.mockImage;
    QVERIFY(!w.assignmentPanel->isVisible());
    QTest::qWait(30);
    QTest::mouseClick(w.viewer, Qt::LeftButton, Qt::NoModifier,
                      w.viewer->buttonCenter(7).toPoint());
    QVERIFY(w.assignmentRows[7]->property("selected").toBool());
    QVERIFY(w.assignmentPanel->isVisible());
    QCOMPARE(w.assignmentStack->currentIndex(), 7);
    QCOMPARE(w.device.mockImage, image);
    for (int n = 0; n < 5; ++n) {
      w.navigation[n]->click();
      QCOMPARE(w.tabs->currentIndex(), n);
      QVERIFY(w.navigation[n]->isChecked());
    }
    QVERIFY(!w.assignmentPanel->isVisible());
    w.navigation[1]->click();
    int target = qMax(0, w.dpi[0]->currentIndex() - 1);
    w.dpiSlider->setValue(target);
    QCOMPARE(w.profile()["settings"].toObject()["dpi"].toArray()[0].toInt(),
             dpis[target]);
    QCOMPARE(w.device.mockImage, image);
    w.profiles = w.saved;
    w.close();
    QTRY_VERIFY(!w.busy);
  }
  void calloutsAndStagedEditing() {
    QTemporaryDir dir;
    qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
    Window w(true);
    w.show();
    QTRY_VERIFY(w.ready());
    QTRY_VERIFY(!w.busy);
    auto image = w.device.mockImage;
    QTest::qWait(30);
    QTest::mouseClick(w.viewer, Qt::LeftButton, Qt::NoModifier,
                      w.viewer->calloutCenter(4).toPoint());
    QCOMPARE(w.assignmentStack->currentIndex(), 4);
    w.buttons[4]->setCurrentText("DPI up");
    QCOMPARE(w.profile()["settings"].toObject()["button5"].toString(),
             QString("DPI up"));
    QCOMPARE(w.device.mockImage, image);
    w.undoButton->click();
    QCOMPARE(w.profile(), w.saved[w.profileIndex].toObject());
    w.navigation[1]->click();
    auto *adjust = w.findChild<QPushButton *>("adjust-stage-4");
    QVERIFY(adjust);
    adjust->click();
    QCOMPARE(w.dpiEditing, 3);
    auto other = w.dpi[0]->currentData();
    w.dpiSlider->setValue(5);
    QCOMPARE(w.dpi[3]->currentData().toInt(), dpis[5]);
    QCOMPARE(w.dpi[0]->currentData(), other);
    w.dpiEditor->setValue(1500);
    QMetaObject::invokeMethod(w.dpiEditor, "editingFinished");
    QCOMPARE(w.dpi[3]->currentData().toInt(), 1400);
    w.navigation[2]->click();
    w.brightnessSlider->setValue(3);
    QCOMPARE(w.profile()["settings"].toObject()["brightness_raw"].toInt(), 3);
    QCOMPARE(w.device.mockImage, image);
    w.navigation[0]->click();
    w.viewer->setFocus();
    QTest::keyClick(w.viewer, Qt::Key_8);
    QCOMPARE(w.assignmentStack->currentIndex(), 7);
    QVERIFY(!w.findChild<QComboBox *>("button8")->isEnabled());
    w.profiles = w.saved;
    w.close();
    QTRY_VERIFY(!w.busy);
  }
  void viewerPicking() {
    Viewer v;
    v.resize(400, 400);
    v.show();
    QTest::qWait(50);
    for (int n = 0; n < 8; ++n)
      QCOMPARE(v.buttonAt(v.buttonCenter(n)), n);
    for (int n = 0; n < 8; ++n)
      QCOMPARE(v.buttonAt(v.calloutCenter(n)), n);
    v.setThreeD(true);
    QTest::qWait(50);
    // Each slot has a projected independent region; frontmost visible faces
    // win.
    for (int n = 0; n < 8; ++n)
      QVERIFY(!v.buttonCenter(n).isNull());
    QSignalSpy selected(&v, &Viewer::selected);
    v.setThreeD(false);
    QTest::qWait(50);
    QTest::mouseClick(&v, Qt::LeftButton, Qt::NoModifier,
                      v.buttonCenter(3).toPoint());
    QCOMPARE(selected.size(), 1);
    QCOMPARE(selected[0][0].toInt(), 3);
  }
  void gm320ModelAndOcclusion() {
    MouseModel model;
    QVERIFY(model.valid());
    QVERIFY(model.vertexCount() > 8000);
    QVERIFY(model.triangleCount() > 15000);
    auto top =
        model.render({720, 640}, 1, 0, 90, 1, -1, QColor("#159bff"), false);
    QVERIFY(!top.image.isNull());
    // Traced top controls occupy the front half; the logo sits on the palm.
    QVERIFY(top.anchors[0].x() < top.anchors[2].x());
    QVERIFY(top.anchors[2].x() < top.anchors[1].x());
    QVERIFY(top.anchors[2].y() < top.anchors[5].y());
    QVERIFY(top.anchors[5].y() < top.anchors[6].y());
    for (int n : {0, 1, 2, 5, 6})
      QCOMPARE(top.buttonAt(top.anchors[n]), n);
    auto left =
        model.render({720, 640}, 1, 90, 12, 1, -1, QColor("#159bff"), false);
    std::set<int> leftButtons(left.buttons.begin(), left.buttons.end());
    for (int n : {3, 4, 7})
      QVERIFY2(leftButtons.contains(n),
               qPrintable(QString("Missing side button %1").arg(n)));
    auto right =
        model.render({720, 640}, 1, -90, 12, 1, -1, QColor("#159bff"), false);
    std::set<int> rightButtons(right.buttons.begin(), right.buttons.end());
    // Hidden physical side buttons must not be selectable through the body.
    for (int n : {3, 4, 7})
      QVERIFY(!rightButtons.contains(n));
    QCOMPARE(right.buttonAt({-10, 0}), -1);
    QCOMPARE(right.buttonAt({721, 0}), -1);
    auto hidpi =
        model.render({1440, 1280}, 2, 0, 90, 1, -1, QColor("#159bff"), false);
    QCOMPARE(hidpi.image.devicePixelRatio(), 2.0);
    QCOMPARE(hidpi.buttonAt(hidpi.anchors[5]), 5);
    auto behind =
        model.render({720, 640}, 1, 0, -60, 1, -1, QColor("#159bff"), false);
    for (int n : {0, 1, 2, 5, 6})
      QVERIFY(std::find(behind.buttons.begin(), behind.buttons.end(), n) ==
              behind.buttons.end());
    // A visible side callout reaches the disabled fire editor without
    // bypassing protocol protections or writing any mock setting.
    QTemporaryDir dir;
    qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
    Window w(true);
    w.show();
    QTRY_VERIFY(w.ready());
    QTRY_VERIFY(!w.busy);
    auto image = w.device.mockImage;
    w.findChild<QPushButton *>("three-view")->click();
    auto *camera = w.findChild<QComboBox *>("model-angle");
    QVERIFY(camera);
    camera->setCurrentIndex(2);
    QTest::qWait(30);
    QTest::mouseClick(w.viewer, Qt::LeftButton, Qt::NoModifier,
                      w.viewer->calloutCenter(7).toPoint());
    QCOMPARE(w.assignmentStack->currentIndex(), 7);
    QVERIFY(!w.findChild<QComboBox *>("button8")->isEnabled());
    QCOMPARE(w.device.mockImage, image);
    w.close();
    QTRY_VERIFY(!w.busy);
  }
  void essentialControlsFitCompactWindow() {
    QTemporaryDir dir;
    qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
    Window w(true);
    w.resize(1040, 720);
    w.show();
    QTRY_VERIFY(w.ready());
    QTRY_VERIFY(!w.busy);
    auto visibleInPage = [&](QWidget *control) {
      auto *page = qobject_cast<QScrollArea *>(w.tabs->currentWidget());
      QRect area(control->mapTo(page->viewport(), QPoint(0, 0)),
                 control->size());
      return control->isVisible() && page->viewport()->rect().contains(area);
    };
    w.selectButton(7);
    QTest::qWait(30);
    QVERIFY(visibleInPage(w.assignmentPanel));
    auto *customize = qobject_cast<QScrollArea *>(w.tabs->currentWidget());
    QCOMPARE(customize->verticalScrollBar()->maximum(), 0);
    w.navigation[1]->click();
    QTest::qWait(30);
    for (auto *control :
         {static_cast<QWidget *>(w.polling), static_cast<QWidget *>(w.timing),
          static_cast<QWidget *>(w.dpiSlider)})
      QVERIFY(visibleInPage(control));
    w.navigation[2]->click();
    QTest::qWait(30);
    QVERIFY(visibleInPage(w.lightSummary));
    QVERIFY(visibleInPage(w.liveSpeed));
    w.navigation[3]->click();
    QTest::qWait(30);
    for (auto *control : {static_cast<QWidget *>(w.previewButton),
                          static_cast<QWidget *>(w.macroApplyButton),
                          static_cast<QWidget *>(w.macroStopButton),
                          static_cast<QWidget *>(w.macroNote)})
      QVERIFY(visibleInPage(control));
    auto *macros = qobject_cast<QScrollArea *>(w.tabs->currentWidget());
    QCOMPARE(macros->verticalScrollBar()->maximum(), 0);
    w.resize(1040, 680);
    QTest::qWait(30);
    auto *footer = w.findChild<QFrame *>("footer");
    QVERIFY(footer);
    QVERIFY(w.rect().contains(
        QRect(footer->mapTo(&w, QPoint(0, 0)), footer->size())));
    w.close();
    QTRY_VERIFY(!w.busy);
  }
};
} // namespace mouse
QTEST_MAIN(mouse::WindowTest)
#include "window_test.moc"
