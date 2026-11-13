#pragma once
#include "core/protocol.h"
#include <QJsonArray>
#include <memory>
namespace mouse {
struct DeviceInfo {
  QString node, sysfs, product, name, interface, hash;
  int vendor = 0, productId = 0;
  bool supported = false;
  QJsonObject json() const;
};
QVector<DeviceInfo> discover();
struct Device {
  bool mock = false;
  Image mockImage = fixture();
  explicit Device(bool mock = false) : mock(mock) {}
  QJsonObject operation(const QString &action, const QJsonObject &request);
};
} // namespace mouse
