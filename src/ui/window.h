#pragma once
#include "drivers/device.h"
#include "platform/linux.h"
#include "viewer.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QFutureWatcher>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
namespace mouse {
class Window : public QMainWindow {
  Q_OBJECT
public:
  explicit Window(bool mock = false);
  ~Window() override;
  bool ready() const { return !busy && !current.isEmpty(); }
  bool verifyGui(const QString &directory);

protected:
  void closeEvent(QCloseEvent *) override;

private:
  friend class WindowTest;
  Device device;
  LinuxIntegration integration;
  bool busy = false, rendering = false, closeRequested = false,
       firstRead = true, haveSavedProfiles = false, shutdownComplete = false;
  QFutureWatcher<QJsonObject> watcher;
  std::function<void(QJsonObject)> completed;
  QJsonArray profiles, saved;
  int profileIndex = 0;
  QJsonObject current, baseline, desktop;
  QString revisionToken;
  QLabel *status, *connection, *desktopNote, *macroNote, *tester;
  QLabel *dpiSummary, *rateSummary, *lightSummary, *presetState, *fireNote,
      *assignmentName, *assignmentNumber, *macroEmpty;
  QVector<QPushButton *> navigation;
  QVector<QFrame *> assignmentRows;
  QVector<QFrame *> dpiTiles;
  QSlider *dpiSlider, *brightnessSlider, *speedSlider;
  QSpinBox *dpiEditor;
  int dpiEditing = 0;
  int selectedButton = 0;
  QFrame *assignmentPanel;
  QStackedWidget *assignmentStack;
  QToolButton *profileMenu;
  QComboBox *profileCombo, *polling, *effect, *direction, *liveEffect,
      *eventType, *eventKey, *trigger;
  QVector<QComboBox *> buttons, dpi, response;
  QVector<QCheckBox *> dpiEnabled, colorEnabled, responseEnabled;
  QVector<QRadioButton *> stage;
  QVector<QPushButton *> colors;
  QSpinBox *brightness, *speed, *timing, *eventDelay, *liveSpeed,
      *experimentalBrightness, *experimentalSpeed;
  QDoubleSpinBox *gain, *scroll;
  QCheckBox *symmetry, *wheelEnabled, *liveEnabled;
  QPushButton *applyButton, *saveButton, *undoButton, *restoreButton,
      *refreshButton, *macroApplyButton, *macroStopButton, *previewButton;
  QTabWidget *tabs;
  QListWidget *events;
  Viewer *viewer, *lightingViewer;
  QColor liveColor = Qt::red;
  QTimer previewTimer;
  int previewAt = 0;
  void build();
  QVBoxLayout *addPage(const QString &, bool canvas = false);
  void buildCustomize();
  void buildPerformance();
  void buildLighting();
  void buildMacros();
  void buildDesktop();
  void selectButton(int, bool openEditor = true);
  void syncPresentation();
  void render();
  void collect();
  void dirty();
  void saveProfiles();
  void loadMouse();
  void hardwareApply();
  void refreshDesktop();
  void enqueue(std::function<QJsonObject()> work,
               std::function<void(QJsonObject)> done);
  void message(const QString &, bool error = false);
  QJsonObject profile() const;
  void setProfile(QJsonObject);
  void importFile();
  void exportFile();
  void stopPreview();
};
} // namespace mouse
