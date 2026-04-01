#include "battleai.h"

#include <QRandomGenerator>

BattleAI::BattleAI(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &BattleAI::onTimer);
}

void BattleAI::initThis()
{
    m_qixue = 10000;
    emit baseInfo("神秘高手|10000|0.5|0.03");
}

void BattleAI::battleStart()
{
    const int delay = QRandomGenerator::global()->bounded(1000, 4000);
    m_timer.start(delay);
}

void BattleAI::onTimer()
{
    m_timer.stop();
    const int skill  = QRandomGenerator::global()->bounded(101);
    const int dmg    = QRandomGenerator::global()->bounded(700);
    emit damage(QString::number(skill) + "|" + QString::number(dmg));
    m_timer.start(QRandomGenerator::global()->bounded(1000, 4000));
}

void BattleAI::beDamage(const QString &data)
{
    const QStringList parts = data.split('|');
    if (parts.size() == 2) {
        m_qixue -= parts.at(1).toDouble();
        emit nowQixue(QString::number(m_qixue, 'f', 0));
    }
}
