#pragma once

#include "playerprofile.h"

class PlayerProfileStore
{
public:
    static QString configFilePath();

    // 返回 false 表示文件不存在或解析失败
    static bool load(PlayerProfile *profile);
    static void save(const PlayerProfile &profile);

    static PlayerProfile createNew(const QString &userName);
    static QString       generateUserId(const QString &userName);
    static bool          hasValidUserId(const QString &userId);
    static int           seedFromUserId(const QString &userId, int fallback = 2222);
};
