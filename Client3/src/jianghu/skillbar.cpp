#include "skillbar.h"

SkillBar::SkillBar(QWidget *parent)
    : QPushButton(parent)
{
    connect(this,       &QPushButton::clicked, this, &SkillBar::onClick);
    connect(&m_cdTimer, &QTimer::timeout,      this, &SkillBar::refCD);
    m_cdTimer.start(50);
    m_baseTime = QDateTime::currentDateTime();
}

void SkillBar::initThis(const BaseKillInfo &info)
{
    killInfo   = info;
    setText(info.name);
    m_baseTime = QDateTime::currentDateTime();
}

void SkillBar::setRefValue(float value)
{
    value = qBound(0.001f, value, 0.999f);
    const float v2 = value + 0.0001f;
    setStyleSheet(
        "background-color: qlineargradient("
        "spread:pad, x1:0, y1:0, x2:1, y2:0,"
        "stop:" + QString::number(value, 'f', 4) + " rgba(0,0,0,149),"
        "stop:" + QString::number(v2,   'f', 4) + " rgba(0,0,0,0));"
    );
}

void SkillBar::onClick()
{
    m_baseTime = QDateTime::currentDateTime();
    emit damage(text() + "|" + killInfo.kShanghai);
}

void SkillBar::refCD()
{
    const qint64 ms  = m_baseTime.msecsTo(QDateTime::currentDateTime());
    const int    cd  = killInfo.kCD.toInt();
    if (ms >= cd) {
        setEnabled(true);
        setRefValue(0.0f);
    } else {
        setEnabled(false);
        setRefValue(1.0f - float(ms) / float(cd));
    }
}
