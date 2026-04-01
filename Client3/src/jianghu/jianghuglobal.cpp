#include "jianghuglobal.h"

#include <QDir>

JiangHuGlobal *jg = nullptr;

JiangHuGlobal::JiangHuGlobal(QObject *parent)
    : QObject(parent)
{
    loadData();
    connect(&m_saveTimer, &QTimer::timeout, this, &JiangHuGlobal::saveData);
    m_saveTimer.start(10000);
}

QString JiangHuGlobal::configPath()
{
    return QDir::homePath() + "/AppData/Roaming/xiuxian_jianghu.ini";
}

void JiangHuGlobal::loadData()
{
    QSettings s(configPath(), QSettings::IniFormat);
    userData.uTizhi      = s.value("jh/tizhi",  baseTizhi).toDouble();
    userData.uGengu      = s.value("jh/gengu",  baseGengu).toDouble();
    userData.uShenfa     = s.value("jh/shenfa", baseShenfa).toDouble();
    userData.uJianggong  = s.value("jh/jianggong", 0).toDouble();
    userData.uWeiwang    = s.value("jh/weiwang",   0).toDouble();
    userData.uJifen      = s.value("jh/jifen",     0).toDouble();
    userData.uXiaofeiquan= s.value("jh/xiaofeiquan", 0).toDouble();
    refUserData();
}

void JiangHuGlobal::saveData()
{
    QSettings s(configPath(), QSettings::IniFormat);
    s.setValue("jh/tizhi",       userData.uTizhi);
    s.setValue("jh/gengu",       userData.uGengu);
    s.setValue("jh/shenfa",      userData.uShenfa);
    s.setValue("jh/jianggong",   userData.uJianggong);
    s.setValue("jh/weiwang",     userData.uWeiwang);
    s.setValue("jh/jifen",       userData.uJifen);
    s.setValue("jh/xiaofeiquan", userData.uXiaofeiquan);
}

void JiangHuGlobal::refUserData()
{
    // 面板计算（与 Client2 公式一致）
    userData.uShanghai = (baseShanghai + userData.uTizhi * 3 + userData.uGengu * 3);
    userData.uBaoji    = baseBaoji + userData.uGengu * 0.005;
    userData.uQixue    = baseQixue + userData.uTizhi * 100;
    userData.uNeili    = baseNeili + userData.uGengu * 5;
    userData.uHuajin   = baseHuajin + userData.uShenfa * 5;
    userData.uYujin    = baseYujin + userData.uShenfa * 0.003;
}
