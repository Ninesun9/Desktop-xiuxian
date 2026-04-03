#include "gamelogic.h"

#include <QTime>
#include <QtMath>

// 每境界突破所需修为 = 100^jingjie
double GameLogic::xiuweiThreshold(int jingjie)
{
    return qPow(100.0, jingjie);
}

static double nightMultiplier()
{
    const int hour = QTime::currentTime().hour();
    if (hour < 2)      return 2.0;
    if (hour < 4)      return 5.0;
    if (hour < 6)      return 2.0;
    return 1.0;
}

void GameLogic::tickXiuwei(PlayerProfile &p)
{
    p.xiuwei += p.wuxing * nightMultiplier();
}

void GameLogic::tickLingshi(PlayerProfile &p)
{
    // 灵石收益：境界越高、悟性越高赚得越多
    // 基础 = (jingjie+1) * wuxing * 0.5 / 秒
    // 深夜同享加成
    p.lingshi += (p.jingjie + 1) * p.wuxing * 0.5 * nightMultiplier();
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
