#include "playerprofilestore.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QVariant>

namespace {

QString decodeStoredValue(const QSettings &settings, const QString &key)
{
    const QString encoded = settings.value(key).toString();
    const QByteArray hexBytes = QByteArray::fromHex(encoded.toLatin1());
    return QString::fromUtf8(QByteArray::fromBase64(hexBytes));
}

QString encodeStoredValue(const QVariant &value)
{
    return QString(value.toString().toLatin1().toBase64().toHex());
}

} // namespace

QString PlayerProfileStore::configFilePath()
{
    return QDir::homePath() + "/AppData/Roaming/xiuxian.ini";
}

bool PlayerProfileStore::load(PlayerProfile *profile)
{
    if (profile == nullptr) {
        return false;
    }

    QSettings settings(configFilePath(), QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    profile->userId = settings.value("xiuxian/userid").toString();
    profile->userName = settings.value("xiuxian/username").toString();
    profile->shengming = QString::number(QVariant(decodeStoredValue(settings, "xiuxian/shengming")).toReal(), 'g', 15);
    profile->gongji = QString::number(QVariant(decodeStoredValue(settings, "xiuxian/gongji")).toReal(), 'g', 15);
    profile->fangyu = QString::number(QVariant(decodeStoredValue(settings, "xiuxian/fangyu")).toReal(), 'g', 15);
    profile->wuxing = QString::number(QVariant(decodeStoredValue(settings, "xiuxian/wuxing")).toReal(), 'g', 15);
    profile->jingjie = QVariant(decodeStoredValue(settings, "xiuxian/jingjie")).toInt();
    profile->xiuwei = QString::number(QVariant(decodeStoredValue(settings, "xiuxian/xiuwei")).toReal(), 'g', 15);
    return true;
}

void PlayerProfileStore::save(const PlayerProfile &profile)
{
    QSettings settings(configFilePath(), QSettings::IniFormat);
    settings.setValue("xiuxian/userid", profile.userId);
    settings.setValue("xiuxian/username", profile.userName);
    settings.setValue("xiuxian/shengming", encodeStoredValue(profile.shengming.toDouble()));
    settings.setValue("xiuxian/gongji", encodeStoredValue(profile.gongji.toDouble()));
    settings.setValue("xiuxian/fangyu", encodeStoredValue(profile.fangyu.toDouble()));
    settings.setValue("xiuxian/wuxing", encodeStoredValue(profile.wuxing.toDouble()));
    settings.setValue("xiuxian/jingjie", encodeStoredValue(profile.jingjie));
    settings.setValue("xiuxian/xiuwei", encodeStoredValue(profile.xiuwei.toDouble()));
}

PlayerProfile PlayerProfileStore::createNew(const QString &userName)
{
    PlayerProfile profile;
    profile.userName = userName;
    profile.userId = generateUserId(userName);
    profile.shengming = QString::number((qrand() % 10) + 95);
    profile.gongji = QString::number((qrand() % 10) + 15.0);
    profile.fangyu = QString::number((qrand() % 10) + 15.0);
    profile.wuxing = QString::number(((qrand() % 2000) / 2000.0) + 0.5);
    profile.jingjie = 0;
    profile.xiuwei = "0";
    return profile;
}

QString PlayerProfileStore::generateUserId(const QString &userName)
{
    return QCryptographicHash::hash(
                (QDateTime::currentDateTime().toString() + userName + QString::number(qrand())).toLatin1(),
                QCryptographicHash::Md5)
            .toHex();
}

bool PlayerProfileStore::hasValidUserId(const QString &userId)
{
    return userId.count() == 32;
}

int PlayerProfileStore::seedFromUserId(const QString &userId, int fallbackSeed)
{
    int seed = fallbackSeed;
    const QByteArray bytes = userId.toLatin1();
    for (int index = 0; index < bytes.count(); ++index) {
        seed += bytes.at(index);
    }
    return seed;
}
