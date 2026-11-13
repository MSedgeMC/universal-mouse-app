#include "window.h"
#include "core/storage.h"
#include "design.h"
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
  resize(1260, 840);
  setMinimumSize(1050, 730);
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
class LevelSpin : public QSpinBox {
public:
  explicit LevelSpin(bool brightness) : brightness(brightness) {
    setRange(0, 7);
    lineEdit()->setReadOnly(true);
  }

protected:
  QString textFromValue(int value) const override {
    if (brightness)
      return value <= 6 ? QString::number(qRound((6 - value) * 100. / 6)) + " %"
                        : "Unverified value";
    const QStringList names = {"Slowest",        "Slow",          "Balanced",
                               "Quick",          "Fast",          "Fastest",
                               "Unverified (6)", "Unverified (7)"};
    return names.value(value);
  }

private:
  bool brightness;
};
void Window::build() {
  auto *center = new QWidget;
  center->setObjectName("app-root");
  setCentralWidget(center);
  auto *shell = new QHBoxLayout(center);
  shell->setContentsMargins(0, 0, 0, 0);
  shell->setSpacing(0);
  auto *sidebar = new QWidget;
  sidebar->setObjectName("sidebar");
  sidebar->setFixedWidth(220);
  auto *sidebarShell = new QVBoxLayout(sidebar);
  sidebarShell->setContentsMargins(0, 0, 0, 0);
  auto *sidebarScroll = new QScrollArea;
  sidebarScroll->setWidgetResizable(true);
  sidebarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  sidebarScroll->setObjectName("sidebar-scroll");
  auto *sidebarContent = new QWidget;
  sidebarScroll->setWidget(sidebarContent);
  sidebarShell->addWidget(sidebarScroll);
  auto *rail = new QVBoxLayout(sidebarContent);
  rail->setSizeConstraint(QLayout::SetMinimumSize);
  rail->setContentsMargins(20, 26, 20, 22);
  rail->setSpacing(12);
  auto *brand = new QHBoxLayout;
  auto *mark = new QLabel;
  mark->setFixedSize(32, 32);
  mark->setPixmap(designIcon("brand", QColor("#a9e9cc"), 30).pixmap(30, 30));
  brand->addWidget(mark);
  brand->addWidget(designLabel("UNIVERSAL", "brand"));
  rail->addLayout(brand);
  rail->addWidget(designLabel("MOUSE STUDIO", "eyebrow"));
  rail->addSpacing(25);
  auto *deviceCard = new QFrame;
  deviceCard->setObjectName("device-card");
  auto *dc = new QVBoxLayout(deviceCard);
  dc->setSizeConstraint(QLayout::SetMinimumSize);
  dc->setContentsMargins(15, 16, 15, 16);
  dc->setSpacing(8);
  dc->addWidget(designLabel("YOUR DEVICE", "eyebrow"));
  auto *deviceIcon = new QLabel;
  deviceIcon->setPixmap(
      designIcon("mouse", QColor("#a9e9cc"), 42).pixmap(42, 42));
  dc->addWidget(deviceIcon);
  dc->addWidget(designLabel("Ant Esports GM320", "section"));
  dc->addWidget(designLabel("8 physical buttons · Wired", "muted"));
  connection =
      designLabel(device.mock ? "Demo device" : "Connecting…", "badge");
  dc->addWidget(connection);
  rail->addWidget(deviceCard);
  rail->addSpacing(16);
  rail->addWidget(designLabel("DEVICE SETTINGS", "eyebrow"));
  QStringList names = {"Assignments", "Performance", "Lighting", "Macros",
                       "Desktop"};
  QStringList icons = {"buttons", "performance", "lighting", "macro",
                       "desktop"};
  auto *navGroup = new QButtonGroup(this);
  navGroup->setExclusive(true);
  for (int n = 0; n < names.size(); ++n) {
    auto *nav = new QPushButton(names[n]);
    nav->setProperty("role", "nav");
    nav->setCheckable(true);
    nav->setIcon(designIcon(icons[n]));
    nav->setIconSize(QSize(20, 20));
    nav->setObjectName("nav-" + QString::number(n));
    navigation << nav;
    navGroup->addButton(nav, n);
    rail->addWidget(nav);
  }
  rail->addStretch();
  rail->addWidget(designLabel("LOCAL PRESET", "eyebrow"));
  profileCombo = new QComboBox;
  profileCombo->setObjectName("profile");
  profileCombo->addItems({"Office", "Game I", "Game II", "Game III"});
  rail->addWidget(profileCombo);
  presetState = designLabel("Saved locally", "muted");
  rail->addWidget(presetState);
  auto *presetTools = new QHBoxLayout;
  presetTools->setSpacing(7);
  connect(button("Import", presetTools), &QPushButton::clicked, this,
          &Window::importFile);
  connect(button("Export", presetTools), &QPushButton::clicked, this,
          &Window::exportFile);
  rail->addLayout(presetTools);
  auto *version = designLabel("LINUX  /  0.2.0", "eyebrow");
  rail->addSpacing(16);
  rail->addWidget(version);
  shell->addWidget(sidebar);
  auto *workspace = new QWidget;
  workspace->setObjectName("workspace");
  auto *layout = new QVBoxLayout(workspace);
  layout->setContentsMargins(28, 25, 28, 0);
  layout->setSpacing(18);
  shell->addWidget(workspace, 1);
  auto *head = new QHBoxLayout;
  auto *headText = new QVBoxLayout;
  headText->setSpacing(5);
  headText->addWidget(designLabel("GM320  /  DEVICE CONFIGURATION", "eyebrow"));
  pageTitle = designLabel("Button assignments", "title");
  pageSubtitle = designLabel("Make every control work your way.", "muted");
  headText->addWidget(pageTitle);
  headText->addWidget(pageSubtitle);
  head->addLayout(headText, 1);
  refreshButton = button("Sync device", head);
  refreshButton->setProperty("role", "quiet");
  refreshButton->setIcon(designIcon("refresh"));
  connect(refreshButton, &QPushButton::clicked, this, &Window::loadMouse);
  layout->addLayout(head);
  auto *summary = new QHBoxLayout;
  summary->setSpacing(12);
  auto addMetric = [&](QString text, QString icon, QLabel **value) {
    QVBoxLayout *box;
    auto *card = designCard(&box);
    box->setContentsMargins(16, 12, 16, 12);
    box->setSpacing(7);
    auto *row = new QHBoxLayout;
    row->addWidget(designLabel(text, "eyebrow"));
    row->addStretch();
    auto *i = new QLabel;
    i->setPixmap(designIcon(icon, QColor("#677584"), 18).pixmap(18, 18));
    row->addWidget(i);
    box->addLayout(row);
    *value = designLabel("—", "value");
    box->addWidget(*value);
    summary->addWidget(card, 1);
  };
  addMetric("PRESET DPI", "performance", &dpiSummary);
  addMetric("PRESET POLLING", "mouse", &rateSummary);
  addMetric("LIGHTING", "lighting", &lightSummary);
  layout->addLayout(summary);
  tabs = new QTabWidget;
  tabs->setObjectName("tabs");
  tabs->tabBar()->hide();
  layout->addWidget(tabs, 1);
  auto addPage = [&](QString name) {
    auto *content = new QWidget;
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    tabs->addTab(scroll, name);
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(0, 0, 6, 0);
    v->setSpacing(16);
    return v;
  };
  auto *bv = addPage("Assignments");
  auto *bh = new QHBoxLayout;
  bh->setSpacing(18);
  QVBoxLayout *viewColumn;
  auto *previewCard =
      designCard(&viewColumn, "Your mouse",
                 "Select a numbered control to edit its assignment.");
  auto *viewControls = new QHBoxLayout;
  auto *three = new QCheckBox("3D view");
  viewControls->addWidget(three);
  auto *rotate = new QCheckBox("Rotate");
  viewControls->addWidget(rotate);
  viewControls->addStretch();
  auto *reset = button("Reset", viewControls);
  reset->setProperty("role", "quiet");
  viewColumn->addLayout(viewControls);
  viewer = new Viewer;
  viewer->setMinimumSize(340, 285);
  viewColumn->setSpacing(10);
  viewColumn->addWidget(viewer, 1);
  connect(three, &QCheckBox::toggled, viewer, &Viewer::setThreeD);
  connect(rotate, &QCheckBox::toggled, viewer, &Viewer::setAutoRotate);
  connect(reset, &QPushButton::clicked, viewer, &Viewer::resetView);
  auto *test = new QCheckBox("Test mouse input in this view");
  viewColumn->addWidget(test);
  connect(test, &QCheckBox::toggled, viewer, &Viewer::setTestEnabled);
  tester = designLabel("Input test stays inside this view.", "muted");
  viewColumn->addWidget(tester);
  connect(viewer, &Viewer::tested, tester, &QLabel::setText);
  bh->addWidget(previewCard, 5);
  QVBoxLayout *assign;
  auto *assignCard = designCard(&assign, "Assignments");
  assign->setSpacing(6);
  assign->setContentsMargins(16, 16, 16, 16);
  for (int i = 0; i < 8; ++i) {
    auto *row = new QFrame;
    row->setProperty("surface", "inset");
    row->setProperty("selected", i == 0);
    row->setObjectName("assignment-" + QString::number(i + 1));
    assignmentRows << row;
    auto *r = new QHBoxLayout(row);
    r->setContentsMargins(10, 5, 10, 5);
    r->setSpacing(10);
    auto *number = designLabel(QString::number(i + 1), "number");
    number->setFixedSize(25, 25);
    number->setAlignment(Qt::AlignCenter);
    r->addWidget(number);
    auto *name = designLabel(i < 7 ? buttonNames[i] : "Fire button");
    name->setMinimumWidth(90);
    r->addWidget(name, 1);
    auto *c = new QComboBox;
    c->setMinimumWidth(125);
    c->setMaximumWidth(175);
    c->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    c->setObjectName("button" + QString::number(i + 1));
    if (i < 7) {
      c->addItems(mouse::actions);
      buttons << c;
      connect(c, &QComboBox::currentTextChanged, this, [this, i] {
        if (!rendering) {
          viewer->setSelected(i);
          for (int n = 0; n < assignmentRows.size(); ++n) {
            assignmentRows[n]->setProperty("selected", n == i);
            assignmentRows[n]->style()->unpolish(assignmentRows[n]);
            assignmentRows[n]->style()->polish(assignmentRows[n]);
          }
          collect();
        }
      });
    } else {
      c->addItem("Unverified");
      c->setEnabled(false);
      c->setToolTip("The dedicated fire button is present. Its remapping "
                    "protocol is not verified.");
    }
    r->addWidget(c, 2);
    assign->addWidget(row);
  }
  fireNote = designLabel(
      "8 · Dedicated fire button. Remapping support is not verified.", "muted");
  assign->addWidget(fireNote);
  assign->addStretch();
  bh->addWidget(assignCard, 5);
  bv->addLayout(bh, 1);
  connect(viewer, &Viewer::selected, this, [this](int n) {
    for (int i = 0; i < assignmentRows.size(); ++i) {
      assignmentRows[i]->setProperty("selected", i == n);
      assignmentRows[i]->style()->unpolish(assignmentRows[i]);
      assignmentRows[i]->style()->polish(assignmentRows[i]);
    }
    if (n < 7)
      buttons[n]->setFocus();
    else {
      fireNote->setText("Fire button selected · its separate remapping "
                        "protocol has not been verified.");
    }
  });
  auto *sv = addPage("Performance");
  QVBoxLayout *stagesLayout;
  auto *stagesCard =
      designCard(&stagesLayout, "DPI stages",
                 "Choose a stage to edit. The active stage must stay enabled.");
  auto *stageGrid = new QGridLayout;
  stageGrid->setSpacing(10);
  auto *stageGroup = new QButtonGroup(this);
  stageGroup->setExclusive(true);
  const QStringList stageColors = {"#a9e9cc", "#93c7f5", "#c6aff3",
                                   "#e9b7c7", "#e8cea1", "#91d5df"};
  for (int i = 0; i < 6; ++i) {
    auto *card = new QFrame;
    card->setProperty("surface", "inset");
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(12, 12, 12, 12);
    v->setSpacing(9);
    auto *row = new QHBoxLayout;
    row->addWidget(designLabel("STAGE " + QString::number(i + 1), "eyebrow"));
    auto *en = new QCheckBox;
    en->setAccessibleName("Enable DPI stage " + QString::number(i + 1));
    row->addStretch();
    row->addWidget(en);
    v->addLayout(row);
    auto *value = designLabel("—", "value");
    value->setStyleSheet("color:" + stageColors[i] + ";font-size:24px");
    dpiValues << value;
    v->addWidget(value);
    auto *d = new QComboBox;
    for (int dpiValue : dpis)
      d->addItem(QString::number(dpiValue), dpiValue);
    d->setObjectName("dpi-stage-" + QString::number(i + 1));
    v->addWidget(d);
    auto *r = new QRadioButton("Active");
    stageGroup->addButton(r);
    dpiEnabled << en;
    dpi << d;
    stage << r;
    v->addWidget(r);
    auto *edit = button("Adjust");
    edit->setProperty("role", "quiet");
    v->addWidget(edit);
    connect(edit, &QPushButton::clicked, this, [this, i] {
      dpiEditing = i;
      render();
    });
    connect(en, &QCheckBox::toggled, this, [this] { collect(); });
    connect(d, &QComboBox::currentIndexChanged, this, [this] { collect(); });
    connect(r, &QRadioButton::toggled, this, [this] { collect(); });
    stageGrid->addWidget(card, 0, i);
  }
  stagesLayout->addLayout(stageGrid);
  auto *editor = new QHBoxLayout;
  editor->addWidget(designLabel("Stage adjustment", "muted"));
  dpiSlider = new QSlider(Qt::Horizontal);
  dpiSlider->setRange(0, dpis.size() - 1);
  editor->addWidget(dpiSlider, 1);
  dpiEditor = spin(200, 12800);
  dpiEditor->setSuffix(" DPI");
  dpiEditor->setMinimumWidth(135);
  editor->addWidget(dpiEditor);
  stagesLayout->addLayout(editor);
  connect(dpiSlider, &QSlider::valueChanged, this, [this](int v) {
    if (!rendering)
      dpi[dpiEditing]->setCurrentIndex(v);
  });
  connect(dpiEditor, &QSpinBox::editingFinished, this, [this] {
    if (rendering)
      return;
    int v = dpiEditor->value(), best = 0;
    for (int i = 1; i < dpis.size(); ++i)
      if (qAbs(dpis[i] - v) < qAbs(dpis[best] - v))
        best = i;
    dpi[dpiEditing]->setCurrentIndex(best);
    render();
  });
  sv->addWidget(stagesCard);
  QVBoxLayout *responseLayout;
  auto *responseCard = designCard(&responseLayout, "Response");
  auto *sf = new QFormLayout;
  sf->setSpacing(12);
  polling = new QComboBox;
  for (int rate : {125, 250, 500, 1000})
    polling->addItem(QString::number(rate) + " Hz", rate);
  sf->addRow("Polling choice", polling);
  timing = spin(0, 65535);
  timing->setSuffix(" ms");
  sf->addRow("Click timing", timing);
  auto *responseRow = new QHBoxLayout;
  auto *pollBox = new QVBoxLayout;
  pollBox->addWidget(designLabel("Polling choice", "muted"));
  pollBox->addWidget(polling);
  auto *clickBox = new QVBoxLayout;
  clickBox->addWidget(designLabel("Click timing", "muted"));
  clickBox->addWidget(timing);
  sf->removeWidget(polling);
  sf->removeWidget(timing);
  delete sf;
  responseRow->addLayout(pollBox, 1);
  responseRow->addLayout(clickBox, 1);
  responseLayout->addLayout(responseRow);
  responseLayout->addWidget(designLabel(
      "Timing edits: 8–300 ms. Existing values remain intact.", "muted"));
  sv->addWidget(responseCard);
  sv->addStretch();
  connect(polling, &QComboBox::currentIndexChanged, this,
          [this] { collect(); });
  connect(timing, &QSpinBox::valueChanged, this, [this] { collect(); });
  auto *lv = addPage("Lighting");
  auto *lightTop = new QHBoxLayout;
  lightTop->setSpacing(16);
  QVBoxLayout *lightPreviewLayout;
  auto *lightPreview =
      designCard(&lightPreviewLayout, "Lighting preview",
                 "Illustrative colors; the physical effect can differ.");
  lightingViewer = new Viewer;
  lightingViewer->setMinimumSize(310, 300);
  lightPreviewLayout->addWidget(lightingViewer, 1);
  lightTop->addWidget(lightPreview, 4);
  QVBoxLayout *effectLayout;
  auto *effectCard = designCard(&effectLayout, "Effect settings");
  auto *lf = new QFormLayout;
  lf->setSpacing(13);
  effect = new QComboBox;
  effect->addItems(effects);
  direction = new QComboBox;
  direction->addItems(directions);
  brightness = new LevelSpin(true);
  speed = new LevelSpin(false);
  symmetry = new QCheckBox("Symmetric pattern");
  lf->addRow("Stored effect", effect);
  lf->addRow("Brightness", brightness);
  lf->addRow("Animation speed", speed);
  lf->addRow("Direction", direction);
  lf->addRow(symmetry);
  effectLayout->addLayout(lf);
  effectLayout->addWidget(designLabel(
      "Unsupported values read from your mouse remain intact.", "muted"));
  effectLayout->addStretch();
  lightTop->addWidget(effectCard, 5);
  lv->addLayout(lightTop);
  QVBoxLayout *paletteLayout;
  auto *paletteCard = designCard(&paletteLayout, "Color palette");
  auto *paletteGrid = new QGridLayout;
  paletteGrid->setHorizontalSpacing(14);
  paletteGrid->setVerticalSpacing(8);
  QStringList headings = {"COLOR", "PALETTE", "ENABLED", "ON BUTTON PRESS",
                          "RESPONSE"};
  for (int n = 0; n < headings.size(); ++n)
    paletteGrid->addWidget(designLabel(headings[n], "eyebrow"), 0, n);
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
    paletteGrid->addWidget(designLabel(QString::number(i + 1), "muted"), i + 1,
                           0);
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

  paletteLayout->addLayout(paletteGrid);
  lv->addWidget(paletteCard);
  for (auto *c : {effect, direction})
    connect(c, &QComboBox::currentTextChanged, this, [this] { collect(); });
  for (auto *s : {brightness, speed})
    connect(s, &QSpinBox::valueChanged, this, [this] { collect(); });
  connect(symmetry, &QCheckBox::toggled, this, [this] { collect(); });
  QVBoxLayout *liveLayout;
  auto *liveBox = designCard(&liveLayout, "Live color",
                             "Experimental override; the device cannot report "
                             "its active effect back.");
  auto *liveForm = new QFormLayout;
  liveForm->setSpacing(10);
  liveEnabled = new QCheckBox("Use live color override");
  liveEffect = new QComboBox;
  liveEffect->addItems({"Static", "Breathing", "Off"});
  liveSpeed = new LevelSpin(false);
  liveSpeed->setRange(0, 5);
  auto *colorButton = button("Choose color");
  connect(colorButton, &QPushButton::clicked, this, [this] {
    QColor c = QColorDialog::getColor(liveColor, this);
    if (c.isValid()) {
      liveColor = c;
      collect();
    }
  });
  liveForm->addRow(liveEnabled);
  liveForm->addRow("Effect", liveEffect);
  liveForm->addRow("Color", colorButton);
  liveForm->addRow("Speed", liveSpeed);
  liveLayout->addLayout(liveForm);
  lv->addWidget(liveBox);
  connect(liveEnabled, &QCheckBox::toggled, this, [this] { collect(); });
  connect(liveEffect, &QComboBox::currentIndexChanged, this,
          [this] { collect(); });
  connect(liveSpeed, &QSpinBox::valueChanged, this, [this] { collect(); });
  auto *advancedToggle = new QPushButton("Advanced lighting");
  advancedToggle->setProperty("role", "quiet");
  advancedToggle->setCheckable(true);
  lv->addWidget(advancedToggle, 0, Qt::AlignLeft);
  auto *experimental = new QGroupBox("Experimental vendor report");
  experimental->setVisible(false);
  connect(advancedToggle, &QPushButton::toggled, experimental,
          &QWidget::setVisible);
  auto *xf = new QFormLayout(experimental);
  experimentalBrightness = spin(0, 7);
  experimentalSpeed = spin(0, 7);
  xf->addRow("Brightness code", experimentalBrightness);
  xf->addRow("Speed code", experimentalSpeed);
  auto *xb = button("Send experimental report");
  xf->addRow(xb);
  xf->addRow(
      designLabel("One captured report; effect and persistence are unverified. "
                  "This operation cannot back up live lighting.",
                  "muted"));
  connect(xb, &QPushButton::clicked, this, [this] {
    auto request = QJsonObject{{"brightness", experimentalBrightness->value()},
                               {"speed", experimentalSpeed->value()}};
    enqueue(
        [this, request] { return device.operation("experimental", request); },
        [this](auto) {
          message(
              "Experimental report accepted. Physical effect is unverified.");
        });
  });
  lv->addWidget(experimental);
  lv->addStretch();
  auto *mv = addPage("Macros");
  QVBoxLayout *macroLayout;
  auto *macroCard =
      designCard(&macroLayout, "Sequence editor",
                 "Create a keyboard sequence for a Linux mouse trigger.");
  events = new QListWidget;
  events->setObjectName("macro-events");
  events->setMinimumHeight(180);
  macroLayout->addWidget(events, 1);
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
  connect(button("Clear all", er), &QPushButton::clicked, this, [this] {
    auto p = profile();
    p["macro"] = QJsonArray();
    setProfile(p);
    stopPreview();
    render();
    dirty();
  });

  macroLayout->addLayout(er);
  mv->addWidget(macroCard, 1);
  QVBoxLayout *triggerLayout;
  auto *triggerCard = designCard(&triggerLayout, "Trigger & playback");
  auto *mr = new QHBoxLayout;
  trigger = new QComboBox;
  trigger->addItem("Back button", 275);
  trigger->addItem("Forward button", 276);
  trigger->addItem("Middle click", 274);
  mr->addWidget(trigger, 1);
  macroApplyButton = button("Enable macro", mr);
  macroStopButton = button("Stop", mr);
  previewButton = button("Preview", mr);
  triggerLayout->addLayout(mr);
  macroNote = designLabel("Uses Input Remapper. Macros run on Linux while this "
                          "app is open; they are not stored onboard.",
                          "muted");
  triggerLayout->addWidget(macroNote);
  mv->addWidget(triggerCard);
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

  auto *dv = addPage("Desktop");
  QVBoxLayout *desktopLayout;
  auto *desktopCard = designCard(&desktopLayout, "Pointer & scrolling",
                                 "Desktop controls for your mouse.");
  auto *df = new QFormLayout;
  df->setSpacing(16);
  gain = new QDoubleSpinBox;
  gain->setRange(-1, 1);
  gain->setSingleStep(.1);
  scroll = new QDoubleSpinBox;
  scroll->setRange(.25, 11);
  scroll->setSingleStep(.25);
  wheelEnabled = new QCheckBox("Enable X11 scroll helper");
  df->addRow("Pointer acceleration", gain);
  df->addRow("Scroll multiplier", scroll);
  df->addRow(wheelEnabled);
  desktopLayout->addLayout(df);
  desktopNote = designLabel("Reading desktop settings…", "muted");
  desktopLayout->addWidget(desktopNote);
  auto *dr = new QHBoxLayout;
  connect(button("Reload", dr), &QPushButton::clicked, this,
          &Window::refreshDesktop);
  auto *desktopApply = button("Apply desktop settings", dr);
  desktopApply->setProperty("role", "primary");
  connect(desktopApply, &QPushButton::clicked, this, [this] {
    QString rev = desktop["revision"].toString();
    double g = gain->value(), s = scroll->value();
    bool en = wheelEnabled->isChecked();
    enqueue([this, rev, g, s,
             en] { return integration.desktopApply(rev, g, s, en); },
            [this](auto r) {
              desktop = r;
              message("Desktop settings applied and read back.");
              QTimer::singleShot(0, this, &Window::refreshDesktop);
            });
  });
  desktopLayout->addLayout(dr);
  dv->addWidget(desktopCard);
  dv->addStretch();
  auto *footerCard = new QFrame;
  footerCard->setObjectName("footer");
  auto *foot = new QVBoxLayout(footerCard);
  foot->setContentsMargins(0, 13, 0, 16);
  foot->setSpacing(9);
  status = designLabel("Connecting to your mouse…", "muted");
  status->setMinimumHeight(32);
  foot->addWidget(status);
  auto *footer = new QHBoxLayout;
  restoreButton = button("Restore last change", footer);
  restoreButton->setProperty("role", "quiet");
  restoreButton->setIcon(designIcon("restore"));
  footer->addStretch();
  undoButton = button("Discard edits", footer);
  undoButton->setProperty("role", "quiet");
  saveButton = button("Save preset", footer);
  saveButton->setIcon(designIcon("save"));
  applyButton = button("Apply to mouse", footer);
  applyButton->setObjectName("apply-mouse");
  applyButton->setProperty("role", "primary");
  applyButton->setIcon(designIcon("arrow", QColor("#142c20")));
  foot->addLayout(footer);
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

  const QStringList pageTitles = {"Button assignments", "Performance",
                                  "Lighting studio", "Macro editor",
                                  "Desktop controls"};
  const QStringList descriptions = {
      "Make every control work your way.",
      "Tune sensitivity, stages and response.",
      "Set the colors that make it yours.",
      "Build a sequence. Choose a trigger.",
      "Fine-tune pointer movement and scrolling."};
  connect(navGroup, &QButtonGroup::idClicked, this,
          [this](int n) { tabs->setCurrentIndex(n); });
  connect(tabs, &QTabWidget::currentChanged, this,
          [this, pageTitles, descriptions](int n) {
            navigation[n]->setChecked(true);
            pageTitle->setText(pageTitles[n]);
            pageSubtitle->setText(descriptions[n]);
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
  for (int n = 0; n < 6; ++n)
    dpiValues[n]->setText(QString::number(s["dpi"].toArray()[n].toInt()));
  dpiSummary->setText(
      QString::number(s["dpi"].toArray()[s["stage"].toInt()].toInt()));
  rateSummary->setText(QString::number(s["polling"].toInt()) + " Hz");
  lightSummary->setStyleSheet("font-size:16px;color:#dbe3ec");
  lightSummary->setText(p["live"].toObject().isEmpty()
                            ? s["lighting"].toObject()["effect"].toString()
                            : p["live"].toObject()["effect"].toString());
  dpiSlider->blockSignals(true);
  dpiEditor->blockSignals(true);
  dpiSlider->setValue(dpi[dpiEditing]->currentIndex());
  dpiEditor->setValue(dpi[dpiEditing]->currentData().toInt());
  dpiSlider->blockSignals(false);
  dpiEditor->blockSignals(false);
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
  for (int n = 0; n < 6; ++n)
    dpiValues[n]->setText(QString::number(s["dpi"].toArray()[n].toInt()));
  dpiSlider->setValue(dpi[dpiEditing]->currentIndex());
  dpiEditor->setValue(dpi[dpiEditing]->currentData().toInt());
  dpiSummary->setText(
      QString::number(s["dpi"].toArray()[s["stage"].toInt()].toInt()));
  rateSummary->setText(QString::number(s["polling"].toInt()) + " Hz");
  lightSummary->setStyleSheet("font-size:16px;color:#dbe3ec");
  lightSummary->setText(p["live"].toObject().isEmpty()
                            ? l["effect"].toString()
                            : p["live"].toObject()["effect"].toString());
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
    if (b->text() == "Import" || b->text() == "Export")
      b->setEnabled(!busy);
  tabs->setEnabled(!busy);
  for (auto *nav : navigation)
    nav->setEnabled(!busy);
  presetState->setText(changed ? "Unsaved changes" : "Saved locally");
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
            connection->setText(device.mock ? "●  Demo device"
                                            : "●  Connected");
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
    writeImage(4, "desktop");
    tabs->setCurrentIndex(2);
    auto *lightingScroll = qobject_cast<QScrollArea *>(tabs->widget(2));
    lightingScroll->verticalScrollBar()->setValue(
        lightingScroll->verticalScrollBar()->maximum());
    writeImage(2, "lighting-palette");
    lightingScroll->verticalScrollBar()->setValue(0);
    viewer->setSelected(7);
    writeImage(0, "fire-button");
    viewer->setSelected(0);
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
    resize(1050, 730);
    viewer->setThreeD(false);
    writeImage(0, "compact-assignments");
    resize(1260, 840);
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
