#pragma once
#include "core/profile.h"
#include <sys/types.h>
namespace mouse {
struct LinuxIntegration {
  bool mock = false;
  QString activeGroup;
  int activeProfile = -1;
  pid_t wheelPid = -1;
  QString wheelSchema() const;
  explicit LinuxIntegration(bool mock = false) : mock(mock) {}
  QJsonObject desktopRead();
  QJsonObject desktopApply(const QString &revision, double gain, double scroll,
                           bool wheelEnabled);
  QJsonObject macroApply(int profile, int trigger, const QJsonArray &events);
  void macroStop();
  void close();
  QString macroState(const QString &group);
  QString macroGroup();
};
QString run(const QString &, const QStringList &, int timeout = 6000);
} // namespace mouse
