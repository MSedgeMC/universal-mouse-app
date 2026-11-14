#include "window.h"
#include "core/storage.h"
#include "design.h"
#include <QAction>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStyle>
#include <QTabBar>
#include <QVBoxLayout>
#include <QtConcurrent>
namespace mouse {
static QPushButton *button(const QString &text, QLayout *layout = nullptr) {
  auto *b = new QPushButton(text);
  if (layout)
    layout->addWidget(b);
  return b;
}
QJsonObject Window::profile() const {
  return profiles[profileIndex].toObject();
}
void Window::setProfile(QJsonObject p) { profiles[profileIndex] = p; }
Window::Window(bool mock) : device(mock), integration(mock) {
  setWindowTitle("Universal Mouse App — GM320");
  resize(1280, 820);
  setMinimumSize(1040, 680);
  for (int i = 0; i < 4; ++i)
    profiles.append(defaultProfile());
  try {
    QString file = configDir() + "/profiles.json";
    if (QFileInfo::exists(file)) {
      haveSavedProfiles = true;
      profiles = importProfiles(readJson(file).toObject());
    }
  } catch (const std::exception &e) {
    QTimer::singleShot(0, this, [this, e = QString::fromUtf8(e.what())] {
      message("Saved profiles could not be loaded: " + e, true);
    });
  }
  saved = profiles;
  build();
  connect(&watcher, &QFutureWatcher<QJsonObject>::finished, this, [this] {
    auto r = watcher.result();
    busy = false;
    bool error = r.contains("error");
    auto done = std::move(completed);
    completed = {};
    try {
      if (error) {
        revisionToken.clear();
        message(r["error"].toString() +
                    " Reload mouse settings before another hardware write.",
                true);
      } else if (done)
        done(r);
    } catch (const std::exception &e) {
      message(e.what(), true);
    }
    dirty();
    if (closeRequested) {
      closeRequested = false;
      close();
    }
  });
  render();
  QTimer::singleShot(0, this, [this] { loadMouse(); });
}
Window::~Window() { watcher.waitForFinished(); }
void Window::enqueue(std::function<QJsonObject()> work,
                     std::function<void(QJsonObject)> done) {
  if (busy) {
    message("Wait for the current operation to finish");
    return;
  }
  busy = true;
  completed = std::move(done);
  dirty();
  watcher.setFuture(QtConcurrent::run([work = std::move(work)] {
    try {
      return work();
    } catch (const std::exception &e) {
      return QJsonObject{{"error", QString::fromUtf8(e.what())}};
    }
  }));
}
void Window::message(const QString &text, bool error) {
  status->setText(text);
  status->setStyleSheet(error ? "color:#ffabb3" : "color:#b6cadb");
}
void Window::build() {
  auto *center = new QWidget;
  center->setObjectName("app-root");
  setCentralWidget(center);
  auto *layout = new QVBoxLayout(center);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto *appBar = new QFrame;
  appBar->setObjectName("app-bar");
  auto *appRow = new QHBoxLayout(appBar);
  appRow->setContentsMargins(26, 0, 26, 0);
  appRow->setSpacing(12);
  auto *mark = new QLabel;
  mark->setPixmap(designIcon("brand", QColor("#159bff"), 22).pixmap(22, 22));
  appRow->addWidget(mark);
  appRow->addWidget(designLabel("UNIVERSAL", "brand"));
  auto *divider = new QFrame;
  divider->setObjectName("brand-divider");
  divider->setFixedSize(1, 18);
  appRow->addWidget(divider);
  appRow->addWidget(designLabel("MOUSE", "app-section"));
  appRow->addStretch();
  appRow->addWidget(designLabel("LINUX  /  0.3.0", "micro"));
  layout->addWidget(appBar);

  auto *deviceBar = new QFrame;
  deviceBar->setObjectName("device-bar");
  auto *deviceRow = new QHBoxLayout(deviceBar);
  deviceRow->setContentsMargins(27, 10, 27, 10);
  deviceRow->setSpacing(10);
  auto *profileIcon = new QLabel;
  profileIcon->setPixmap(
      designIcon("layers", QColor("#91949c"), 20).pixmap(20, 20));
  deviceRow->addWidget(profileIcon);
  profileCombo = new QComboBox;
  profileCombo->setObjectName("profile");
  profileCombo->addItems({"Office", "Game I", "Game II", "Game III"});
  profileCombo->setFixedWidth(167);
  profileCombo->setToolTip(
      "Local preset. Switching presets does not write the mouse.");
  deviceRow->addWidget(profileCombo);
  profileMenu = new QToolButton;
  profileMenu->setObjectName("profile-menu");
  profileMenu->setIcon(designIcon("more"));
  profileMenu->setAccessibleName("Preset import and export");
  profileMenu->setProperty("role", "icon");
  profileMenu->setPopupMode(QToolButton::InstantPopup);
  auto *menu = new QMenu(profileMenu);
  connect(menu->addAction("Import presets…"), &QAction::triggered, this,
          &Window::importFile);
  connect(menu->addAction("Export presets…"), &QAction::triggered, this,
          &Window::exportFile);
  profileMenu->setMenu(menu);
  deviceRow->addWidget(profileMenu);
  deviceRow->addStretch(1);
  auto *deviceTitle = designLabel("ANT ESPORTS  GM320", "device-title");
  deviceRow->addWidget(deviceTitle);
  deviceRow->addStretch(1);
  connection =
      designLabel(device.mock ? "●  DEMO DEVICE" : "CONNECTING…", "connection");
  connection->setMinimumWidth(94);
  deviceRow->addWidget(connection);
  refreshButton = button("Sync device", deviceRow);
  refreshButton->setProperty("role", "quiet");
  refreshButton->setIcon(designIcon("refresh"));
  connect(refreshButton, &QPushButton::clicked, this, &Window::loadMouse);
  layout->addWidget(deviceBar);

  auto *navBar = new QFrame;
  navBar->setObjectName("navigation-bar");
  auto *navRow = new QHBoxLayout(navBar);
  navRow->setContentsMargins(24, 6, 24, 11);
  navRow->setSpacing(9);
  auto *profileInfo = new QWidget;
  profileInfo->setFixedWidth(235);
  auto *pi = new QHBoxLayout(profileInfo);
  pi->setContentsMargins(0, 0, 0, 0);
  presetState = designLabel("LOCAL PRESET · SAVED", "micro");
  pi->addWidget(presetState);
  navRow->addWidget(profileInfo);
  navRow->addStretch();
  auto *navGroup = new QButtonGroup(this);
  navGroup->setExclusive(true);
  const QStringList names = {"CUSTOMIZE", "PERFORMANCE", "LIGHTING", "MACROS",
                             "DESKTOP"};
  for (int n = 0; n < names.size(); ++n) {
    auto *nav = button(names[n], navRow);
    nav->setProperty("role", "nav");
    nav->setFixedHeight(36);
    nav->setCheckable(true);
    nav->setObjectName("nav-" + QString::number(n));
    navigation << nav;
    navGroup->addButton(nav, n);
  }
  navRow->addStretch();
  auto *wired = designLabel("8 BUTTONS  ·  WIRED", "micro");
  wired->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  wired->setFixedWidth(135);
  navRow->addWidget(wired);
  layout->addWidget(navBar);

  tabs = new QTabWidget;
  tabs->setObjectName("tabs");
  tabs->tabBar()->hide();
  layout->addWidget(tabs, 1);
  buildCustomize();
  buildPerformance();
  buildLighting();
  buildMacros();
  buildDesktop();

  auto *footerCard = new QFrame;
  footerCard->setObjectName("footer");
  auto *footer = new QHBoxLayout(footerCard);
  footer->setContentsMargins(26, 14, 26, 14);
  footer->setSpacing(9);
  restoreButton = button("Restore", footer);
  restoreButton->setProperty("role", "quiet");
  restoreButton->setIcon(designIcon("restore"));
  restoreButton->setToolTip(
      "Restore only fields changed by the last verified hardware transaction.");
  status = designLabel("Connecting to your mouse…", "muted");
  status->setMinimumWidth(140);
  footer->addWidget(status, 1);
  undoButton = button("Discard", footer);
  undoButton->setProperty("role", "quiet");
  saveButton = button("Save preset", footer);
  applyButton = button("Apply to mouse", footer);
  applyButton->setObjectName("apply-mouse");
  applyButton->setProperty("role", "primary");
  layout->addWidget(footerCard);
  connect(saveButton, &QPushButton::clicked, this, [this] {
    try {
      saveProfiles();
      message("Preset saved locally; mouse settings unchanged");
    } catch (const std::exception &e) {
      message(e.what(), true);
    }
  });
  connect(undoButton, &QPushButton::clicked, this, [this] {
    profiles[profileIndex] = saved[profileIndex];
    stopPreview();
    render();
    dirty();
  });
  connect(applyButton, &QPushButton::clicked, this, &Window::hardwareApply);
  connect(restoreButton, &QPushButton::clicked, this, [this] {
    QString rev = revisionToken;
    enqueue(
        [this, rev] {
          return device.operation("restore", {{"expected_revision", rev}});
        },
        [this](auto r) {
          current = r["settings"].toObject();
          revisionToken = r["revision"].toString();
          auto p = profile();
          p["settings"] = current;
          setProfile(p);
          baseline = p;
          restoreButton->setProperty("canRestore", r["can_restore"].toBool());
          render();
          message("Last changed fields restored and read back");
        });
  });
  connect(profileCombo, &QComboBox::currentIndexChanged, this, [this](int n) {
    if (rendering)
      return;
    profileIndex = n;
    stopPreview();
    render();
    dirty();
  });

  connect(navGroup, &QButtonGroup::idClicked, this,
          [this](int n) { tabs->setCurrentIndex(n); });
  connect(tabs, &QTabWidget::currentChanged, this, [this](int n) {
    navigation[n]->setChecked(true);
    assignmentPanel->hide();
    stopPreview();
  });
  navigation[0]->setChecked(true);
  applyDesign(this);
}
void Window::collect() {
  if (rendering)
    return;
  auto p = profile(), s = p["settings"].toObject(),
       l = s["lighting"].toObject();
  for (int n = 0; n < 7; ++n)
    s["button" + QString::number(n + 1)] = buttons[n]->currentText();
  QJsonArray vals, en;
  int active = s["stage"].toInt();
  for (int n = 0; n < 6; ++n) {
    vals.append(dpi[n]->currentData().toInt());
    en.append(dpiEnabled[n]->isChecked());
    if (stage[n]->isChecked())
      active = n;
  }
  s["dpi"] = vals;
  s["enabled"] = en;
  s["stage"] = active;
  s["polling"] = polling->currentData().toInt();
  s["double_click_ms"] = timing->value();
  s["brightness_raw"] = brightness->value();
  l["effect"] = effect->currentText();
  l["speed_raw"] = speed->value();
  l["direction"] = direction->currentText();
  l["symmetry"] = symmetry->isChecked();
  QJsonArray ce, re, rs;
  for (int n = 0; n < 7; ++n) {
    ce.append(colorEnabled[n]->isChecked());
    re.append(responseEnabled[n]->isChecked());
    rs.append(response[n]->currentText());
  }
  l["enabled"] = ce;
  l["response_enabled"] = re;
  l["responses"] = rs;
  s["lighting"] = l;
  p["settings"] = s;
  p["trigger"] = trigger->currentData().toInt();
  p["live"] = liveEnabled->isChecked()
                  ? QJsonObject{{"effect", liveEffect->currentText()},
                                {"color", liveColor.name()},
                                {"speed", liveSpeed->value()}}
                  : QJsonObject();
  setProfile(p);
  syncPresentation();
  dirty();
}
void Window::render() {
  rendering = true;
  auto p = profile(), s = p["settings"].toObject(),
       l = s["lighting"].toObject();
  for (int n = 0; n < 7; ++n)
    buttons[n]->setCurrentText(s["button" + QString::number(n + 1)].toString());
  for (int n = 0; n < 6; ++n) {
    dpi[n]->setCurrentIndex(dpi[n]->findData(s["dpi"].toArray()[n].toInt()));
    dpiEnabled[n]->setChecked(s["enabled"].toArray()[n].toBool());
    stage[n]->setChecked(s["stage"].toInt() == n);
  }
  polling->setCurrentIndex(polling->findData(s["polling"].toInt()));
  timing->setValue(s["double_click_ms"].toInt());
  brightness->setValue(s["brightness_raw"].toInt());
  speed->setValue(l["speed_raw"].toInt());
  if (effect->findText(l["effect"].toString()) < 0)
    effect->addItem(l["effect"].toString());
  effect->setCurrentText(l["effect"].toString());
  auto *model = qobject_cast<QStandardItemModel *>(effect->model());
  for (int n = 0; n < effect->count(); ++n)
    model->item(n)->setEnabled(effects.contains(effect->itemText(n)));
  direction->setCurrentText(l["direction"].toString());
  symmetry->setChecked(l["symmetry"].toBool());
  for (int n = 0; n < 7; ++n) {
    auto a = l["palette"].toArray()[n].toArray();
    QColor c((15 - a[0].toInt()) * 17, (15 - a[1].toInt()) * 17,
             (15 - a[2].toInt()) * 17);
    colors[n]->setText(c.name());
    colors[n]->setStyleSheet("background:" + c.name() + ";color:" +
                             (c.lightness() > 130 ? "#111" : "#fff"));
    colorEnabled[n]->setChecked(l["enabled"].toArray()[n].toBool());
    response[n]->setCurrentText(l["responses"].toArray()[n].toString());
    responseEnabled[n]->setChecked(l["response_enabled"].toArray()[n].toBool());
  }
  auto live = p["live"].toObject();
  liveEnabled->setChecked(!live.isEmpty());
  if (!live.isEmpty()) {
    liveEffect->setCurrentText(live["effect"].toString());
    liveSpeed->setValue(live["speed"].toInt());
    liveColor = QColor(live["color"].toString());
  }
  trigger->setCurrentIndex(trigger->findData(p["trigger"].toInt()));
  events->clear();
  for (auto v : p["macro"].toArray()) {
    auto e = v.toObject();
    auto *item = new QListWidgetItem(
        e["type"] == "delay"
            ? QString("Wait %1 ms").arg(e["ms"].toInt())
            : e["key"].toString() + " · key " + e["type"].toString(),
        events);
    item->setData(Qt::UserRole, e["type"].toString());
    item->setData(Qt::UserRole + 1, e["key"].toString());
    item->setData(Qt::UserRole + 2, e["ms"].toInt());
  }
  syncPresentation();
  rendering = false;
  dirty();
}
void Window::dirty() {
  bool changed = profile() != saved[profileIndex].toObject();
  saveButton->setEnabled(!busy && changed);
  undoButton->setEnabled(!busy && changed);
  bool hardware = false;
  if (!current.isEmpty())
    try {
      hardware = !hardwareChanges(profile(), current).isEmpty() ||
                 profile()["live"] != baseline["live"];
    } catch (...) {
      hardware = true;
    }
  applyButton->setEnabled(!busy && !revisionToken.isEmpty() && hardware);
  restoreButton->setEnabled(!busy && !revisionToken.isEmpty() &&
                            restoreButton->property("canRestore").toBool());
  refreshButton->setEnabled(!busy);
  profileCombo->setEnabled(!busy);
  profileMenu->setEnabled(!busy);
  tabs->setEnabled(!busy);
  for (auto *nav : navigation)
    nav->setEnabled(!busy);
  presetState->setText(changed ? "LOCAL PRESET · EDITED"
                               : "LOCAL PRESET · SAVED");
  macroApplyButton->setEnabled(!busy &&
                               !profile()["macro"].toArray().isEmpty());
  macroStopButton->setEnabled(!busy && !integration.activeGroup.isEmpty());
  previewButton->setEnabled(!busy && !profile()["macro"].toArray().isEmpty());
}
void Window::saveProfiles() {
  validateProfile(profile());
  auto next = saved;
  next[profileIndex] = profile();
  saveJson(configDir() + "/profiles.json",
           QJsonObject{{"version", 2}, {"profiles", next}});
  saved = next;
  haveSavedProfiles = true;
  dirty();
}
void Window::loadMouse() {
  enqueue([this] { return device.operation("read", {}); },
          [this](auto r) {
            auto fresh = r["settings"].toObject();
            if (firstRead) {
              if (!haveSavedProfiles) {
                for (int n = 0; n < profiles.size(); ++n) {
                  auto p = profiles[n].toObject();
                  p["settings"] = fresh;
                  profiles[n] = p;
                }
                saved = profiles;
              }
              baseline = QJsonObject{{"live", QJsonObject()}};
              firstRead = false;
            } else {
              auto p = profile();
              p["settings"] =
                  rebaseSettings(p["settings"].toObject(), current, fresh);
              setProfile(p);
            }
            current = fresh;
            revisionToken = r["revision"].toString();
            baseline["settings"] = fresh;
            restoreButton->setProperty("canRestore", r["can_restore"].toBool());
            connection->setText(device.mock ? "●  DEMO DEVICE"
                                            : "●  CONNECTED");
            render();
            message("Mouse synced. Changes require Apply.");
            QTimer::singleShot(0, this, &Window::refreshDesktop);
          });
}
void Window::hardwareApply() {
  QJsonObject p = profile(), changes;
  try {
    changes = hardwareChanges(p, current);
    validateProfile(p);
    if (p["live"].toObject().isEmpty() &&
        !baseline["live"].toObject().isEmpty()) {
      auto light = changes["lighting"].toObject();
      QString stored =
          p["settings"].toObject()["lighting"].toObject()["effect"].toString();
      require(effects.contains(stored),
              "Choose a supported stored effect to clear the live override");
      light["effect"] = stored;
      changes["lighting"] = light;
    }
    if (!p["live"].toObject().isEmpty())
      rgbReports(p["live"].toObject());
  } catch (const std::exception &e) {
    message(e.what(), true);
    return;
  }
  QString rev = revisionToken;
  auto live = p["live"].toObject();
  bool needsLive =
      !live.isEmpty() && (!changes.isEmpty() || p["live"] != baseline["live"]);
  enqueue(
      [this, changes, rev, live, needsLive] {
        QJsonObject r;
        if (!changes.isEmpty())
          r = device.operation(
              "apply", {{"expected_revision", rev}, {"changes", changes}});
        else
          r = device.operation("read", {});
        if (needsLive) {
          try {
            device.operation("rgb", live);
            r["live_sent"] = true;
          } catch (const std::exception &e) {
            r["live_error"] = QString::fromUtf8(e.what());
          }
        }
        return r;
      },
      [this, p](auto r) {
        current = r["settings"].toObject();
        revisionToken = r["revision"].toString();
        auto next = p;
        next["settings"] = current;
        if (p["live"].toObject()["effect"] == "Off") {
          auto settings = next["settings"].toObject(),
               light = settings["lighting"].toObject();
          auto wanted = p["settings"].toObject();
          light["response_enabled"] =
              wanted["lighting"].toObject()["response_enabled"];
          light["effect"] = wanted["lighting"].toObject()["effect"];
          settings["lighting"] = light;
          settings["brightness_raw"] = wanted["brightness_raw"];
          next["settings"] = settings;
        }
        setProfile(next);
        baseline["settings"] = current;
        if (!r.contains("live_error"))
          baseline["live"] = next["live"];
        restoreButton->setProperty("canRestore", r["can_restore"].toBool());
        QString saveError;
        try {
          saveProfiles();
        } catch (const std::exception &e) {
          saveError = QString(" Hardware read back, but local save failed: ") +
                      e.what();
        }
        render();
        message(r.contains("live_error")
                    ? "Hardware settings read back; live override failed: " +
                          r["live_error"].toString()
                : r["live_sent"].toBool()
                    ? "Hardware read back. Live color sent; physical effect is "
                      "unverified."
                    : "Mouse settings written and read back.",
                r.contains("live_error"));
        if (!saveError.isEmpty())
          message(status->text() + saveError, true);
      });
}
void Window::refreshDesktop() {
  if (busy)
    return;
  enqueue([this] { return integration.desktopRead(); },
          [this](auto r) {
            desktop = r;
            gain->setValue(r["gain"].toDouble());
            scroll->setValue(r["scroll"].toDouble());
            gain->setEnabled(r["pointer"].toBool());
            scroll->setEnabled(r["wheel"].toBool());
            wheelEnabled->setVisible(!r["native_wheel"].toBool());
            wheelEnabled->setChecked(r["wheel_enabled"].toBool());
            scroll->setMaximum(r["native_wheel"].toBool() ? 2.5 : 11);
            desktopNote->setText(r["note"].toString() + " " +
                                 r["wheel_note"].toString());
          });
}
void Window::importFile() {
  QString path = QFileDialog::getOpenFileName(this, "Import four local presets",
                                              {}, "JSON presets (*.json)");
  if (path.isEmpty())
    return;
  try {
    QFile f(path);
    require(f.open(QIODevice::ReadOnly) && f.size() <= 100000,
            "Cannot read preset file or file is too large");
    QJsonParseError error;
    auto d = QJsonDocument::fromJson(f.readAll(), &error);
    require(error.error == QJsonParseError::NoError && d.isObject(),
            "Invalid preset JSON");
    profiles = importProfiles(d.object());
    haveSavedProfiles = true;
    stopPreview();
    render();
    message("Presets imported and staged. Save/Apply remain separate.");
  } catch (const std::exception &e) {
    message(e.what(), true);
  }
}
void Window::exportFile() {
  QString path = QFileDialog::getSaveFileName(this, "Export local presets",
                                              "mouse-presets.json",
                                              "JSON presets (*.json)");
  if (path.isEmpty())
    return;
  try {
    for (auto p : profiles)
      validateProfile(p.toObject());
    saveJson(path, QJsonObject{{"version", 2}, {"profiles", profiles}});
    message("Presets exported");
  } catch (const std::exception &e) {
    message(e.what(), true);
  }
}
void Window::stopPreview() {
  previewTimer.stop();
  events->clearSelection();
}
void Window::closeEvent(QCloseEvent *e) {
  if (shutdownComplete) {
    e->accept();
    return;
  }
  if (busy) {
    closeRequested = true;
    e->ignore();
    message("Waiting for the current operation before closing…");
    return;
  }
  if (profiles != saved) {
    auto answer = QMessageBox::question(
        this, "Unsaved local presets", "Discard unsaved local preset edits?",
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Discard) {
      e->ignore();
      return;
    }
  }
  stopPreview();
  enqueue(
      [this] {
        integration.close();
        return QJsonObject{{"closed", true}};
      },
      [this](auto) {
        profiles = saved;
        shutdownComplete = true;
        close();
      });
  e->ignore();
}
bool Window::verifyGui(const QString &directory) {
  try {
    require(qEnvironmentVariable("KUBUNTU_VIRTUAL_SCREEN") == "1" ||
                qEnvironmentVariable("UMA_ISOLATED_TEST") == "1",
            "GUI verification requires isolated screen");
    QDir().mkpath(directory);
    auto writeImage = [&](int i, QString name) {
      tabs->setCurrentIndex(i);
      QCoreApplication::processEvents();
      require(grab().save(directory + "/" + name + ".png"),
              "Cannot save screenshot");
    };
    writeImage(0, "buttons");
    writeImage(1, "sensitivity");
    writeImage(2, "lighting");
    writeImage(3, "macros");
    writeImage(4, "desktop");
    tabs->setCurrentIndex(2);
    auto *lightingScroll = qobject_cast<QScrollArea *>(tabs->widget(2));
    lightingScroll->verticalScrollBar()->setValue(
        lightingScroll->verticalScrollBar()->maximum());
    writeImage(2, "lighting-palette");
    lightingScroll->verticalScrollBar()->setValue(0);
    tabs->setCurrentIndex(0);
    selectButton(7);
    writeImage(0, "fire-button");
    selectButton(0);
    writeImage(0, "assignment-editor");
    assignmentPanel->hide();
    auto p = profile();
    p["macro"] = QJsonArray{QJsonObject{{"type", "down"}, {"key", "Ctrl"}},
                            QJsonObject{{"type", "down"}, {"key", "C"}},
                            QJsonObject{{"type", "delay"}, {"ms", 100}},
                            QJsonObject{{"type", "up"}, {"key", "C"}},
                            QJsonObject{{"type", "up"}, {"key", "Ctrl"}}};
    setProfile(p);
    render();
    saveProfiles();
    require(events->count() == 5, "Macro timeline failed");
    require(macroExpression(p["macro"].toArray()) ==
                "key_down(KEY_LEFTCTRL).key_down(KEY_C).wait(100).key_up(KEY_C)"
                ".key_up(KEY_LEFTCTRL)",
            "Macro export failed");
    findChild<QPushButton *>("three-view")->click();
    writeImage(0, "viewer-3d");
    writeImage(3, "macro-saved");
    resize(1040, 720);
    findChild<QPushButton *>("top-view")->click();
    writeImage(0, "compact-assignments");
    selectButton(7);
    writeImage(0, "compact-editor");
    assignmentPanel->hide();
    writeImage(1, "compact-performance");
    writeImage(2, "compact-lighting");
    lightingScroll->verticalScrollBar()->setValue(
        lightingScroll->verticalScrollBar()->maximum());
    writeImage(2, "compact-palette");
    lightingScroll->verticalScrollBar()->setValue(0);
    writeImage(3, "compact-macros");
    writeImage(4, "compact-desktop");
    resize(1280, 820);
    QFile f(directory + "/result.json");
    require(f.open(QIODevice::WriteOnly), "Cannot save GUI result");
    f.write(QJsonDocument(QJsonObject{{"ready", ready()},
                                      {"native", true},
                                      {"mock", device.mock},
                                      {"macro_events", 5},
                                      {"hardware_writes", 0},
                                      {"input_injection", 0}})
                .toJson());
    return true;
  } catch (const std::exception &e) {
    message(e.what(), true);
    return false;
  }
}
} // namespace mouse
