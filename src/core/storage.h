#pragma once
#include "protocol.h"
namespace mouse {
QString configDir();
void privateDir(const QString &);
void saveJson(const QString &, const QJsonValue &);
QJsonValue readJson(const QString &, int limit = 100000);
void saveBackup(const QString &, const Image &, const Image &);
QPair<Image, Image> loadBackup(const QString &, const Image &current);
} // namespace mouse
