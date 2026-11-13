#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <QVector>
#include <functional>
#include <stdexcept>
namespace mouse {
using Image = QMap<int, int>;
using Parts = QVector<QVector<int>>;
struct Error : std::runtime_error {
  explicit Error(const QString &s) : std::runtime_error(s.toStdString()) {}
};
inline void require(bool ok, const QString &s) {
  if (!ok)
    throw Error(s);
}
extern const QString descriptor;
extern const QVector<int> dpis, codes, buttonAddresses;
extern const QStringList buttonNames, actions, modeNames, effects, responses,
    directions;
extern const QMap<QString, int> actionCodes;
extern const QMap<int, int> masks, rates;
int integer(QJsonValue v, int low, int high);
Image fromJson(const QJsonObject &);
QJsonObject toJson(const Image &);
void validate(const Image &);
QString revision(const Image &);
QJsonObject decode(const Image &);
Image merge(const Image &, const QJsonObject &changes);
Parts fragments(const QVector<int> &addresses);
Parts changedParts(const Image &, const Image &);
int restoreMask(const Image &, const Image &, int address);
Image restore(const Image &current, const Image &before, const Image &after);
Image fixture();
QVector<QByteArray> rgbReports(const QJsonObject &);
QByteArray experimentalReport(int brightness, int speed);
struct Memory {
  virtual ~Memory() = default;
  virtual Image snapshot() = 0;
  virtual void write(const QVector<int> &, const Image &) = 0;
  virtual void finalize() = 0;
  virtual void pause(int ms);
  Image stable();
  void verify(const Image &);
};
bool transact(Memory &, const Image &, const Image &,
              const std::function<void()> &backup = {}, bool activate = false);
} // namespace mouse
