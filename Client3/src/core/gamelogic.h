#pragma once

#include "playerprofile.h"

// 纯函数，无副作用。每次 tick 调用后将结果写回 profile。
class GameLogic
{
public:
    // 每秒调用：按悟性增加修为
    static void tickXiuwei(PlayerProfile &p);

    // 每秒调用：按境界+悟性赚取灵石
    static void tickLingshi(PlayerProfile &p);

    // 每10秒调用：检查修为是否满足境界突破条件
    // 返回 true 表示发生了突破
    static bool tryLevelUp(PlayerProfile &p);

    // 每境界所需修为阈值
    static double xiuweiThreshold(int jingjie);
};
