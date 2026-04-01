#include "gamelogic.h"

#include <QTime>
#include <QtMath>

// 每境界突破所需修为 = 100^jingjie
double GameLogic::xiuweiThreshold(int jingjie)
{
    return qPow(100.0, jingjie);
}

void GameLogic::tickXiuwei(PlayerProfile &p)
{
    // 深夜加成（与 Client2 一致）
    const int hour = QTime::currentTime().hour();
    double multiplier = 1.0;
    if (hour < 2)       multiplier = 2.0;
    else if (hour < 4)  multiplier = 5.0;
    else if (hour < 6)  multiplier = 2.0;

    p.xiuwei += p.wuxing * multiplier;
}

bool GameLogic::tryLevelUp(PlayerProfile &p)
{
    bool leveled = false;
    for (int i = 0; i < kJieDuanNames.size(); ++i) {
        if (p.xiuwei >= xiuweiThreshold(i) && p.jingjie < i) {
            const double ratio = p.wuxing; // wuxing 已在上一次突破时更新，此处只做属性放大
            p.jingjie   = i;
            p.shengming *= ratio;
            p.gongji    *= ratio;
            p.fangyu    *= ratio;
            leveled = true;
        }
    }
    return leveled;
}
