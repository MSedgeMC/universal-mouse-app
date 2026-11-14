// Original native device-page implementation. Copyright 2026 MSedgeMC. MIT.
#include "core/storage.h"
#include "design.h"
#include "window.h"
#include <QButtonGroup>
#include <QColorDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QStyle>
#include <algorithm>
namespace mouse {
namespace {
QPushButton *pageButton(const QString &text, QLayout *layout = nullptr,
                        const QString &role = "quiet") {
  auto *b = new QPushButton(text);
  b->setProperty("role", role);
  if (layout)
    layout->addWidget(b);
  return b;
}
QSpinBox *pageSpin(int low, int high, const QString &suffix = {}) {
  auto *s = new QSpinBox;
  s->setRange(low, high);
  s->setSuffix(suffix);
  return s;
}
void repolish(QWidget *w) {
  w->style()->unpolish(w);
  w->style()->polish(w);
  w->update();
}
QFrame *rule() {
  auto *f = new QFrame;
  f->setProperty("role", "rule");
  f->setFixedHeight(1);
  return f;
}
} // namespace
QVBoxLayout *Window::addPage(const QString &name, bool canvas) {
  auto *content = new QWidget;
  content->setObjectName(canvas ? "canvas-page" : "settings-page");
  auto *scrollArea = new QScrollArea;
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setWidget(content);
  tabs->addTab(scrollArea, name);
  auto *v = new QVBoxLayout(content);
  v->setContentsMargins(canvas ? 24 : 32, canvas ? 12 : 18, canvas ? 24 : 32,
                        canvas ? 12 : 18);
  v->setSpacing(16);
  return v;
}
void Window::buildCustomize() {
  auto *v = addPage("Customize", true);
  v->setSpacing(9);
  auto *heading = new QHBoxLayout;
  heading->addWidget(designLabel("BUTTON ASSIGNMENTS", "eyebrow"));
  heading->addStretch();
  auto *instruction =
      designLabel("Select a button on the mouse to customize it.", "muted");
  instruction->setWordWrap(false);
  heading->addWidget(instruction);
  v->addLayout(heading);
  auto *stageRow = new QHBoxLayout;
  stageRow->setSpacing(18);
  QVBoxLayout *editor;
  assignmentPanel = designCard(&editor);
  assignmentPanel->setObjectName("assignment-panel");
  assignmentPanel->setFixedWidth(284);
  editor->setContentsMargins(22, 20, 22, 22);
  editor->setSpacing(14);
  auto *edHead = new QHBoxLayout;
  assignmentNumber = designLabel("01", "assignment-number");
  edHead->addWidget(assignmentNumber);
  edHead->addStretch();
  auto *close = pageButton("", edHead, "icon");
  close->setIcon(designIcon("close"));
  close->setAccessibleName("Close button editor");
  close->setFixedSize(28, 28);
  connect(close, &QPushButton::clicked, assignmentPanel, &QWidget::hide);
  editor->addLayout(edHead);
  assignmentName = designLabel("Left click", "editor-title");
  editor->addWidget(assignmentName);
  editor->addWidget(rule());
  editor->addWidget(designLabel("ASSIGNMENT", "eyebrow"));
  assignmentStack = new QStackedWidget;
  assignmentStack->setSizePolicy(QSizePolicy::Expanding,
                                 QSizePolicy::Preferred);
  for (int i = 0; i < 8; ++i) {
    auto *row = new QFrame;
    row->setObjectName("assignment-" + QString::number(i + 1));
    assignmentRows << row;
    auto *form = new QVBoxLayout(row);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(12);
    auto *combo = new QComboBox;
    combo->setObjectName("button" + QString::number(i + 1));
    combo->setMinimumWidth(180);
    if (i < 7) {
      combo->addItems(mouse::actions);
      buttons << combo;
      connect(combo, &QComboBox::currentTextChanged, this,
              [this] { collect(); });
    } else {
      combo->addItem("Remapping unavailable");
      combo->setEnabled(false);
    }
    form->addWidget(combo);
    form->addWidget(designLabel(
        i < 7 ? "Choose the action this physical button performs. Edits stay "
                "in the preset until you apply them."
              : "The small fire button is present beside left click. Its "
                "separate remapping command has not been verified.",
        "muted"));
    form->addStretch();
    assignmentStack->addWidget(row);
  }
  editor->addWidget(assignmentStack, 1);
  editor->addWidget(rule());
  fireNote = designLabel("Seven button assignments are supported.", "muted");
  editor->addWidget(fireNote);
  assignmentPanel->hide();
  stageRow->addWidget(assignmentPanel);
  viewer = new Viewer;
  viewer->setMinimumSize(560, 375);
  stageRow->addWidget(viewer, 1);
  v->addLayout(stageRow, 1);
  connect(viewer, &Viewer::selected, this, [this](int n) { selectButton(n); });
  auto *controls = new QHBoxLayout;
  controls->addStretch();
  auto *views = new QFrame;
  views->setProperty("surface", "segment");
  views->setFixedHeight(40);
  auto *viewRow = new QHBoxLayout(views);
  viewRow->setContentsMargins(4, 4, 4, 4);
  viewRow->setSpacing(3);
  auto *viewGroup = new QButtonGroup(this);
  auto *top = pageButton("Top view", viewRow, "segment");
  auto *three = pageButton("3D schematic", viewRow, "segment");
  top->setFixedHeight(32);
  three->setFixedHeight(32);
  top->setCheckable(true);
  three->setCheckable(true);
  top->setChecked(true);
  top->setObjectName("top-view");
  three->setObjectName("three-view");
  viewGroup->addButton(top);
  viewGroup->addButton(three);
  connect(top, &QPushButton::clicked, this,
          [this] { viewer->setThreeD(false); });
  connect(three, &QPushButton::clicked, this,
          [this] { viewer->setThreeD(true); });
  controls->addWidget(views);
  auto *rotate = new QCheckBox("Rotate");
  rotate->setVisible(false);
  connect(three, &QPushButton::toggled, rotate, &QWidget::setVisible);
  connect(rotate, &QCheckBox::toggled, viewer, &Viewer::setAutoRotate);
  controls->addWidget(rotate);
  auto *reset = pageButton("Reset view", controls);
  connect(reset, &QPushButton::clicked, viewer, &Viewer::resetView);
  controls->addStretch();
  v->addLayout(controls);
  auto *testRow = new QHBoxLayout;
  auto *test = new QCheckBox("Test mouse input");
  connect(test, &QCheckBox::toggled, viewer, &Viewer::setTestEnabled);
  testRow->addWidget(test);
  tester = designLabel("Click or scroll inside the mouse view.", "muted");
  testRow->addWidget(tester, 1);
  connect(viewer, &Viewer::tested, tester, &QLabel::setText);
  testRow->addWidget(
      designLabel("Fire button · remapping unavailable", "micro"));
  v->addLayout(testRow);
}
void Window::selectButton(int n, bool openEditor) {
  if (n < 0 || n >= 8)
    return;
  selectedButton = n;
  viewer->setSelected(n);
  assignmentStack->setCurrentIndex(n);
  const QStringList names = {"Left click",     "Right click", "Scroll click",
                             "Forward button", "Back button", "Top button 1",
                             "Top button 2",   "Fire button"};
  assignmentName->setText(names[n]);
  assignmentNumber->setText(QString("%1").arg(n + 1, 2, 10, QChar('0')));
  for (int i = 0; i < 8; ++i)
    assignmentRows[i]->setProperty("selected", i == n);
  fireNote->setText(n == 7 ? "Fire-button remapping is not yet supported."
                           : "Changes require Apply to mouse.");
  if (openEditor)
    assignmentPanel->show();
}
void Window::buildPerformance() {
  auto *v = addPage("Performance");
  auto *heading = new QHBoxLayout;
  auto *titles = new QVBoxLayout;
  titles->setSpacing(6);
  titles->addWidget(designLabel("SENSITIVITY", "eyebrow"));
  titles->addWidget(designLabel("Sensitivity (DPI)", "page-title"));
  heading->addLayout(titles, 1);
  dpiSummary = designLabel("—", "hero-value");
  heading->addWidget(dpiSummary);
  heading->addWidget(designLabel("DPI", "unit"));
  v->addLayout(heading);
  QVBoxLayout *sensitivity;
  auto *panel = designCard(&sensitivity);
  sensitivity->setSpacing(10);
  auto *stageHeading = new QHBoxLayout;
  stageHeading->addWidget(designLabel("DPI STAGES", "section"));
  stageHeading->addStretch();
  auto *stageHint =
      designLabel("Enable stages to include them when cycling.", "muted");
  stageHint->setWordWrap(false);
  stageHeading->addWidget(stageHint);
  sensitivity->addLayout(stageHeading);
  auto *tileRow = new QHBoxLayout;
  tileRow->setSpacing(10);
  auto *group = new QButtonGroup(this);
  const QStringList stageColors = {"#159bff", "#8a77ff", "#e271ac",
                                   "#e6a257", "#5fbcb1", "#83b9ed"};
  for (int i = 0; i < 6; ++i) {
    auto *tile = new QFrame;
    tile->setProperty("surface", "dpi-tile");
    tile->setObjectName("dpi-tile-" + QString::number(i + 1));
    dpiTiles << tile;
    auto *box = new QVBoxLayout(tile);
    box->setContentsMargins(12, 10, 12, 10);
    box->setSpacing(7);
    auto *row = new QHBoxLayout;
    auto *num = designLabel("STAGE " + QString::number(i + 1), "micro");
    num->setStyleSheet("color:" + stageColors[i]);
    row->addWidget(num);
    row->addStretch();
    auto *en = new QCheckBox;
    en->setAccessibleName("Enable DPI stage " + QString::number(i + 1));
    dpiEnabled << en;
    row->addWidget(en);
    box->addLayout(row);
    auto *d = new QComboBox;
    d->setObjectName("dpi-stage-" + QString::number(i + 1));
    d->setProperty("role", "dpi-value");
    for (int value : dpis)
      d->addItem(QString::number(value), value);
    dpi << d;
    box->addWidget(d);
    auto *active = new QRadioButton("Active");
    group->addButton(active);
    stage << active;
    auto *tileActions = new QHBoxLayout;
    tileActions->addWidget(active);
    tileActions->addStretch();
    auto *adjust = pageButton("Adjust", nullptr, "stage-adjust");
    adjust->setObjectName("adjust-stage-" + QString::number(i + 1));
    tileActions->addWidget(adjust);
    box->addLayout(tileActions);
    connect(adjust, &QPushButton::clicked, this, [this, i] {
      dpiEditing = i;
      syncPresentation();
    });
    connect(en, &QCheckBox::toggled, this, [this] { collect(); });
    connect(d, &QComboBox::currentIndexChanged, this, [this] { collect(); });
    connect(active, &QRadioButton::toggled, this, [this] { collect(); });
    tileRow->addWidget(tile, 1);
  }
  sensitivity->addLayout(tileRow);
  sensitivity->addWidget(rule());
  auto *adjustment = new QHBoxLayout;
  auto *editing = designLabel("ADJUSTING STAGE 1", "eyebrow");
  editing->setObjectName("editing-stage-label");
  adjustment->addWidget(editing);
  adjustment->addStretch();
  dpiEditor = pageSpin(200, 12800, " DPI");
  dpiEditor->setObjectName("dpi-editor");
  dpiEditor->setFixedWidth(138);
  adjustment->addWidget(dpiEditor);
  sensitivity->addLayout(adjustment);
  dpiSlider = new QSlider(Qt::Horizontal);
  dpiSlider->setObjectName("dpi-slider");
  dpiSlider->setRange(0, dpis.size() - 1);
  dpiSlider->setMinimumHeight(24);
  sensitivity->addWidget(dpiSlider);
  auto *scale = new QHBoxLayout;
  for (auto value : {"200", "1,000", "3,200", "6,400", "12,800"}) {
    auto *label = designLabel(value, "micro");
    scale->addWidget(label);
    if (QString(value) != "12,800")
      scale->addStretch();
  }
  sensitivity->addLayout(scale);
  connect(dpiSlider, &QSlider::valueChanged, this, [this](int value) {
    if (!rendering)
      dpi[dpiEditing]->setCurrentIndex(value);
  });
  connect(dpiEditor, &QSpinBox::editingFinished, this, [this] {
    if (rendering)
      return;
    int value = dpiEditor->value(), best = 0;
    for (int i = 1; i < dpis.size(); ++i)
      if (qAbs(dpis[i] - value) < qAbs(dpis[best] - value))
        best = i;
    dpi[dpiEditing]->setCurrentIndex(best);
    syncPresentation();
  });
  v->addWidget(panel);
  auto *response = new QHBoxLayout;
  QVBoxLayout *rateBox;
  auto *rateCard = designCard(&rateBox);
  auto *rateRow = new QHBoxLayout;
  rateRow->addWidget(designLabel("REPORT RATE", "section"));
  rateRow->addStretch();
  polling = new QComboBox;
  polling->setObjectName("polling");
  for (int rate : {125, 250, 500, 1000})
    polling->addItem(QString::number(rate) + " Hz", rate);
  polling->setMinimumWidth(150);
  polling->setToolTip("Choose how often the mouse reports to the computer.");
  rateRow->addWidget(polling);
  rateBox->addLayout(rateRow);
  rateSummary = designLabel("—", "muted");
  rateSummary->setToolTip("This is the stored choice. The app does not measure "
                          "physical report frequency.");
  rateBox->addWidget(rateSummary);
  response->addWidget(rateCard, 1);
  QVBoxLayout *timingBox;
  auto *timingCard = designCard(&timingBox);
  auto *timingRow = new QHBoxLayout;
  timingRow->addWidget(designLabel("CLICK TIMING", "section"));
  timingRow->addStretch();
  timing = pageSpin(0, 65535, " ms");
  timing->setObjectName("click-timing");
  timing->setMinimumWidth(150);
  timing->setToolTip(
      "Device timing, separate from desktop double-click speed.");
  timingRow->addWidget(timing);
  timingBox->addLayout(timingRow);
  timingBox->addWidget(designLabel(
      "Editable range: 8–300 ms. Existing values are preserved.", "muted"));
  response->addWidget(timingCard, 1);
  v->addLayout(response);
  v->addStretch();
  connect(polling, &QComboBox::currentIndexChanged, this,
          [this] { collect(); });
  connect(timing, &QSpinBox::valueChanged, this, [this] { collect(); });
}
void Window::buildLighting() {
  auto *v = addPage("Lighting");
  auto *row = new QHBoxLayout;
  row->setSpacing(26);
  auto *preview = new QFrame;
  preview->setObjectName("lighting-stage");
  preview->setFixedHeight(350);
  auto *previewBox = new QVBoxLayout(preview);
  previewBox->setContentsMargins(12, 8, 12, 8);
  previewBox->setSpacing(8);
  previewBox->addWidget(designLabel("LIGHTING PREVIEW", "eyebrow"));
  lightingViewer = new Viewer;
  lightingViewer->setMinimumSize(315, 250);
  lightingViewer->setCalloutsVisible(false);
  lightingViewer->setDisplayLighting(true);
  previewBox->addWidget(lightingViewer, 1);
  lightSummary = designLabel("—", "preview-caption");
  lightSummary->setAlignment(Qt::AlignCenter);
  previewBox->addWidget(lightSummary);
  auto *previewNote =
      designLabel("Illustrative preview; device behavior can differ.", "muted");
  previewNote->setWordWrap(false);
  previewBox->addWidget(previewNote, 0, Qt::AlignCenter);
  row->addWidget(preview, 1);
  QVBoxLayout *settings;
  auto *settingsPanel = designCard(&settings, "Effect settings");
  auto *form = new QFormLayout;
  form->setSpacing(12);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  effect = new QComboBox;
  effect->addItems(effects);
  direction = new QComboBox;
  direction->addItems(directions);
  brightness = new LevelSpin(true);
  speed = new LevelSpin(false);
  brightness->setFixedWidth(140);
  speed->setFixedWidth(140);
  brightnessSlider = new QSlider(Qt::Horizontal);
  brightnessSlider->setRange(0, 6);
  speedSlider = new QSlider(Qt::Horizontal);
  speedSlider->setRange(0, 5);
  auto *brRow = new QHBoxLayout;
  brRow->addWidget(brightnessSlider, 1);
  brRow->addWidget(brightness);
  auto *speedRow = new QHBoxLayout;
  speedRow->addWidget(speedSlider, 1);
  speedRow->addWidget(speed);
  form->addRow("Effect", effect);
  form->addRow("Brightness", brRow);
  form->addRow("Speed", speedRow);
  form->addRow("Direction", direction);
  symmetry = new QCheckBox("Symmetric pattern");
  form->addRow(symmetry);
  settings->addLayout(form);
  settings->addWidget(designLabel(
      "Unsupported values read from the mouse remain intact.", "muted"));
  settings->addStretch();
  row->addWidget(settingsPanel, 1);
  v->addLayout(row);
  QVBoxLayout *liveBox;
  auto *livePanel = designCard(&liveBox);
  auto *liveHeading = new QHBoxLayout;
  liveHeading->addWidget(designLabel("LIVE COLOR", "section"));
  liveHeading->addStretch();
  liveEnabled = new QCheckBox("Use live color override");
  liveHeading->addWidget(liveEnabled);
  liveBox->addLayout(liveHeading);
  liveEffect = new QComboBox;
  liveEffect->addItems({"Static", "Breathing", "Off"});
  liveSpeed = new LevelSpin(false);
  liveSpeed->setRange(0, 5);
  auto *liveFields = new QHBoxLayout;
  liveFields->setSpacing(24);
  liveFields->addWidget(designLabel("Effect", "muted"));
  liveFields->addWidget(liveEffect, 1);
  auto *choose = pageButton("Choose color", nullptr, "secondary");
  choose->setObjectName("live-color");
  liveFields->addWidget(designLabel("Color", "muted"));
  liveFields->addWidget(choose, 1);
  liveFields->addWidget(designLabel("Speed", "muted"));
  liveFields->addWidget(liveSpeed, 1);
  liveBox->addLayout(liveFields);
  liveBox->addWidget(designLabel(
      "Experimental override with no active-effect readback.", "muted"));
  v->addWidget(livePanel);
  connect(choose, &QPushButton::clicked, this, [this] {
    auto c = QColorDialog::getColor(liveColor, this);
    if (c.isValid()) {
      liveColor = c;
      collect();
    }
  });
  connect(brightnessSlider, &QSlider::valueChanged, this, [this](int value) {
    if (!rendering)
      brightness->setValue(6 - value);
  });
  connect(speedSlider, &QSlider::valueChanged, this, [this](int value) {
    if (!rendering)
      speed->setValue(value);
  });
  for (auto *c : {effect, direction})
    connect(c, &QComboBox::currentTextChanged, this, [this] { collect(); });
  for (auto *s : {brightness, speed, liveSpeed})
    connect(s, &QSpinBox::valueChanged, this, [this] { collect(); });
  connect(symmetry, &QCheckBox::toggled, this, [this] { collect(); });
  connect(liveEnabled, &QCheckBox::toggled, this, [this] { collect(); });
  connect(liveEffect, &QComboBox::currentIndexChanged, this,
          [this] { collect(); });

  QVBoxLayout *paletteBox;
  auto *palette =
      designCard(&paletteBox, "Color palette",
                 "Colors and button-response choices stored in your preset.");
  auto *grid = new QGridLayout;
  grid->setHorizontalSpacing(20);
  grid->setVerticalSpacing(12);
  const QStringList headings = {"SLOT", "COLOR", "ENABLED", "BUTTON RESPONSE",
                                "ON PRESS"};
  for (int n = 0; n < headings.size(); ++n)
    grid->addWidget(designLabel(headings[n], "eyebrow"), 0, n);
  for (int i = 0; i < 7; ++i) {
    auto *color = pageButton("", nullptr, "color-swatch");
    auto *enabled = new QCheckBox;
    auto *r = new QComboBox;
    r->addItems(responses);
    auto *ren = new QCheckBox;
    colors << color;
    colorEnabled << enabled;
    response << r;
    responseEnabled << ren;
    color->setAccessibleName("Choose palette color " + QString::number(i + 1));
    enabled->setAccessibleName("Enable palette slot " + QString::number(i + 1));
    ren->setAccessibleName("Enable button response " + QString::number(i + 1));
    grid->addWidget(
        designLabel(QString("%1").arg(i + 1, 2, 10, QChar('0')), "micro"),
        i + 1, 0);
    grid->addWidget(color, i + 1, 1);
    grid->addWidget(enabled, i + 1, 2);
    grid->addWidget(r, i + 1, 3);
    grid->addWidget(ren, i + 1, 4);
    connect(color, &QPushButton::clicked, this, [this, i] {
      auto rgb = profile()["settings"]
                     .toObject()["lighting"]
                     .toObject()["palette"]
                     .toArray()[i]
                     .toArray();
      QColor initial((15 - rgb[0].toInt()) * 17, (15 - rgb[1].toInt()) * 17,
                     (15 - rgb[2].toInt()) * 17);
      auto chosen = QColorDialog::getColor(initial, this, "Palette color");
      if (!chosen.isValid())
        return;
      auto p = profile(), s = p["settings"].toObject(),
           l = s["lighting"].toObject();
      auto a = l["palette"].toArray();
      a[i] = QJsonArray{15 - qRound(chosen.red() / 17.),
                        15 - qRound(chosen.green() / 17.),
                        15 - qRound(chosen.blue() / 17.)};
      l["palette"] = a;
      s["lighting"] = l;
      p["settings"] = s;
      setProfile(p);
      render();
    });
    connect(enabled, &QCheckBox::toggled, this, [this] { collect(); });
    connect(r, &QComboBox::currentTextChanged, this, [this] { collect(); });
    connect(ren, &QCheckBox::toggled, this, [this] { collect(); });
  }
  paletteBox->addLayout(grid);
  v->addWidget(palette);
  auto *advanced = pageButton("Advanced lighting", nullptr, "quiet");
  advanced->setCheckable(true);
  v->addWidget(advanced, 0, Qt::AlignLeft);
  auto *advancedPanel = new QGroupBox("Experimental vendor report");
  advancedPanel->hide();
  connect(advanced, &QPushButton::toggled, advancedPanel, &QWidget::setVisible);
  auto *advancedForm = new QFormLayout(advancedPanel);
  experimentalBrightness = pageSpin(0, 7);
  experimentalSpeed = pageSpin(0, 7);
  advancedForm->addRow("Brightness code", experimentalBrightness);
  advancedForm->addRow("Speed code", experimentalSpeed);
  auto *send = pageButton("Send experimental report");
  advancedForm->addRow(send);
  advancedForm->addRow(
      designLabel("One captured report. Effect and persistence are unverified; "
                  "live lighting cannot be backed up.",
                  "muted"));
  connect(send, &QPushButton::clicked, this, [this] {
    auto request = QJsonObject{{"brightness", experimentalBrightness->value()},
                               {"speed", experimentalSpeed->value()}};
    enqueue(
        [this, request] { return device.operation("experimental", request); },
        [this](auto) {
          message(
              "Experimental report accepted. Physical effect is unverified.");
        });
  });
  v->addWidget(advancedPanel);
  v->addStretch();
}
void Window::buildMacros() {
  auto *v = addPage("Macros");
  auto *heading = new QHBoxLayout;
  auto *title = new QVBoxLayout;
  title->setSpacing(6);
  title->addWidget(designLabel("MACRO EDITOR", "eyebrow"));
  title->addWidget(designLabel("Macro sequence", "page-title"));
  heading->addLayout(title, 1);
  heading->addWidget(designLabel("64 EVENTS MAX  ·  30s TOTAL DELAY", "micro"));
  v->addLayout(heading);
  auto *body = new QHBoxLayout;
  body->setSpacing(24);
  QVBoxLayout *sequence;
  auto *sequencePanel = designCard(&sequence, "Sequence");
  sequencePanel->setMinimumWidth(500);
  events = new QListWidget;
  events->setObjectName("macro-events");
  events->setItemDelegate(new MacroEventDelegate(events));
  events->setMinimumHeight(210);
  sequence->addWidget(events, 1);
  macroEmpty = designLabel("No events yet. Add a key press, release, or delay.",
                           "muted");
  sequence->addWidget(macroEmpty);
  auto *tools = new QHBoxLayout;
  auto *remove = pageButton("Remove selected", tools);
  auto *clear = pageButton("Clear all", tools);
  tools->addStretch();
  previewButton = pageButton("Preview sequence", tools, "secondary");
  sequence->addLayout(tools);
  body->addWidget(sequencePanel, 3);
  QVBoxLayout *editor;
  auto *editorPanel = designCard(&editor, "Add event");
  editor->setContentsMargins(20, 14, 20, 14);
  editor->setSpacing(9);
  auto *form = new QFormLayout;
  form->setSpacing(10);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  eventType = new QComboBox;
  eventType->addItems({"Key down", "Key up", "Delay"});
  eventKey = new QComboBox;
  eventKey->addItems(macroKeys);
  eventDelay = pageSpin(10, 10000, " ms");
  eventDelay->setValue(100);
  form->addRow("Type", eventType);
  form->addRow("Key", eventKey);
  form->addRow("Delay", eventDelay);
  editor->addLayout(form);
  auto *add = pageButton("Add event", nullptr, "primary");
  editor->addWidget(add);
  connect(eventType, &QComboBox::currentIndexChanged, this, [this](int n) {
    eventKey->setEnabled(n != 2);
    eventDelay->setEnabled(n == 2);
  });
  eventDelay->setEnabled(false);
  auto *eventHint = designLabel("Pair every key down with a key up.", "muted");
  eventHint->setToolTip(
      "Preview highlights events without sending keyboard input.");
  editor->addWidget(eventHint);
  previewButton->setToolTip(
      "Highlights the sequence without sending keyboard input.");
  editor->addWidget(rule());
  editor->addWidget(designLabel("MOUSE TRIGGER", "eyebrow"));
  trigger = new QComboBox;
  trigger->addItem("Back button", 275);
  trigger->addItem("Forward button", 276);
  trigger->addItem("Middle click", 274);
  editor->addWidget(trigger);
  macroApplyButton = pageButton("Enable macro", nullptr, "secondary");
  macroStopButton = pageButton("Stop macro");
  auto *macroControls = new QHBoxLayout;
  macroControls->setSpacing(8);
  macroControls->addWidget(macroApplyButton, 1);
  macroControls->addWidget(macroStopButton, 1);
  editor->addLayout(macroControls);
  macroNote = designLabel(
      "Uses Input Remapper while the app is open. Macros are stored locally.",
      "muted");
  editor->addWidget(macroNote);
  editor->addStretch();
  body->addWidget(editorPanel, 2);
  v->addLayout(body, 1);
  connect(add, &QPushButton::clicked, this, [this] {
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
  });
  connect(remove, &QPushButton::clicked, this, [this] {
    auto p = profile();
    auto a = p["macro"].toArray();
    if (events->currentRow() >= 0)
      a.removeAt(events->currentRow());
    p["macro"] = a;
    setProfile(p);
    stopPreview();
    render();
  });
  connect(clear, &QPushButton::clicked, this, [this] {
    auto p = profile();
    p["macro"] = QJsonArray();
    setProfile(p);
    stopPreview();
    render();
  });
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
}
void Window::buildDesktop() {
  auto *v = addPage("Desktop");
  v->addWidget(designLabel("DESKTOP CONTROLS", "eyebrow"));
  v->addWidget(designLabel("Pointer & scrolling", "page-title"));
  auto *row = new QHBoxLayout;
  QVBoxLayout *pointer;
  auto *pointerPanel = designCard(&pointer, "Pointer & scrolling");
  auto *form = new QFormLayout;
  form->setSpacing(20);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  gain = new QDoubleSpinBox;
  gain->setRange(-1, 1);
  gain->setSingleStep(.1);
  scroll = new QDoubleSpinBox;
  scroll->setRange(.25, 11);
  scroll->setSingleStep(.25);
  scroll->setSuffix(" ×");
  wheelEnabled = new QCheckBox("Enable X11 scroll helper");
  form->addRow("Pointer acceleration", gain);
  form->addRow("Scroll multiplier", scroll);
  form->addRow(wheelEnabled);
  pointer->addLayout(form);
  desktopNote = designLabel("Reading desktop settings…", "muted");
  pointer->addWidget(desktopNote);
  auto *actions = new QHBoxLayout;
  auto *reload = pageButton("Reload", actions);
  actions->addStretch();
  auto *apply = pageButton("Apply desktop settings", actions, "primary");
  pointer->addLayout(actions);
  pointer->addStretch();
  row->addWidget(pointerPanel, 3);
  QVBoxLayout *info;
  auto *infoPanel = designCard(&info, "Your Linux desktop");
  info->addWidget(designLabel("These settings control pointer movement and "
                              "scrolling in your desktop session.",
                              "muted"));
  info->addWidget(rule());
  info->addWidget(designLabel(
      "KDE uses device-specific controls for the connected GM320. Other "
      "desktops can apply pointer changes across the session.",
      "muted"));
  info->addWidget(designLabel("Mouse DPI and button assignments are configured "
                              "separately in Performance and Customize.",
                              "muted"));
  info->addStretch();
  row->addWidget(infoPanel, 2);
  v->addLayout(row);
  v->addStretch();
  connect(reload, &QPushButton::clicked, this, &Window::refreshDesktop);
  connect(apply, &QPushButton::clicked, this, [this] {
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
}
void Window::syncPresentation() {
  auto p = profile(), s = p["settings"].toObject();
  viewer->setAssignments({s["button1"].toString(), s["button2"].toString(),
                          s["button3"].toString(), s["button4"].toString(),
                          s["button5"].toString(), s["button6"].toString(),
                          s["button7"].toString(), "Fire button"});
  dpiSummary->setText(
      QString::number(s["dpi"].toArray()[s["stage"].toInt()].toInt()));
  rateSummary->setText(QString::number(s["polling"].toInt()) +
                       " Hz stored report rate");
  lightSummary->setText(p["live"].toObject().isEmpty()
                            ? s["lighting"].toObject()["effect"].toString()
                            : p["live"].toObject()["effect"].toString());
  for (int i = 0; i < 6; ++i) {
    dpiTiles[i]->setProperty("editing", i == dpiEditing);
    dpiTiles[i]->setProperty("active", s["stage"].toInt() == i);
    repolish(dpiTiles[i]);
  }
  findChild<QLabel *>("editing-stage-label")
      ->setText("ADJUSTING STAGE " + QString::number(dpiEditing + 1));
  auto blockSet = [](auto *w, int value) {
    bool old = w->blockSignals(true);
    w->setValue(value);
    w->blockSignals(old);
  };
  blockSet(dpiSlider, dpi[dpiEditing]->currentIndex());
  blockSet(dpiEditor, dpi[dpiEditing]->currentData().toInt());
  int raw = s["brightness_raw"].toInt(),
      spd = s["lighting"].toObject()["speed_raw"].toInt();
  blockSet(brightnessSlider, qBound(0, 6 - raw, 6));
  blockSet(speedSlider, qMin(spd, 5));
  brightnessSlider->setEnabled(raw <= 6);
  speedSlider->setEnabled(spd <= 5);
  liveEffect->setEnabled(liveEnabled->isChecked());
  liveSpeed->setEnabled(liveEnabled->isChecked());
  auto *color = findChild<QPushButton *>("live-color");
  color->setEnabled(liveEnabled->isChecked());
  color->setText(liveColor.name().toUpper());
  color->setIcon(designIcon("lighting", liveColor));
  viewer->setLighting(s, p["live"].toObject());
  lightingViewer->setLighting(s, p["live"].toObject());
  macroEmpty->setVisible(p["macro"].toArray().isEmpty());
}
} // namespace mouse
