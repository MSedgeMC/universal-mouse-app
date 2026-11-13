#include "storage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace mouse {
QString configDir() {
  QString base = qEnvironmentVariable("XDG_CONFIG_HOME");
  if (!QDir::isAbsolutePath(base))
    base = QDir::homePath() + "/.config";
  return base + "/universal-mouse-app";
}
void privateDir(const QString &p) {
  bool existed = QFileInfo::exists(p);
  require(QDir().mkpath(p), "Cannot create settings directory");
  QFileInfo info(p);
  struct stat st {};
  require(lstat(QFile::encodeName(p).constData(), &st) == 0 &&
              S_ISDIR(st.st_mode) && st.st_uid == geteuid() &&
              info.canonicalFilePath() == info.absoluteFilePath(),
          "Settings directory must be owned by you without symlinks or public "
          "write access");
  if (!existed)
    require(chmod(QFile::encodeName(p).constData(), 0700) == 0,
            "Cannot set directory permissions");
  require(lstat(QFile::encodeName(p).constData(), &st) == 0 &&
              !(st.st_mode & 0022),
          "Settings directory must not be writable by other users");
}
void saveJson(const QString &p, const QJsonValue &v) {
  privateDir(QFileInfo(p).absolutePath());
  struct stat st {};
  if (lstat(QFile::encodeName(p).constData(), &st) == 0)
    require(S_ISREG(st.st_mode) && st.st_uid == geteuid() &&
                !(st.st_mode & 0077),
            "Existing settings file must be private and owned by you");
  QSaveFile f(p);
  f.setDirectWriteFallback(false);
  require(f.open(QIODevice::WriteOnly), "Cannot save settings");
  f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  auto d =
      v.isArray() ? QJsonDocument(v.toArray()) : QJsonDocument(v.toObject());
  QByteArray bytes = d.toJson();
  require(f.write(bytes) == bytes.size(), "Incomplete settings save");
  require(f.commit(), "Cannot commit settings save");
}
QJsonValue readJson(const QString &p, int limit) {
  int fd = open(QFile::encodeName(p).constData(),
                O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
  require(fd >= 0, "Cannot read saved settings");
  struct stat st {};
  bool ok = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
            st.st_uid == geteuid() && !(st.st_mode & 0077) &&
            st.st_size <= limit;
  if (!ok) {
    close(fd);
    throw Error("Settings must be a private regular file owned by you");
  }
  QFile f;
  if (!f.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
    close(fd);
    throw Error("Cannot open settings");
  }
  auto bytes = f.read(limit + 1);
  require(bytes.size() <= limit, "Settings file is too large");
  QJsonParseError e;
  auto d = QJsonDocument::fromJson(bytes, &e);
  require(e.error == QJsonParseError::NoError && !d.isNull(),
          "Invalid saved JSON");
  return d.isArray() ? QJsonValue(d.array()) : QJsonValue(d.object());
}
void saveBackup(const QString &p, const Image &b, const Image &a) {
  decode(b);
  decode(a);
  saveJson(p, QJsonObject{{"version", 4},
                          {"device", "30fa:1440"},
                          {"descriptor", descriptor},
                          {"before", toJson(b)},
                          {"after", toJson(a)}});
}
QPair<Image, Image> loadBackup(const QString &p, const Image &current) {
  auto j = readJson(p, 8192).toObject();
  require(j.keys() == QStringList({"after", "before", "descriptor", "device",
                                   "version"}) &&
              j["device"] == "30fa:1440" && j["descriptor"] == descriptor,
          "Invalid device backup");
  int version = integer(j["version"], 1, 4);
  require(j["before"].isObject() && j["after"].isObject(),
          "Invalid backup images");
  auto bj = j["before"].toObject(), aj = j["after"].toObject();
  require(bj.keys() == aj.keys(), "Different backup fields");
  QSet<int> stored;
  for (auto it = bj.begin(); it != bj.end(); ++it) {
    bool ok = false;
    int a = it.key().toInt(&ok);
    require(ok && QString::number(a) == it.key() && masks.contains(a),
            "Invalid backup address");
    stored.insert(a);
  }
  QSet<int> all;
  for (int a : masks.keys())
    all.insert(a);
  QSet<int> common = {0x13, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1e, 0x3f};
  bool allowed = version >= 3 ? stored == all
                 : version == 2
                     ? stored == all - QSet<int>{0x1a, 0x1b}
                     : stored == common + QSet<int>{0x48} ||
                           stored == common + QSet<int>{0x47, 0x48} ||
                           stored == common + QSet<int>(buttonAddresses.begin(),
                                                        buttonAddresses.end());
  require(allowed, "Unexpected backup fields");
  Image b = current, a = current;
  for (int ad : stored) {
    b[ad] = integer(bj[QString::number(ad)], 0, 255);
    a[ad] = integer(aj[QString::number(ad)], 0, 255);
  }
  decode(b);
  decode(a);
  for (int ad : masks.keys()) {
    int mask = masks[ad];
    if (version == 2 || version == 3) {
      if (buttonAddresses.contains(ad))
        mask &= ~32;
    }
    if (version == 1)
      mask = common.contains(ad) || buttonAddresses.contains(ad)
                 ? buttonAddresses.contains(ad) ? 31
                   : ad == 0x3f                 ? 0x70
                                                : masks[ad]
                 : 0;
    require(!((b[ad] ^ a[ad]) & ~mask), "Backup changes unrelated fields");
  }
  return {b, a};
}
} // namespace mouse
