#pragma once

#include <QObject>
#include <QTimer>

class BattleAI : public QObject
{
    Q_OBJECT
public:
    explicit BattleAI(QObject *parent = nullptr);

public slots:
    void battleStart();
    void initThis();
    void beDamage(const QString &data); // "技能|伤害"

signals:
    void damage(const QString &data);    // "技能|伤害"
    void nowQixue(const QString &qixue);
    void baseInfo(const QString &info);  // "名字|气血|化劲|御劲"

private slots:
    void onTimer();

private:
    QTimer m_timer;
    double m_qixue = 10000;
};
