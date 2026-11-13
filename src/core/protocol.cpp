// MIT: derived from Resa's bounded Python worker, Copyright 2026 Rajan Dhamala.
#include "protocol.h"
#include <QCryptographicHash>
#include <QSet>
#include <QThread>
#include <algorithm>
namespace mouse {
const QString descriptor =
    "0ec3eb2dc293bd8edcb490f5cee9ab272ebec8f4e0edc5f9f365f1bab53b2018";
const QVector<int> dpis = {200,  400,  600,  800,  1000,  1200,  1400, 1600,
                           1800, 2000, 2400, 3200, 4000,  4800,  5600, 6400,
                           7200, 8000, 8800, 9600, 10400, 11200, 12800};
const QVector<int> codes = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 12, 13,
                            14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
const QVector<int> buttonAddresses = {0x44, 0x46, 0x45, 0x48, 0x47, 0x49, 0x4a};
const QStringList buttonNames = {"Left button", "Right button", "Middle button",
                                 "Forward",     "Back",         "Top button 1",
                                 "Top button 2"};
const QStringList actions = {"Left click",  "Right click", "Middle click",
                             "Forward",     "Back",        "Rapid Fire",
                             "DPI Loop",    "DPI up",      "DPI down",
                             "Color cycle", "Disabled"};
const QMap<QString, int> actionCodes = {
    {"Left click", 1}, {"Right click", 3},  {"Middle click", 2}, {"Back", 4},
    {"Forward", 5},    {"Rapid Fire", 9},   {"DPI Loop", 6},     {"DPI up", 11},
    {"DPI down", 12},  {"Color cycle", 21}, {"Disabled", 0}};
const QMap<int, int> rates = {{1000, 0}, {500, 1}, {250, 3}, {125, 7}};
const QStringList modeNames = {"DPI Breathing Effect",
                               "Cycle Breathing Effect",
                               "Light On",
                               "Flowing Water Effect",
                               "Mono Water Effect",
                               "Comet Streak",
                               "Neon",
                               "Ambilight",
                               "Flicker",
                               "Starlight",
                               "Ripple",
                               "Enraptured"};
const QStringList effects = {modeNames[0], modeNames[1],  modeNames[2],
                             modeNames[3], modeNames[4],  modeNames[6],
                             modeNames[8], modeNames[11], "Button Response"};
const QStringList responses = {"Cycle Breathing Effect", "Flowing Water Effect",
                               "Comet Streak", "Mono Water Effect"};
const QStringList directions = {"Clockwise", "Anti Clockwise", "Two way",
                                "Overwrite"};
const QMap<int, int> masks = [] {
  QMap<int, int> m = {{0x13, 0x3f}, {0x15, 255},  {0x16, 255}, {0x17, 255},
                      {0x18, 7},    {0x19, 0x3f}, {0x1a, 255}, {0x1b, 255},
                      {0x1c, 0x7f}, {0x1d, 0x7f}, {0x1e, 255}, {0x33, 0x80},
                      {0x3a, 0x70}, {0x3e, 255},  {0x3f, 0x7f}};
  for (int a = 0x20; a < 0x32; ++a)
    m[a] = 0xf0;
  for (int a : buttonAddresses)
    m[a] = 255;
  return m;
}();
int integer(QJsonValue v, int low, int high) {
  require(v.isDouble() && v.toDouble() == v.toInt() && v.toInt() >= low &&
              v.toInt() <= high,
          "Setting is outside its supported range");
  return v.toInt();
}
static void fields(const QJsonObject &v, const QStringList &allowed,
                   bool nonempty = true) {
  require(!nonempty || !v.isEmpty(), "Choose a supported settings section");
  for (auto i = v.begin(); i != v.end(); ++i)
    require(allowed.contains(i.key()),
            "Unsupported settings field: " + i.key());
}
static QJsonArray array(QJsonValue v, int n) {
  require(v.isArray() && v.toArray().size() == n,
          "Unexpected number of settings values");
  return v.toArray();
}
static bool boolean(QJsonValue v) {
  require(v.isBool(), "Invalid boolean setting");
  return v.toBool();
}
void validate(const Image &i) {
  require(i.keys() == masks.keys(), "Unexpected settings snapshot");
  for (int v : i)
    require(v >= 0 && v <= 255, "Invalid snapshot byte");
}
Image fromJson(const QJsonObject &j) {
  Image i;
  for (auto it = j.begin(); it != j.end(); ++it) {
    bool ok = false;
    int a = it.key().toInt(&ok);
    require(ok && QString::number(a) == it.key(),
            "Invalid address in snapshot");
    i[a] = integer(it.value(), 0, 255);
  }
  validate(i);
  return i;
}
QJsonObject toJson(const Image &i) {
  QJsonObject j;
  for (auto it = i.begin(); it != i.end(); ++it)
    j[QString::number(it.key())] = it.value();
  return j;
}
QString revision(const Image &i) {
  validate(i);
  QByteArray b;
  for (int v : i)
    b.append(char(v));
  return QString::fromLatin1(
      QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex());
}
static QJsonObject lighting(const Image &i) {
  int mode = ((i[0x1c] >> 4) & 7) | ((i[0x33] >> 7) << 3);
  QString e = (i[0x1c] & 8) && mode == 1                  ? "Button Response"
              : !(i[0x1c] & 8) && mode < modeNames.size() ? modeNames[mode]
                                                          : "Unrecognized";
  QJsonArray palette, enabled, resp, ren;
  for (int n = 0; n < 6; ++n) {
    palette.append(QJsonArray{i[0x20 + n * 3] >> 4, i[0x21 + n * 3] >> 4,
                              i[0x22 + n * 3] >> 4});
  }
  palette.append(QJsonArray{i[0x3e] >> 4, i[0x3e] & 15, i[0x3f] & 15});
  for (int n = 0; n < 7; ++n) {
    enabled.append(bool(i[0x1d] & (1 << n)));
    resp.append(responses[i[buttonAddresses[n]] >> 6]);
    ren.append(bool(i[buttonAddresses[n]] & 32));
  }
  return {{"effect", e},
          {"speed_raw", i[0x1c] & 7},
          {"palette", palette},
          {"enabled", enabled},
          {"responses", resp},
          {"response_enabled", ren},
          {"direction", directions[(i[0x3a] >> 4) & 3]},
          {"symmetry", bool(i[0x3a] & 64)}};
}
QJsonObject decode(const Image &i) {
  validate(i);
  QJsonArray vals, en;
  for (int n = 0; n < 6; ++n) {
    int c = ((i[0x15 + n / 2] >> (n % 2 ? 0 : 4)) & 15) |
            (((i[0x13] >> n) & 1) << 4);
    require(codes.contains(c), "Unrecognized DPI code in settings readback");
    vals.append(dpis[codes.indexOf(c)]);
    en.append(bool(i[0x19] & (1 << n)));
  }
  int stage = i[0x18] & 7;
  require(stage < 6 && en[stage].toBool() && rates.values().contains(i[0x1e]),
          "Mouse settings do not match captured configuration");
  QJsonObject j = {{"dpi", vals},
                   {"enabled", en},
                   {"stage", stage},
                   {"polling", rates.key(i[0x1e])},
                   {"brightness_raw", (i[0x3f] >> 4) & 7},
                   {"double_click_ms", i[0x1a] | i[0x1b] << 8},
                   {"lighting", lighting(i)}};
  for (int n = 0; n < 7; ++n) {
    int c = i[buttonAddresses[n]] & 31;
    require(actionCodes.values().contains(c), "Unrecognized mouse action");
    j["button" + QString::number(n + 1)] = actionCodes.key(c);
  }
  return j;
}
Image merge(const Image &i, const QJsonObject &c) {
  decode(i);
  QStringList allowed = {"dpi", "polling", "brightness_raw", "lighting",
                         "double_click_ms"};
  for (int n = 1; n <= 7; ++n)
    allowed << "button" + QString::number(n);
  fields(c, allowed);
  Image a = i;
  if (c.contains("dpi")) {
    require(c["dpi"].isObject(), "Invalid DPI settings");
    auto d = c["dpi"].toObject();
    require(d.keys() == QStringList({"enabled", "stage", "values"}),
            "Invalid DPI settings");
    auto v = array(d["values"], 6), en = array(d["enabled"], 6);
    int stage = integer(d["stage"], 0, 5);
    require(boolean(en[stage]), "The selected DPI stage must be enabled");
    int high = 0, enabled = 0;
    QVector<int> co;
    for (int n = 0; n < 6; ++n) {
      int val = integer(v[n], 200, 12800);
      require(dpis.contains(val), "Unsupported DPI value");
      int code = codes[dpis.indexOf(val)];
      co << code;
      high |= ((code >> 4) & 1) << n;
      enabled |= int(boolean(en[n])) << n;
    }
    a[0x13] = (i[0x13] & ~0x3f) | high;
    a[0x18] = (i[0x18] & ~7) | stage;
    a[0x19] = (i[0x19] & ~0x3f) | enabled;
    for (int n = 0; n < 3; ++n)
      a[0x15 + n] = ((co[n * 2] & 15) << 4) | (co[n * 2 + 1] & 15);
  }
  if (c.contains("polling")) {
    int r = integer(c["polling"], 125, 1000);
    require(rates.contains(r), "Choose 125, 250, 500 or 1000 Hz");
    a[0x1e] = rates[r];
  }
  bool buttons = false;
  for (int n = 0; n < 7; ++n) {
    QString key = "button" + QString::number(n + 1);
    if (c.contains(key)) {
      require(c[key].isString() && actionCodes.contains(c[key].toString()),
              "Unsupported button action");
      a[buttonAddresses[n]] =
          (a[buttonAddresses[n]] & 0xe0) | actionCodes[c[key].toString()];
      buttons = true;
    }
  }
  if (c.contains("brightness_raw"))
    a[0x3f] = (i[0x3f] & ~0x70) | (integer(c["brightness_raw"], 0, 6) << 4);
  if (c.contains("double_click_ms")) {
    int v = integer(c["double_click_ms"], 8, 300);
    a[0x1a] = v & 255;
    a[0x1b] = v >> 8;
  }
  if (c.contains("lighting")) {
    require(c["lighting"].isObject(), "Invalid lighting changes");
    auto l = c["lighting"].toObject();
    fields(l, {"effect", "speed_raw", "palette", "enabled", "responses",
               "response_enabled", "direction", "symmetry"});
    if (l.contains("effect")) {
      QString e = l["effect"].toString();
      require(l["effect"].isString() && effects.contains(e),
              "Choose an available captured effect");
      int mode = e == "Button Response" ? 1 : modeNames.indexOf(e);
      a[0x1c] = (a[0x1c] & ~0x78) | ((mode & 7) << 4) |
                (e == "Button Response" ? 8 : 0);
      a[0x33] = (a[0x33] & 127) | ((mode >> 3) << 7);
    }
    if (l.contains("speed_raw"))
      a[0x1c] = (a[0x1c] & ~7) | integer(l["speed_raw"], 0, 5);
    if (l.contains("palette")) {
      auto p = array(l["palette"], 7);
      for (int n = 0; n < 7; ++n) {
        auto rgb = array(p[n], 3);
        for (int ch = 0; ch < 3; ++ch)
          integer(rgb[ch], 0, 15);
        if (n < 6) {
          for (int ch = 0; ch < 3; ++ch) {
            int ad = 0x20 + n * 3 + ch;
            a[ad] = (a[ad] & 15) | (rgb[ch].toInt() << 4);
          }
        } else {
          a[0x3e] = (rgb[0].toInt() << 4) | rgb[1].toInt();
          a[0x3f] = (a[0x3f] & 0xf0) | rgb[2].toInt();
        }
      }
    }
    if (l.contains("enabled")) {
      auto en = array(l["enabled"], 7);
      int bits = 0;
      for (int n = 0; n < 7; ++n)
        bits |= int(boolean(en[n])) << n;
      require(bits, "Keep at least one color slot enabled");
      a[0x1d] = (a[0x1d] & 128) | bits;
    }
    if (l.contains("responses")) {
      auto v = array(l["responses"], 7);
      for (int n = 0; n < 7; ++n) {
        require(v[n].isString() && responses.contains(v[n].toString()),
                "Unsupported response effect");
        a[buttonAddresses[n]] = (a[buttonAddresses[n]] & 0x3f) |
                                (responses.indexOf(v[n].toString()) << 6);
      }
    }
    if (l.contains("response_enabled")) {
      auto v = array(l["response_enabled"], 7);
      for (int n = 0; n < 7; ++n)
        a[buttonAddresses[n]] =
            (a[buttonAddresses[n]] & ~32) | (int(boolean(v[n])) << 5);
    }
    if (l.contains("direction")) {
      require(l["direction"].isString() &&
                  directions.contains(l["direction"].toString()),
              "Unsupported direction");
      a[0x3a] = (a[0x3a] & ~0x30) |
                (directions.indexOf(l["direction"].toString()) << 4);
    }
    if (l.contains("symmetry"))
      a[0x3a] = (a[0x3a] & ~64) | (int(boolean(l["symmetry"])) << 6);
    if (l.contains("effect") || l.contains("direction"))
      require(lighting(a)["effect"] != "Mono Water Effect" ||
                  (a[0x3a] & 0x30) != 0x30,
              "Mono Water supports Clockwise, Anti Clockwise or Two way");
  }
  if (buttons) {
    bool left = false;
    for (int ad : buttonAddresses)
      left |= (a[ad] & 31) == 1;
    require(left, "Keep at least one button assigned to Left click");
  }
  decode(a);
  return a;
}
Parts fragments(const QVector<int> &addresses) {
  auto sorted = addresses;
  std::sort(sorted.begin(), sorted.end());
  Parts parts;
  for (int a : sorted) {
    if (!parts.isEmpty() && parts.last().last() + 1 == a &&
        parts.last().size() < 5)
      parts.last() << a;
    else
      parts << QVector<int>{a};
  }
  return parts;
}
Parts changedParts(const Image &b, const Image &a) {
  QVector<int> addresses;
  for (int ad : masks.keys())
    if (b[ad] != a[ad])
      addresses << ad;
  return fragments(addresses);
}
int restoreMask(const Image &b, const Image &a, int ad) {
  if (ad == 0x1a || ad == 0x1b)
    return b[0x1a] != a[0x1a] || b[0x1b] != a[0x1b] ? 255 : 0;
  QVector<int> fields = {masks[ad]};
  if (ad == 0x1c)
    fields = {0x78, 7};
  if (ad == 0x1d)
    fields = {1, 2, 4, 8, 16, 32, 64};
  if (ad == 0x3a)
    fields = {0x30, 64};
  if (ad == 0x3e)
    fields = {0xf0, 15};
  if (ad == 0x3f)
    fields = {0x70, 15};
  if (buttonAddresses.contains(ad))
    fields = {31, 32, 0xc0};
  int m = 0;
  for (int f : fields)
    if ((b[ad] ^ a[ad]) & f)
      m |= f;
  return m;
}
Image restore(const Image &current, const Image &before, const Image &after) {
  decode(current);
  decode(before);
  decode(after);
  Image out = current;
  for (int a : masks.keys()) {
    int m = restoreMask(before, after, a);
    out[a] = (current[a] & ~m) | (before[a] & m);
  }
  decode(out);
  return out;
}
Image fixture() {
  Image i;
  for (int a : masks.keys())
    i[a] = 0;
  Image observed = {{0x13, 240}, {0x15, 228}, {0x16, 111}, {0x17, 40},
                    {0x18, 152}, {0x19, 189}, {0x1e, 0},   {0x3f, 131},
                    {0x44, 33},  {0x45, 34},  {0x46, 35},  {0x47, 36},
                    {0x48, 37},  {0x49, 38},  {0x4a, 53},  {0x1c, 149},
                    {0x1d, 255}, {0x33, 31},  {0x3a, 11},  {0x3e, 143}};
  for (auto it = observed.begin(); it != observed.end(); ++it)
    i[it.key()] = it.value();
  return i;
}
static QByteArray packet(std::initializer_list<int> values) {
  QByteArray out;
  for (int v : values)
    out.append(char(v));
  return out;
}
QVector<QByteArray> rgbReports(const QJsonObject &r) {
  require(r.keys() == QStringList({"color", "effect", "speed"}),
          "Invalid lighting settings");
  QString e = r["effect"].toString(), c = r["color"].toString();
  require(QStringList({"Static", "Breathing", "Off"}).contains(e),
          "Unsupported live effect");
  int speed = integer(r["speed"], 0, 5);
  require(c.size() == 7 && c[0] == '#', "Choose an RGB color");
  bool ok = false;
  uint rgb = c.mid(1).toUInt(&ok, 16);
  require(ok, "Choose an RGB color");
  int red = e == "Off" ? 15 : 15 - ((rgb >> 16) & 255) / 16,
      green = e == "Off" ? 15 : 15 - ((rgb >> 8) & 255) / 16,
      blue = e == "Off" ? 15 : 15 - (rgb & 255) / 16;
  QVector<QByteArray> reports;
  for (int n = 0; n < 6; ++n)
    reports << packet({7, 0x14, n << 5 | green, red << 4 | blue, 0, 0, 0, 0});
  reports << packet({7, 0x13, 255, (e == "Breathing" ? 8 : 10) << 4 | speed, 11,
                     e == "Off" ? 15 : 8, 0, 0});
  return reports;
}
QByteArray experimentalReport(int b, int s) {
  require(b >= 0 && b <= 7 && s >= 0 && s <= 7,
          "Invalid experimental lighting");
  return packet({7, 0x13, 255, 0xf0 | s, 11, b, 0, 2});
}
void Memory::pause(int ms) { QThread::msleep(ms); }
Image Memory::stable() {
  Image p = snapshot();
  for (int n = 0; n < 5; ++n) {
    Image i = snapshot();
    if (i == p)
      return i;
    p = i;
  }
  throw Error("Mouse settings are still changing; release DPI and read again");
}
void Memory::verify(const Image &expected) {
  Image p;
  for (int n = 0; n < 8; ++n) {
    Image i = snapshot();
    if (i == p && i == expected)
      return;
    p = i;
    pause(25);
  }
  throw Error("Requested settings did not settle in readback");
}
bool transact(Memory &m, const Image &b, const Image &a,
              const std::function<void()> &backup, bool activate) {
  decode(b);
  decode(a);
  auto parts = changedParts(b, a);
  if (parts.isEmpty()) {
    if (activate) {
      try {
        m.finalize();
        m.verify(a);
      } catch (const std::exception &e) {
        throw Error("Lighting activation could not be confirmed. Read before "
                    "retrying. " +
                    QString::fromUtf8(e.what()));
      }
      return true;
    }
    return false;
  }
  bool left = false, unchangedLeft = false;
  for (int ad : buttonAddresses) {
    left |= (a[ad] & 31) == 1;
    unchangedLeft |= (a[ad] & 31) == 1 && (b[ad] & 31) == 1;
  }
  require(left, "Keep at least one Left click");
  if (!unchangedLeft)
    std::stable_sort(
        parts.begin(), parts.end(), [&](const auto &x, const auto &y) {
          auto has = [&](const auto &p) {
            for (int ad : p)
              if (buttonAddresses.contains(ad) && (a[ad] & 31) == 1)
                return true;
            return false;
          };
          return has(x) > has(y);
        });
  for (int ad : masks.keys())
    require(!((b[ad] ^ a[ad]) & ~masks[ad]), "Unrelated settings would change");
  if (backup)
    backup();
  Parts attempted;
  try {
    for (const auto &part : parts) {
      attempted << part;
      m.write(part, a);
    }
    m.finalize();
    m.verify(a);
  } catch (const std::exception &e) {
    QString reason = QString::fromUtf8(e.what());
    for (auto it = attempted.crbegin(); it != attempted.crend(); ++it) {
      try {
        m.write(*it, b);
      } catch (const std::exception &) {
      }
    }
    try {
      m.finalize();
      m.verify(b);
    } catch (const std::exception &re) {
      throw Error("Update failed and restoration could not be verified. "
                  "Reconnect, read, then Restore last change. Update: " +
                  reason + " Restoration: " + QString::fromUtf8(re.what()));
    }
    throw Error("Update failed; original settings restored and read back. " +
                reason);
  }
  return true;
}
} // namespace mouse
