#include "profile.h"
#include <QRegularExpression>
#include <QSet>
namespace mouse {
const QStringList macroKeys = [] {
  QStringList k;
  for (char c = 'A'; c <= 'Z'; ++c)
    k << QString(QChar(c));
  for (int n = 0; n < 10; ++n)
    k << QString::number(n);
  for (int n = 1; n <= 12; ++n)
    k << "F" + QString::number(n);
  k << "Enter" << "Escape" << "Backspace" << "Tab" << "Space" << "Left"
    << "Right" << "Up" << "Down" << "Home" << "End" << "PageUp" << "PageDown"
    << "Delete" << "Insert" << "Ctrl" << "Shift" << "Alt" << "Super";
  return k;
}();
static QString symbol(const QString &k) {
  const QMap<QString, QString> special = {{"Enter", "KEY_ENTER"},
                                          {"Escape", "KEY_ESC"},
                                          {"Backspace", "KEY_BACKSPACE"},
                                          {"Tab", "KEY_TAB"},
                                          {"Space", "KEY_SPACE"},
                                          {"Left", "KEY_LEFT"},
                                          {"Right", "KEY_RIGHT"},
                                          {"Up", "KEY_UP"},
                                          {"Down", "KEY_DOWN"},
                                          {"Home", "KEY_HOME"},
                                          {"End", "KEY_END"},
                                          {"PageUp", "KEY_PAGEUP"},
                                          {"PageDown", "KEY_PAGEDOWN"},
                                          {"Delete", "KEY_DELETE"},
                                          {"Insert", "KEY_INSERT"},
                                          {"Ctrl", "KEY_LEFTCTRL"},
                                          {"Shift", "KEY_LEFTSHIFT"},
                                          {"Alt", "KEY_LEFTALT"},
                                          {"Super", "KEY_LEFTMETA"}};
  require(macroKeys.contains(k), "Unsupported macro key");
  return special.value(k, "KEY_" + k);
}
bool validEvents(const QJsonArray &events) {
  if (events.size() > 64)
    return false;
  try {
    for (auto v : events) {
      require(v.isObject(), "Invalid event");
      auto e = v.toObject();
      QString t = e["type"].toString();
      if (t == "delay") {
        require(e.keys() == QStringList({"ms", "type"}), "Invalid delay");
        integer(e["ms"], 10, 10000);
      } else
        require((t == "down" || t == "up") &&
                    e.keys() == QStringList({"key", "type"}) &&
                    macroKeys.contains(e["key"].toString()),
                "Invalid key event");
    }
    return true;
  } catch (...) {
    return false;
  }
}
QString macroExpression(const QJsonArray &events, bool allowEmpty) {
  require(validEvents(events), "Invalid macro sequence");
  if (events.isEmpty() && allowEmpty)
    return {};
  require(!events.isEmpty(), "Add a sequence first");
  QStringList parts;
  QSet<QString> held;
  int duration = 0, keys = 0;
  for (auto v : events) {
    auto e = v.toObject();
    QString t = e["type"].toString(), k = e["key"].toString();
    if (t == "delay") {
      duration += e["ms"].toInt();
      parts << QString("wait(%1)").arg(e["ms"].toInt());
    } else {
      ++keys;
      if (t == "down") {
        require(!held.contains(k), "Release " + k + " before pressing again");
        held.insert(k);
      } else
        require(held.remove(k), "Press " + k + " before releasing");
      parts << "key_" + t + "(" + symbol(k) + ")";
    }
  }
  require(keys > 0 && held.isEmpty(),
          "Every key press needs a matching release");
  require(duration <= 30000, "Keep each sequence within 30 seconds");
  return parts.join('.');
}
QJsonObject defaultProfile() {
  return {{"settings", decode(fixture())},
          {"macro", QJsonArray()},
          {"trigger", 275},
          {"live", QJsonObject()}};
}
QJsonObject changesFor(const QJsonObject &desired, const QJsonObject &current) {
  QJsonObject changes;
  QStringList allowed = {"dpi",     "enabled",        "stage",
                         "polling", "brightness_raw", "double_click_ms",
                         "lighting"};
  for (int n = 1; n <= 7; ++n)
    allowed << "button" + QString::number(n);
  require(desired.keys() == current.keys(), "Unexpected profile settings");
  for (const auto &k : desired.keys())
    require(allowed.contains(k), "Unexpected profile field");
  if (desired["dpi"] != current["dpi"] ||
      desired["enabled"] != current["enabled"] ||
      desired["stage"] != current["stage"])
    changes["dpi"] = QJsonObject{{"values", desired["dpi"]},
                                 {"enabled", desired["enabled"]},
                                 {"stage", desired["stage"]}};
  for (const auto &k : allowed) {
    if (QStringList({"dpi", "enabled", "stage", "lighting"}).contains(k))
      continue;
    if (desired[k] != current[k])
      changes[k] = desired[k];
  }
  QJsonObject light;
  auto a = desired["lighting"].toObject(), b = current["lighting"].toObject();
  require(a.keys() == b.keys(), "Unexpected lighting settings");
  for (auto it = a.begin(); it != a.end(); ++it)
    if (it.value() != b[it.key()])
      light[it.key()] = it.value();
  if (!light.isEmpty())
    changes["lighting"] = light;
  return changes;
}
QJsonObject hardwareChanges(const QJsonObject &profile,
                            const QJsonObject &current) {
  auto desired = profile["settings"].toObject();
  if (profile["live"].toObject()["effect"] == "Off") {
    auto light = desired["lighting"].toObject();
    QJsonArray off;
    for (int n = 0; n < 7; ++n)
      off.append(false);
    light["response_enabled"] = off;
    if (light["effect"] == "Button Response")
      light["effect"] = "Light On";
    desired["lighting"] = light;
    desired["brightness_raw"] = current["brightness_raw"];
  }
  return changesFor(desired, current);
}
QJsonObject rebaseSettings(const QJsonObject &desired,
                           const QJsonObject &previous,
                           const QJsonObject &fresh) {
  auto out = desired;
  bool dpiUnchanged = true;
  for (auto key : {"dpi", "enabled", "stage"})
    dpiUnchanged &= desired[key] == previous[key];
  for (auto it = fresh.begin(); it != fresh.end(); ++it) {
    if (QStringList({"dpi", "enabled", "stage"}).contains(it.key())) {
      if (dpiUnchanged)
        out[it.key()] = it.value();
    } else if (it.key() == "lighting") {
      auto a = desired["lighting"].toObject(),
           b = previous["lighting"].toObject(), f = it.value().toObject();
      for (auto li = f.begin(); li != f.end(); ++li)
        if (a[li.key()] == b[li.key()])
          a[li.key()] = li.value();
      out["lighting"] = a;
    } else if (desired[it.key()] == previous[it.key()])
      out[it.key()] = it.value();
  }
  return out;
}
void validateProfile(const QJsonObject &p) {
  require(p.keys() == QStringList({"live", "macro", "settings", "trigger"}) &&
              p["settings"].isObject() && p["macro"].isArray() &&
              p["live"].isObject(),
          "Invalid profile");
  require(validEvents(p["macro"].toArray()), "Invalid saved macro events");
  integer(p["trigger"], 274, 276);
  auto s = p["settings"].toObject(), reference = decode(fixture());
  require(s.keys() == reference.keys(), "Unexpected profile settings");
  auto sized = [](QJsonValue v, int n) {
    require(v.isArray() && v.toArray().size() == n, "Invalid profile array");
    return v.toArray();
  };
  auto vals = sized(s["dpi"], 6), enabled = sized(s["enabled"], 6);
  int stage = integer(s["stage"], 0, 5);
  for (int n = 0; n < 6; ++n) {
    require(dpis.contains(integer(vals[n], 200, 12800)),
            "Unsupported profile DPI");
    require(enabled[n].isBool(), "Invalid DPI enable state");
  }
  require(enabled[stage].toBool(), "The active DPI stage must be enabled");
  require(rates.contains(integer(s["polling"], 125, 1000)),
          "Unsupported profile polling");
  integer(s["double_click_ms"], 0, 65535);
  integer(s["brightness_raw"], 0, 7);
  bool left = false;
  for (int n = 1; n <= 7; ++n) {
    auto v = s["button" + QString::number(n)];
    require(v.isString() && actions.contains(v.toString()),
            "Unsupported profile button");
    left |= v == "Left click";
  }
  require(left, "Keep at least one Left click in the profile");
  require(s["lighting"].isObject(), "Invalid lighting profile");
  auto l = s["lighting"].toObject();
  require(l.keys() == reference["lighting"].toObject().keys(),
          "Unexpected lighting fields");
  require(l["effect"].isString() &&
              (modeNames.contains(l["effect"].toString()) ||
               l["effect"] == "Button Response" ||
               l["effect"] == "Unrecognized"),
          "Invalid profile effect");
  integer(l["speed_raw"], 0, 7);
  for (auto v : sized(l["palette"], 7)) {
    for (auto channel : sized(v, 3))
      integer(channel, 0, 15);
  }
  for (auto key : {"enabled", "response_enabled"})
    for (auto v : sized(l[key], 7))
      require(v.isBool(), "Invalid lighting enable state");
  for (auto v : sized(l["responses"], 7))
    require(v.isString() && responses.contains(v.toString()),
            "Invalid button response");
  require(l["direction"].isString() &&
              directions.contains(l["direction"].toString()) &&
              l["symmetry"].isBool(),
          "Invalid lighting direction/symmetry");
  auto live = p["live"].toObject();
  if (!live.isEmpty())
    rgbReports(live);
}

QJsonArray importProfiles(const QJsonObject &root) {
  require(root["profiles"].isArray() && root["profiles"].toArray().size() == 4,
          "Expected four local profiles");
  QJsonArray out;
  if (root["version"] == 2) {
    for (auto v : root["profiles"].toArray()) {
      require(v.isObject(), "Invalid profile");
      validateProfile(v.toObject());
      out.append(v);
    }
    return out;
  }
  require(root["version"] == 1, "Unsupported profile format");
  for (auto v : root["profiles"].toArray()) {
    auto p = v.toObject();
    QJsonObject s = decode(fixture());
    require(p["buttons"].isArray() && p["buttons"].toArray().size() == 7,
            "Invalid legacy buttons");
    for (int n = 0; n < 7; ++n)
      s["button" + QString::number(n + 1)] = p["buttons"].toArray()[n];
    s["dpi"] = p["dpi"];
    s["enabled"] = p["enabled"];
    s["stage"] = p["stage"];
    s["polling"] = p["polling"].isUndefined() ? QJsonValue(1000)
                                              : QJsonValue(p["polling"]);
    if (!p["doubleClickMs"].isUndefined())
      s["double_click_ms"] = p["doubleClickMs"];
    auto old = p["lighting"].toObject();
    require(!old.isEmpty(), "Invalid legacy lighting");
    s["brightness_raw"] =
        6 - qRound(integer(old["brightness"], 0, 100) * 6. / 100);
    auto l = s["lighting"].toObject();
    QString effect = old["effect"].toString();
    QJsonObject live;
    if (QStringList({"Static", "Breathing", "Off"}).contains(effect))
      live = {{"effect", effect},
              {"color", old["color"]},
              {"speed", qMin(5, qRound(integer(old["speed"], 0, 140) / 20.))}};
    else {
      require(modeNames.contains(effect) || effect == "Button Response" ||
                  effect == "Unrecognized",
              "Unknown legacy lighting effect");
      l["effect"] = effect;
      l["speed_raw"] = qRound(integer(old["speed"], 0, 140) / 20.);
    }
    if (old["palette"].isArray()) {
      auto a = old["palette"].toArray();
      require(a.size() == 7, "Invalid legacy palette");
      QJsonArray palette;
      for (auto color : a) {
        QString c = color.toString();
        require(QRegularExpression("^#[a-fA-F0-9]{6}$").match(c).hasMatch(),
                "Invalid legacy color");
        uint rgb = c.mid(1).toUInt(nullptr, 16);
        palette.append(QJsonArray{15 - qRound(int((rgb >> 16) & 255) / 17.),
                                  15 - qRound(int((rgb >> 8) & 255) / 17.),
                                  15 - qRound(int(rgb & 255) / 17.)});
      }
      l["palette"] = palette;
    }
    for (const auto &pair : QVector<QPair<QString, QString>>{
             {"slots", "enabled"},
             {"responses", "responses"},
             {"responseEnabled", "response_enabled"},
             {"direction", "direction"},
             {"symmetry", "symmetry"}})
      if (old.contains(pair.first))
        l[pair.second] = old[pair.first];
    s["lighting"] = l;
    QJsonObject next = {{"settings", s},
                        {"macro", p["macro"]},
                        {"trigger", p["macroTrigger"].isUndefined()
                                        ? QJsonValue(275)
                                        : QJsonValue(p["macroTrigger"])},
                        {"live", live}};
    validateProfile(next);
    out.append(next);
  }
  return out;
}
} // namespace mouse
