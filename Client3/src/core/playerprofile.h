#pragma once

#include <QString>
#include <QStringList>

// 境界名称列表（按 jingjie 索引）
inline const QStringList kJieDuanNames = {
    "炼体期", "练气期", "筑基期", "金丹期", "元婴期",
    "化神期", "炼虚期", "合体期", "大乘期", "渡劫期"
};

struct PlayerProfile
{
    QString userId;
    QString userName;
    double  shengming = 100.0; // 生命
    double  gongji    = 20.0;  // 攻击
    double  fangyu    = 20.0;  // 防御
    double  wuxing    = 1.0;   // 悟性
    int     jingjie   = 0;     // 境界（索引）
    double  xiuwei    = 0.0;   // 修为
    double  lingshi   = 0.0;   // 灵石（货币）

    QString jieDuanName() const
    {
        if (jingjie >= 0 && jingjie < kJieDuanNames.size())
            return kJieDuanNames.at(jingjie);
        return "未知";
    }
};
