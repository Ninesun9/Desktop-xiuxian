#include "playerprofilestore.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QRandomGenerator>
#include <QSettings>

// ── 内部编解码（与 Client2 格式兼容） ──────────────────────────────────────

static QString decode(const QSettings &s, const QString &key)
{
    const QString encoded = s.value(key).toString();
    const QByteArray hex  = QByteArray::fromHex(encoded.toLatin1());
    return QString::fromUtf8(QByteArray::fromBase64(hex));
}

static QString encode(const QVariant &v)
{
    return QString(v.toString().toLatin1().toBase64().toHex());
}

// ── Public ─────────────────────────────────────────────────────────────────

QString PlayerProfileStore::configFilePath()
{
    return QDir::homePath() + "/AppData/Roaming/xiuxian.ini";
}

bool PlayerProfileStore::load(PlayerProfile *profile)
{
    if (!profile)
        return false;

    QSettings s(configFilePath(), QSettings::IniFormat);
    if (s.status() != QSettings::NoError)
        return false;
    if (!s.contains("xiuxian/userid"))
        return false;

    profile->userId   = s.value("xiuxian/userid").toString();
    profile->userName = s.value("xiuxian/username").toString();
    profile->shengming = decode(s, "xiuxian/shengming").toDouble();
    profile->gongji    = decode(s, "xiuxian/gongji").toDouble();
    profile->fangyu    = decode(s, "xiuxian/fangyu").toDouble();
    profile->wuxing    = decode(s, "xiuxian/wuxing").toDouble();
    profile->jingjie   = decode(s, "xiuxian/jingjie").toInt();
    profile->xiuwei    = decode(s, "xiuxian/xiuwei").toDouble();
    profile->lingshi   = decode(s, "xiuxian/lingshi").toDouble();
    return true;
}

void PlayerProfileStore::save(const PlayerProfile &profile)
{
    QSettings s(configFilePath(), QSettings::IniFormat);
    s.setValue("xiuxian/userid",   profile.userId);
    s.setValue("xiuxian/username", profile.userName);
    s.setValue("xiuxian/shengming", encode(profile.shengming));
    s.setValue("xiuxian/gongji",    encode(profile.gongji));
    s.setValue("xiuxian/fangyu",    encode(profile.fangyu));
    s.setValue("xiuxian/wuxing",    encode(profile.wuxing));
    s.setValue("xiuxian/jingjie",   encode(profile.jingjie));
    s.setValue("xiuxian/xiuwei",    encode(profile.xiuwei));
    s.setValue("xiuxian/lingshi",   encode(profile.lingshi));
}

PlayerProfile PlayerProfileStore::createNew(const QString &userName)
{
    auto *rng = QRandomGenerator::global();
    PlayerProfile p;
    p.userName  = userName;
    p.userId    = generateUserId(userName);
    p.shengming = 95.0 + rng->bounded(10);
    p.gongji    = 15.0 + rng->bounded(10);
    p.fangyu    = 15.0 + rng->bounded(10);
    p.wuxing    = 0.5  + rng->bounded(2000) / 2000.0;
    p.jingjie   = 0;
    p.xiuwei    = 0.0;
    return p;
}

QString PlayerProfileStore::generateUserId(const QString &userName)
{
    const QString seed = QDateTime::currentDateTime().toString()
                       + userName
                       + QString::number(QRandomGenerator::global()->generate());
    return QCryptographicHash::hash(seed.toLatin1(), QCryptographicHash::Md5).toHex();
}

bool PlayerProfileStore::hasValidUserId(const QString &userId)
{
    return userId.length() == 32;
}

int PlayerProfileStore::seedFromUserId(const QString &userId, int fallback)
{
    int seed = fallback;
    const QByteArray bytes = userId.toLatin1();
    for (int i = 0; i < bytes.size(); ++i)
        seed += static_cast<unsigned char>(bytes.at(i));
    return seed;
}
