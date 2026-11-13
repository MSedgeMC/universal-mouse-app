#include "window.h"
#include "core/storage.h"
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
#include <QMessageBox>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QtConcurrent>
namespace mouse {
static QLabel *label(const QString &text) {
  auto *l = new QLabel(text);
  l->setWordWrap(true);
  return l;
}
static QPushButton *button(const QString &text, QLayout *layout = nullptr) {
  auto *b = new QPushButton(text);
  if (layout)
    layout->addWidget(b);
  return b;
}
static QSpinBox *spin(int lo, int hi) {
  auto *s = new QSpinBox;
  s->setRange(lo, hi);
  return s;
}
QJsonObject Window::profile() const {
  return profiles[profileIndex].toObject();
}
void Window::setProfile(QJsonObject p) { profiles[profileIndex] = p; }
Window::Window(bool mock) : device(mock), integration(mock) {
  setWindowTitle("Universal Mouse App — GM320");
  resize(1120, 800);
  setMinimumSize(900, 680);
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
  setCentralWidget(center);
  auto *layout = new QVBoxLayout(center);
  layout->setContentsMargins(24, 20, 24, 20);
  layout->setSpacing(14);
  auto *head = new QHBoxLayout;
  auto *title = new QLabel("Universal Mouse App");
  title->setStyleSheet("font-size:25px;font-weight:600");
  head->addWidget(title);
  head->addStretch();
  connection = label(device.mock ? "Demo · no device writes"
                                 : "Searching for a supported mouse");
  head->addWidget(connection);
  refreshButton = button("Read mouse", head);
  connect(refreshButton, &QPushButton::clicked, this, &Window::loadMouse);
  layout->addLayout(head);
  auto *profileRow = new QHBoxLayout;
  profileRow->addWidget(new QLabel("Local preset"));
  profileCombo = new QComboBox;
  profileCombo->addItems({"Office", "Game I", "Game II", "Game III"});
  profileCombo->setObjectName("profile");
  profileRow->addWidget(profileCombo);
  profileRow->addStretch();
  connect(button("Import presets", profileRow), &QPushButton::clicked, this,
          &Window::importFile);
  connect(button("Export presets", profileRow), &QPushButton::clicked, this,
          &Window::exportFile);
  layout->addLayout(profileRow);
  tabs = new QTabWidget;
  tabs->setObjectName("tabs");
  layout->addWidget(tabs, 1);
  auto addPage = [&](QString name) {
    auto *content = new QWidget;
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    tabs->addTab(scroll, name);
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(20, 20, 20, 20);
    return v;
  };
  auto *bv = addPage("Buttons");
  auto *bh = new QHBoxLayout;
  auto *viewColumn = new QVBoxLayout;
  viewer = new Viewer;
  viewColumn->addWidget(viewer, 1);
  auto *viewControls = new QHBoxLayout;
  auto *three = new QCheckBox("3D view");
  viewControls->addWidget(three);
  auto *rotate = new QCheckBox("Auto rotate");
  viewControls->addWidget(rotate);
  connect(rotate, &QCheckBox::toggled, viewer, &Viewer::setAutoRotate);
  auto *test = new QCheckBox("Test inputs");
  viewControls->addWidget(test);
  connect(test, &QCheckBox::toggled, viewer, &Viewer::setTestEnabled);
  connect(three, &QCheckBox::toggled, viewer, &Viewer::setThreeD);
  connect(button("Reset view", viewControls), &QPushButton::clicked, viewer,
          &Viewer::resetView);
  viewColumn->addLayout(viewControls);
  tester = label("Click or scroll inside the preview to test events.");
  viewColumn->addWidget(tester);
  connect(viewer, &Viewer::tested, tester, &QLabel::setText);
  bh->addLayout(viewColumn, 1);
  auto *assign = new QFormLayout;
  for (int i = 0; i < 7; ++i) {
    auto *c = new QComboBox;
    c->addItems(mouse::actions);
    c->setObjectName("button" + QString::number(i + 1));
    buttons << c;
    assign->addRow(QString::number(i + 1) + " · " + buttonNames[i], c);
    connect(c, &QComboBox::currentTextChanged, this, [this, i] {
      if (!rendering) {
        viewer->setSelected(i);
        collect();
      }
    });
  }
  bh->addLayout(assign, 1);
  bv->addLayout(bh, 1);
  bv->addWidget(label("Seven stored button slots · exact physical mapping and "
                      "most writes remain experimental."));
  connect(viewer, &Viewer::selected, this,
          [this](int n) { buttons[n]->setFocus(); });
  auto *sv = addPage("Sensitivity");
  auto *grid = new QGridLayout;
  grid->addWidget(new QLabel("Stage"), 0, 0);
  grid->addWidget(new QLabel("Enabled"), 0, 1);
  grid->addWidget(new QLabel("DPI"), 0, 2);
  grid->addWidget(new QLabel("Active"), 0, 3);
  for (int i = 0; i < 6; ++i) {
    auto *en = new QCheckBox;
    auto *d = new QComboBox;
    for (int v : dpis)
      d->addItem(QString::number(v), v);
    auto *r = new QRadioButton;
    dpiEnabled << en;
    dpi << d;
    stage << r;
    grid->addWidget(new QLabel(QString::number(i + 1)), i + 1, 0);
    grid->addWidget(en, i + 1, 1);
    grid->addWidget(d, i + 1, 2);
    grid->addWidget(r, i + 1, 3);
    connect(en, &QCheckBox::toggled, this, [this] { collect(); });
    connect(d, &QComboBox::currentIndexChanged, this, [this] { collect(); });
    connect(r, &QRadioButton::toggled, this, [this] { collect(); });
  }
  sv->addLayout(grid);
  auto *sf = new QFormLayout;
  polling = new QComboBox;
  for (int rate : {125, 250, 500, 1000})
    polling->addItem(QString::number(rate) + " Hz", rate);
  sf->addRow("Polling choice", polling);
  timing = spin(0, 65535);
  timing->setSuffix(" ms");
  sf->addRow("Device click timing", timing);
  sv->addLayout(sf);
  sv->addWidget(label(
      "Polling is a stored choice, not a measured report rate. Device timing "
      "edits are limited to 8–300 ms; existing values are preserved."));
  connect(polling, &QComboBox::currentIndexChanged, this,
          [this] { collect(); });
  connect(timing, &QSpinBox::valueChanged, this, [this] { collect(); });
  auto *desktopBox = new QGroupBox("Linux desktop pointer and scrolling");
  auto *df = new QFormLayout(desktopBox);
  gain = new QDoubleSpinBox;
  gain->setRange(-1, 1);
  gain->setSingleStep(.2);
  scroll = new QDoubleSpinBox;
  scroll->setRange(.25, 11);
  scroll->setSingleStep(.25);
  wheelEnabled = new QCheckBox("Enable X11 wheel helper");
  df->addRow("Pointer acceleration", gain);
  df->addRow("Scroll factor", scroll);
  df->addRow(wheelEnabled);
  desktopNote = label("Loading desktop controls…");
  df->addRow(desktopNote);
  auto *dr = new QHBoxLayout;
  connect(button("Reload desktop", dr), &QPushButton::clicked, this,
          &Window::refreshDesktop);
  connect(button("Apply desktop settings", dr), &QPushButton::clicked, this,
          [this] {
            QString rev = desktop["revision"].toString();
            double g = gain->value(), s = scroll->value();
            bool en = wheelEnabled->isChecked();
            enqueue([this, rev, g, s,
                     en] { return integration.desktopApply(rev, g, s, en); },
                    [this](auto r) {
                      desktop = r;
                      message("Desktop settings applied and read back");
                      refreshDesktop();
                    });
          });
  df->addRow(dr);
  sv->addWidget(desktopBox);
  sv->addStretch();
  auto *lv = addPage("Lighting");
  lightingViewer = new Viewer;
  lightingViewer->setMaximumHeight(340);
  lv->addWidget(lightingViewer);
  lv->addWidget(
      label("Illustrative lighting preview · hardware effects may differ"));
  auto *lf = new QFormLayout;
  effect = new QComboBox;
  effect->addItems(effects);
  direction = new QComboBox;
  direction->addItems(directions);
  brightness = spin(0, 7);
  brightness->setSpecialValueText("Maximum (0)");
  speed = spin(0, 7);
  symmetry = new QCheckBox("Symmetric pattern");
  lf->addRow("Stored effect", effect);
  lf->addRow("Brightness code (0–6 editable)", brightness);
  lf->addRow("Speed code (0–5 editable)", speed);
  lf->addRow("Direction", direction);
  lf->addRow(symmetry);
  lv->addLayout(lf);
  auto *paletteGrid = new QGridLayout;
  QStringList headings = {"Slot", "Color", "Enabled", "Button response",
                          "Response on"};
  for (int n = 0; n < headings.size(); ++n)
    paletteGrid->addWidget(new QLabel(headings[n]), 0, n);
  for (int i = 0; i < 7; ++i) {
    auto *c = new QPushButton;
    auto *en = new QCheckBox;
    auto *r = new QComboBox;
    r->addItems(responses);
    auto *re = new QCheckBox;
    colors << c;
    colorEnabled << en;
    response << r;
    responseEnabled << re;
    paletteGrid->addWidget(new QLabel(QString::number(i + 1)), i + 1, 0);
    paletteGrid->addWidget(c, i + 1, 1);
    paletteGrid->addWidget(en, i + 1, 2);
    paletteGrid->addWidget(r, i + 1, 3);
    paletteGrid->addWidget(re, i + 1, 4);
    connect(c, &QPushButton::clicked, this, [this, i] {
      auto rgb = profile()["settings"]
                     .toObject()["lighting"]
                     .toObject()["palette"]
                     .toArray()[i]
                     .toArray();
      QColor initial((15 - rgb[0].toInt()) * 17, (15 - rgb[1].toInt()) * 17,
                     (15 - rgb[2].toInt()) * 17);
      QColor chosen = QColorDialog::getColor(initial, this, "Palette color");
      if (chosen.isValid()) {
        auto p = profile();
        auto s = p["settings"].toObject(), l = s["lighting"].toObject();
        auto a = l["palette"].toArray();
        a[i] = QJsonArray{15 - qRound(chosen.red() / 17.),
                          15 - qRound(chosen.green() / 17.),
                          15 - qRound(chosen.blue() / 17.)};
        l["palette"] = a;
        s["lighting"] = l;
        p["settings"] = s;
        setProfile(p);
        render();
        dirty();
      }
    });
    connect(en, &QCheckBox::toggled, this, [this] { collect(); });
    connect(r, &QComboBox::currentTextChanged, this, [this] { collect(); });
    connect(re, &QCheckBox::toggled, this, [this] { collect(); });
  }
  lv->addLayout(paletteGrid);
  for (auto *c : {effect, direction})
    connect(c, &QComboBox::currentTextChanged, this, [this] { collect(); });
  for (auto *s : {brightness, speed})
    connect(s, &QSpinBox::valueChanged, this, [this] { collect(); });
  connect(symmetry, &QCheckBox::toggled, this, [this] { collect(); });
  auto *liveBox =
      new QGroupBox("Live color override · experimental, not read back");
  auto *liveForm = new QFormLayout(liveBox);
  liveEnabled = new QCheckBox("Apply this override after hardware settings");
  liveEffect = new QComboBox;
  liveEffect->addItems({"Static", "Breathing", "Off"});
  liveSpeed = spin(0, 5);
  auto *colorButton = new QPushButton("Choose live color");
  connect(colorButton, &QPushButton::clicked, this, [this] {
    QColor c = QColorDialog::getColor(liveColor, this);
    if (c.isValid()) {
      liveColor = c;
      collect();
      viewer->setColor(c);
    }
  });
  liveForm->addRow(liveEnabled);
  liveForm->addRow("Effect", liveEffect);
  liveForm->addRow(colorButton);
  liveForm->addRow("Speed", liveSpeed);
  lv->addWidget(liveBox);
  connect(liveEnabled, &QCheckBox::toggled, this, [this] { collect(); });
  connect(liveEffect, &QComboBox::currentIndexChanged, this,
          [this] { collect(); });
  connect(liveSpeed, &QSpinBox::valueChanged, this, [this] { collect(); });
  auto *experimental =
      new QGroupBox("Advanced: vendor default lighting report");
  auto *xf = new QFormLayout(experimental);
  experimentalBrightness = spin(0, 7);
  experimentalSpeed = spin(0, 7);
  xf->addRow("Brightness", experimentalBrightness);
  xf->addRow("Speed", experimentalSpeed);
  auto *xb = button("Apply experimental report");
  xf->addRow(xb);
  xf->addRow(label("One captured report; effect/persistence unverified. "
                   "Current lighting is not backed up by this operation."));
  connect(xb, &QPushButton::clicked, this, [this] {
    auto request = QJsonObject{{"brightness", experimentalBrightness->value()},
                               {"speed", experimentalSpeed->value()}};
    enqueue(
        [this, request] { return device.operation("experimental", request); },
        [this](auto) {
          message("Kernel accepted experimental report; physical effect "
                  "unverified");
        });
  });
  lv->addWidget(experimental);
  lv->addStretch();
  auto *mv = addPage("Macros");
  mv->addWidget(label("Linux software macros · firmware-only DPI/light buttons "
                      "cannot trigger them. Presets are not stored onboard."));
  events = new QListWidget;
  events->setObjectName("macro-events");
  mv->addWidget(events, 1);
  auto *er = new QHBoxLayout;
  eventType = new QComboBox;
  eventType->addItems({"Key down", "Key up", "Delay"});
  eventKey = new QComboBox;
  eventKey->addItems(macroKeys);
  eventDelay = spin(10, 10000);
  eventDelay->setValue(100);
  eventDelay->setSuffix(" ms");
  er->addWidget(eventType);
  er->addWidget(eventKey);
  er->addWidget(eventDelay);
  connect(button("Add event", er), &QPushButton::clicked, this, [this] {
    auto p = profile();
    auto a = p["macro"].toArray();
    if (a.size() >= 64) {
      message("Limit is 64 events", true);
      return;
    }
    a.append(eventType->currentIndex() == 2
                 ? QJsonObject{{"type", "delay"}, {"ms", eventDelay->value()}}
                 : QJsonObject{
                       {"type", eventType->currentIndex() == 0 ? "down" : "up"},
                       {"key", eventKey->currentText()}});
    p["macro"] = a;
    setProfile(p);
    stopPreview();
    render();
    dirty();
  });
  connect(button("Remove selected", er), &QPushButton::clicked, this, [this] {
    auto p = profile();
    auto a = p["macro"].toArray();
    if (events->currentRow() >= 0)
      a.removeAt(events->currentRow());
    p["macro"] = a;
    setProfile(p);
    stopPreview();
    render();
    dirty();
  });
  connect(button("Clear", er), &QPushButton::clicked, this, [this] {
    auto p = profile();
    p["macro"] = QJsonArray();
    setProfile(p);
    stopPreview();
    render();
    dirty();
  });
  mv->addLayout(er);
  auto *mr = new QHBoxLayout;
  trigger = new QComboBox;
  trigger->addItem("Back / side button", 275);
  trigger->addItem("Forward / side button", 276);
  trigger->addItem("Middle click", 274);
  mr->addWidget(trigger);
  macroApplyButton = button("Apply Linux macro", mr);
  macroStopButton = button("Stop Linux macro", mr);
  previewButton = button("Preview sequence", mr);
  mv->addLayout(mr);
  macroNote = label("Input Remapper is a separate service. Normal shutdown "
                    "stops this app's macro; forced exit can leave it active.");
  mv->addWidget(macroNote);
  connect(trigger, &QComboBox::currentIndexChanged, this,
          [this] { collect(); });
  connect(macroApplyButton, &QPushButton::clicked, this, [this] {
    auto p = profile();
    int idx = profileIndex, t = p["trigger"].toInt();
    auto a = p["macro"].toArray();
    enqueue([this, idx, t, a] { return integration.macroApply(idx, t, a); },
            [this](auto r) {
              saveProfiles();
              macroNote->setText(r["note"].toString());
            });
  });
  connect(macroStopButton, &QPushButton::clicked, this, [this] {
    enqueue(
        [this] {
          integration.macroStop();
          return QJsonObject();
        },
        [this](auto) {
          macroNote->setText("Macro stopped. Normal events restored.");
        });
  });
  connect(previewButton, &QPushButton::clicked, this, [this] {
    stopPreview();
    previewAt = 0;
    previewTimer.start(10);
  });
  previewTimer.setSingleShot(true);
  connect(&previewTimer, &QTimer::timeout, this, [this] {
    auto a = profile()["macro"].toArray();
    if (previewAt >= a.size()) {
      events->clearSelection();
      return;
    }
    events->setCurrentRow(previewAt);
    auto e = a[previewAt++].toObject();
    previewTimer.start(e["type"] == "delay" ? e["ms"].toInt() : 180);
  });
  connect(tabs, &QTabWidget::currentChanged, this, [this] { stopPreview(); });
  status = label("Loading settings…");
  status->setMinimumHeight(38);
  layout->addWidget(status);
  auto *footer = new QHBoxLayout;
  restoreButton = button("Restore last hardware change", footer);
  footer->addStretch();
  undoButton = button("Undo local edits", footer);
  saveButton = button("Save preset", footer);
  applyButton = button("Apply mouse settings", footer);
  applyButton->setObjectName("apply-mouse");
  applyButton->setStyleSheet("background:#b6384b;color:white;border:0;font-"
                             "weight:600;padding:10px 22px");
  layout->addLayout(footer);
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
  setStyleSheet(
      "QMainWindow,QWidget{background:#111820;color:#e4ecf4;font-family:Ubuntu;"
      "} QGroupBox{border:1px solid "
      "#344351;border-radius:7px;margin-top:15px;padding:15px;} "
      "QGroupBox::title{subcontrol-origin:margin;left:14px;} "
      "QComboBox,QSpinBox,QDoubleSpinBox,QListWidget{background:#202c38;border:"
      "1px solid #3b4b59;border-radius:5px;padding:7px;} "
      "QPushButton{background:#263442;border:1px solid "
      "#425569;border-radius:5px;padding:8px 14px;} "
      "QPushButton:hover{background:#36495a;} "
      "QPushButton:disabled{color:#71808c;background:#18212a;} "
      "QTabWidget::pane{border:1px solid #334451;border-radius:5px;} "
      "QTabBar::tab{padding:11px 24px;background:#1b2733;} "
      "QTabBar::tab:selected{background:#334352;border-bottom:2px solid "
      "#ed6377;} QScrollArea{border:0;} QCheckBox,QRadioButton{padding:5px;} "
      "QLabel{background:transparent;}");
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
  viewer->setLighting(s, p["live"].toObject());
  lightingViewer->setLighting(s, p["live"].toObject());
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
    if (n == 0)
      viewer->setColor(c);
  }
  auto live = p["live"].toObject();
  liveEnabled->setChecked(!live.isEmpty());
  if (!live.isEmpty()) {
    liveEffect->setCurrentText(live["effect"].toString());
    liveSpeed->setValue(live["speed"].toInt());
    liveColor = QColor(live["color"].toString());
  }
  viewer->setLighting(s, live);
  lightingViewer->setLighting(s, live);
  trigger->setCurrentIndex(trigger->findData(p["trigger"].toInt()));
  events->clear();
  int n = 1;
  for (auto v : p["macro"].toArray()) {
    auto e = v.toObject();
    events->addItem(
        QString::number(n++) + "   " +
        (e["type"] == "delay"
             ? QString("Wait %1 ms").arg(e["ms"].toInt())
             : e["key"].toString() + "  ·  key " + e["type"].toString()));
  }
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
  for (auto *b : findChildren<QPushButton *>())
    if (b->text() == "Import presets" || b->text() == "Export presets")
      b->setEnabled(!busy);
  tabs->setEnabled(!busy);
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
            connection->setText(device.mock
                                    ? "Demo · no device writes"
                                    : "GM320 / Instant mouse connected");
            render();
            message("Mouse settings read. Changes require Apply.");
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
    viewer->setThreeD(true);
    writeImage(0, "viewer-3d");
    writeImage(3, "macro-saved");
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
