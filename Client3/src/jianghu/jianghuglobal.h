#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QTimer>

// 江湖面板属性
struct UserData
{
    // 基础三维
    double uTizhi  = 10; // 体质
    double uGengu  = 10; // 根骨
    double uShenfa = 10; // 身法

    // 面板派生属性
    double uShanghai = 0; // 伤害
    double uBaoji    = 0; // 暴击
    double uQixue    = 0; // 气血
    double uNeili    = 0; // 内力
    double uHuajin   = 0; // 化劲
    double uYujin    = 0; // 御劲

    // 积分系统
    double uJianggong   = 0; // 贡献值
    double uWeiwang     = 0; // 威望
    double uJifen       = 0; // 擂台积分
    double uXiaofeiquan = 0; // 消费券
};

class JiangHuGlobal : public QObject
{
    Q_OBJECT
public:
    explicit JiangHuGlobal(QObject *parent = nullptr);

    QString  uuid;
    QString  uName;
    UserData userData;

    // 基础属性（影响面板计算）
    double baseShanghai = 100;
    double baseBaoji    = 0.1;
    double baseQixue    = 5000;
    double baseNeili    = 2000;
    double baseHuajin   = 30;
    double baseYujin    = 0.05;
    double baseTizhi    = 10;
    double baseGengu    = 10;
    double baseShenfa   = 10;

public slots:
    void refUserData(); // 重新计算面板属性
    void loadData();
    void saveData();

private:
    static QString configPath();
    QTimer m_saveTimer;
};

// 全局单例指针（与 Client2 接口一致）
extern JiangHuGlobal *jg;
