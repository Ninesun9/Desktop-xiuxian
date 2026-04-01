#pragma once

#include <QDateTime>
#include <QPushButton>
#include <QTimer>

struct BaseKillInfo
{
    QString name      = "神秘招式";
    QString kCD       = "3000"; // ms
    QString kShanghai = "500";
};

class SkillBar : public QPushButton
{
    Q_OBJECT
public:
    explicit SkillBar(QWidget *parent = nullptr);

    void initThis(const BaseKillInfo &info);

    BaseKillInfo killInfo;

public slots:
    void setRefValue(float value); // 冷却进度 0~1
    void refCD();
    void onClick();

signals:
    void damage(const QString &data); // "技能名|伤害值"

private:
    QTimer    m_cdTimer;
    QDateTime m_baseTime;
};
