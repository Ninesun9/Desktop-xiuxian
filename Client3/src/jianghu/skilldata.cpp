#include "skilldata.h"
#include <QStringList>

QString SkillData::getSkillInfo(int id)
{
    // 简单技能描述表
    static const QStringList kSkills = {
        "普通攻击", "穿刺", "横扫", "气旋斩", "天罡剑气"
    };
    if (id >= 0 && id < kSkills.size())
        return kSkills.at(id);
    return "未知招式";
}
