#pragma once
#include "protocol.h"
namespace mouse {
extern const QStringList macroKeys;
QString macroExpression(const QJsonArray &events, bool allowEmpty = false);
bool validEvents(const QJsonArray &events);
QJsonObject defaultProfile();
void validateProfile(const QJsonObject &);
QJsonArray importProfiles(const QJsonObject &);
QJsonObject hardwareChanges(const QJsonObject &profile,
                            const QJsonObject &current);
QJsonObject rebaseSettings(const QJsonObject &desired,
                           const QJsonObject &previous,
                           const QJsonObject &fresh);
QJsonObject changesFor(const QJsonObject &desired, const QJsonObject &current);
} // namespace mouse
