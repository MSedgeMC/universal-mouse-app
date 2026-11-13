#include "device.h"
#include "core/storage.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <cstring>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace mouse {
static QString read(const QString &p) {
  QFile f(p);
  return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed()
                                     : QString();
}
static QString ancestor(const QString &p, const QString &field) {
  QDir d(p);
  for (int n = 0; n < 18; ++n) {
    QString f = d.filePath(field);
    if (QFile::exists(f))
      return read(f);
    if (!d.cdUp())
      break;
  }
  return {};
}
QJsonObject DeviceInfo::json() const {
  return {{"node", node},
          {"product", product},
          {"name", name},
          {"interface", interface},
          {"vendor", vendor},
          {"product_id", productId},
          {"descriptor_sha256", hash},
          {"supported", supported}};
}
QVector<DeviceInfo> discover() {
  QVector<DeviceInfo> result;
  QDir dir("/sys/class/hidraw");
  for (const auto &name : dir.entryList({"hidraw*"}, QDir::Dirs)) {
    QString p = QFileInfo(dir.filePath(name) + "/device").canonicalFilePath();
    if (p.isEmpty())
      continue;
    QString event = read(p + "/uevent");
    QRegularExpression id(
        "HID_ID=([0-9a-fA-F]+):([0-9a-fA-F]+):([0-9a-fA-F]+)");
    auto match = id.match(event);
    if (!match.hasMatch())
      continue;
    DeviceInfo info;
    info.node = "/dev/" + name;
    info.sysfs = p;
    info.vendor = match.captured(2).toInt(nullptr, 16);
    info.productId = match.captured(3).toInt(nullptr, 16);
    info.product = ancestor(p, "product");
    info.interface = ancestor(p, "bInterfaceNumber");
    info.name =
        QRegularExpression("HID_NAME=([^\n]*)").match(event).captured(1);
    QFile desc(p + "/report_descriptor");
    if (desc.open(QIODevice::ReadOnly))
      info.hash = QString::fromLatin1(
          QCryptographicHash::hash(desc.readAll(), QCryptographicHash::Sha256)
              .toHex());
    info.supported = match.captured(1).toInt(nullptr, 16) == 3 &&
                     info.vendor == 0x30fa && info.productId == 0x1440 &&
                     info.interface == "01" &&
                     QStringList({"FANTECH-X16v2", "USB GAMING MOUSE"})
                         .contains(info.product) &&
                     info.hash == descriptor;
    result << info;
  }
  return result;
}
struct Fd {
  int n = -1;
  ~Fd() {
    if (n >= 0)
      close(n);
  }
  Fd() = default;
  Fd(const Fd &) = delete;
};
struct HidMemory : Memory {
  int fd;
  explicit HidMemory(int f) : fd(f) {}
  void feature(QByteArray p) {
    require(p.size() == 8, "Invalid feature length");
    require(ioctl(fd, HIDIOCSFEATURE(8), p.data()) == 8,
            "Incomplete HID feature transfer");
  }
  void send(int op, int base = 0, int length = 1, int offset = 0,
            int value = 0) {
    require(QVector<int>({0, 3, 5, 9}).contains(op), "Unsupported transaction");
    if (op) {
      require(length >= 1 && length <= 5 && offset >= 0 && offset < length,
              "Invalid transaction length");
      for (int n = 0; n < length; ++n)
        require(masks.contains(base + n),
                "Transaction outside settings fields");
    }
    QByteArray p(8, 0);
    int v[] = {7, 0x18, op, offset, base & 255, base >> 8, value, length - 1};
    for (int n = 0; n < 8; ++n)
      p[n] = char(v[n]);
    feature(p);
  }
  void finalize() override {
    try {
      feature(QByteArray::fromHex("0718100000000000"));
      pause(5);
    } catch (...) {
      try {
        send(0);
      } catch (...) {
      }
      throw;
    }
    send(0);
  }
  Image snapshot() override {
    Image out;
    for (const auto &part : fragments(masks.keys().toVector())) {
      int b = part[0], len = part.size();
      try {
        for (int n = 0; n < len; ++n) {
          send(3, b, len, n);
          pause(2);
        }
        pause(2);
        send(0, b);
        for (int n = 0; n < len; ++n) {
          send(5, b, len, n);
          pause(2);
          QByteArray r(8, 0);
          r[0] = 7;
          require(ioctl(fd, HIDIOCGFEATURE(8), r.data()) == 8 && r[0] == 7,
                  "Invalid settings readback");
          out[b + n] = quint8(r[1]);
        }
      } catch (...) {
        try {
          send(0);
        } catch (...) {
        }
        throw;
      }
      send(0);
    }
    validate(out);
    return out;
  }
  void write(const QVector<int> &part, const Image &image) override {
    int b = part[0], len = part.size();
    try {
      for (int n = 0; n < len; ++n)
        send(3, b, len, n, image[part[n]]);
      pause(4);
      send(9, b, len);
      pause(2);
    } catch (...) {
      try {
        send(0, b);
      } catch (...) {
      }
      throw;
    }
    send(0, b);
  }
};
struct MockMemory : Memory {
  Image &image;
  explicit MockMemory(Image &i) : image(i) {}
  Image snapshot() override { return image; }
  void write(const QVector<int> &p, const Image &i) override {
    for (int a : p)
      image[a] = i[a];
  }
  void finalize() override {}
  void pause(int) override {}
};
QJsonObject Device::operation(const QString &action,
                              const QJsonObject &request) {
  require(QStringList({"read", "apply", "restore", "rgb", "experimental"})
              .contains(action),
          "Unsupported device operation");
  require(geteuid() != 0, "Run the app as your normal desktop user");
  if (action == "read")
    require(request.isEmpty(), "Read accepts no settings");
  if (action == "apply" || action == "restore") {
    require(request.keys() ==
                (action == "apply"
                     ? QStringList({"changes", "expected_revision"})
                     : QStringList({"expected_revision"})),
            "Invalid settings request");
    require(QRegularExpression("^[a-f0-9]{64}$")
                .match(request["expected_revision"].toString())
                .hasMatch(),
            "Read mouse before applying");
    if (action == "apply") {
      require(request["changes"].isObject(), "Invalid changes");
      Image dummy = fixture();
      for (int a : buttonAddresses)
        dummy[a] = 1;
      merge(dummy, request["changes"].toObject());
    }
  }
  QString folder = configDir();
  privateDir(folder);
  Fd lock;
  lock.n = open(QFile::encodeName(folder + "/device.lock").constData(),
                O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
  struct stat st {};
  require(lock.n >= 0 && fstat(lock.n, &st) == 0 && S_ISREG(st.st_mode) &&
              st.st_uid == geteuid() && !(st.st_mode & 0077),
          "Unsafe device lock");
  require(flock(lock.n, LOCK_EX | LOCK_NB) == 0,
          "Another mouse operation is running");
  Fd hid;
  std::unique_ptr<Memory> mem;
  if (mock)
    mem = std::make_unique<MockMemory>(mockImage);
  else {
    auto all = discover();
    QVector<DeviceInfo> matches;
    for (auto d : all)
      if (d.supported)
        matches << d;
    require(matches.size() == 1,
            "Connect exactly one supported GM320 or X16v2 and refresh");
    hid.n = open(QFile::encodeName(matches[0].node).constData(),
                 O_RDWR | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    require(hid.n >= 0,
            "Cannot open mouse. Install the scoped udev rule and reconnect");
    require(fstat(hid.n, &st) == 0 && S_ISCHR(st.st_mode) &&
                flock(hid.n, LOCK_EX | LOCK_NB) == 0,
            "Invalid or busy HID endpoint");
    hidraw_devinfo info{};
    require(ioctl(hid.n, HIDIOCGRAWINFO, &info) == 0 && info.bustype == 3 &&
                info.vendor == 0x30fa && info.product == 0x1440,
            "Mouse identity changed");
    int length = 0;
    require(ioctl(hid.n, HIDIOCGRDESCSIZE, &length) == 0 && length >= 1 &&
                length <= HID_MAX_DESCRIPTOR_SIZE,
            "Invalid HID descriptor size");
    hidraw_report_descriptor desc{};
    desc.size = length;
    require(ioctl(hid.n, HIDIOCGRDESC, &desc) == 0 &&
                int(desc.size) == length &&
                QString::fromLatin1(
                    QCryptographicHash::hash(
                        QByteArray(reinterpret_cast<const char *>(desc.value),
                                   length),
                        QCryptographicHash::Sha256)
                        .toHex()) == descriptor,
            "Mouse descriptor changed");
    mem = std::make_unique<HidMemory>(hid.n);
  }
  if (action == "rgb" || action == "experimental") {
    QVector<QByteArray> reports;
    if (action == "rgb")
      reports = rgbReports(request);
    else {
      require(request.keys() == QStringList({"brightness", "speed"}),
              "Invalid experimental settings");
      reports << experimentalReport(integer(request["brightness"], 0, 7),
                                    integer(request["speed"], 0, 7));
    }
    if (!mock) {
      auto *m = dynamic_cast<HidMemory *>(mem.get());
      for (auto r : reports)
        m->feature(r);
    }
    return {{"status", "sent-unverified"},
            {"reports_sent", reports.size()},
            {"physical_effect_verified", false}};
  }
  Image before = mem->stable();
  decode(before);
  QString backup = folder + "/device-backup.json";
  auto canRestore = [&](const Image &i) {
    try {
      auto pair = loadBackup(backup, i);
      return restore(i, pair.first, pair.second) != i;
    } catch (...) {
      return false;
    }
  };
  if (action == "read")
    return {{"status", "read"},
            {"settings", decode(before)},
            {"revision", revision(before)},
            {"can_restore", canRestore(before)}};
  require(request["expected_revision"] == revision(before),
          "Mouse settings changed since the last read. Reload before applying");
  Image after;
  if (action == "apply")
    after = merge(before, request["changes"].toObject());
  else {
    auto pair = loadBackup(backup, before);
    after = restore(before, pair.first, pair.second);
  }
  bool activate =
      action == "apply" &&
      request["changes"].toObject()["lighting"].toObject().contains("effect");
  bool changed = transact(*mem, before, after,
                          action == "apply" ? std::function<void()>([&] {
                            saveBackup(backup, before, after);
                          })
                                            : std::function<void()>(),
                          activate);
  return {{"status", changed ? "written-readback" : "unchanged"},
          {"settings", decode(after)},
          {"revision", revision(after)},
          {"can_restore", canRestore(after)},
          {"restored", action == "restore"}};
}
} // namespace mouse
