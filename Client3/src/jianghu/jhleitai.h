#pragma once

#include <QDialog>
#include <QLabel>
#include <QList>
#include <QProgressBar>
#include <QStackedWidget>
#include <QTime>
#include <QTimer>

#include "battleai.h"
#include "jianghuglobal.h"
#include "skillbar.h"

class JHLeiTai : public QDialog
{
    Q_OBJECT
public:
    explicit JHLeiTai(QWidget *parent = nullptr);

public slots:
    void onStart();
    void onTimerOut();
    void initThis();
    void initSkillList();
    void startFight();
    void beDamage(const QString &data);
    void receYourQixue(const QString &qixue);
    void receYourInfo(const QString &info);
    void fightInit();
    void receKillBarClick(const QString &data);
    void fightEnd();

signals:
    void damage(const QString &data);
    void nowQixue(const QString &qixue);
    void baseInfo(const QString &info);

private:
    void buildUi();

    QStackedWidget *m_stack    = nullptr;

    // 匹配页
    QLabel  *m_labMyName1  = nullptr;
    QLabel  *m_labYourName = nullptr;
    QLabel  *m_lcdLabel    = nullptr;  // 替代 QLCDNumber

    // 战斗页
    QLabel      *m_labMyName2    = nullptr;
    QLabel      *m_labYourName2  = nullptr;
    QProgressBar *m_myHpBar      = nullptr;
    QProgressBar *m_yourHpBar    = nullptr;
    QWidget     *m_skillLayout   = nullptr; // 技能按钮容器

    QTimer            m_timer;
    QTime             m_waitTime;
    QList<SkillBar *> m_skillBars;
    BattleAI         *m_ai = nullptr;

    double m_myQixue     = 0;
    double m_yourQixue   = 0;
    double m_yourQixueFull = 0;
    double m_yourHuajin  = 0;
    double m_yourYujin   = 0;
    QString m_yourName;
};
