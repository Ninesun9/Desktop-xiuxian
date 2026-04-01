#ifndef PLAYERPROFILESTORE_H
#define PLAYERPROFILESTORE_H

#include <QString>

struct PlayerProfile
{
    QString userId;
    QString userName;
    QString shengming;
    QString gongji;
    QString fangyu;
    QString wuxing;
    int jingjie = 0;
    QString xiuwei;
};

class PlayerProfileStore
{
public:
    static QString configFilePath();
    static bool load(PlayerProfile *profile);
    static void save(const PlayerProfile &profile);
    static PlayerProfile createNew(const QString &userName);
    static QString generateUserId(const QString &userName);
    static bool hasValidUserId(const QString &userId);
    static int seedFromUserId(const QString &userId, int fallbackSeed = 2222);
};

#endif // PLAYERPROFILESTORE_H
