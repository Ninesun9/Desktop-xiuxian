#include "jhleitai.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

JHLeiTai::JHLeiTai(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("擂台");
    setFixedSize(480, 420);
    buildUi();
    connect(&m_timer, &QTimer::timeout, this, &JHLeiTai::onTimerOut);
}

void JHLeiTai::buildUi()
{
    m_stack = new QStackedWidget(this);
    auto *root = new QVBoxLayout(this);
    root->addWidget(m_stack);

    // ── 页0：匹配等待 ───────────────────────────────
    auto *waitPage = new QWidget;
    auto *waitLayout = new QVBoxLayout(waitPage);

    m_labMyName1  = new QLabel(jg ? jg->uName : "我", waitPage);
    m_labYourName = new QLabel("等待对手…", waitPage);
    m_lcdLabel    = new QLabel("00:00", waitPage);
    m_lcdLabel->setStyleSheet("font:bold 32px; color:#333;");
    m_lcdLabel->setAlignment(Qt::AlignCenter);

    auto *startBtn = new QPushButton("开始匹配", waitPage);
    connect(startBtn, &QPushButton::clicked, this, &JHLeiTai::onStart);

    waitLayout->addWidget(m_labMyName1,  0, Qt::AlignCenter);
    waitLayout->addWidget(m_labYourName, 0, Qt::AlignCenter);
    waitLayout->addWidget(m_lcdLabel);
    waitLayout->addWidget(startBtn);
    m_stack->addWidget(waitPage);

    // ── 页1：战斗界面 ───────────────────────────────
    auto *fightPage = new QWidget;
    auto *fightLayout = new QVBoxLayout(fightPage);

    auto *namesRow = new QHBoxLayout;
    m_labMyName2   = new QLabel(jg ? jg->uName : "我", fightPage);
    m_labYourName2 = new QLabel("神秘高手", fightPage);
    namesRow->addWidget(m_labMyName2, 1);
    namesRow->addWidget(m_labYourName2, 1, Qt::AlignRight);
    fightLayout->addLayout(namesRow);

    m_myHpBar   = new QProgressBar(fightPage);
    m_yourHpBar = new QProgressBar(fightPage);
    m_myHpBar->setRange(0, 10000);
    m_yourHpBar->setRange(0, 10000);
    m_myHpBar->setValue(10000);
    m_yourHpBar->setValue(10000);
    fightLayout->addWidget(m_yourHpBar);
    fightLayout->addWidget(m_myHpBar);

    m_skillLayout = new QWidget(fightPage);
    auto *skillRow = new QHBoxLayout(m_skillLayout);
    skillRow->setContentsMargins(0, 0, 0, 0);
    fightLayout->addWidget(m_skillLayout);
    fightLayout->addStretch(1);

    m_stack->addWidget(fightPage);
    m_stack->setCurrentIndex(0);
}

void JHLeiTai::onStart()
{
    m_waitTime = QTime(0, 0, 0);
    if (jg) m_labMyName1->setText(jg->uName);
    m_timer.start(1000);
}

void JHLeiTai::onTimerOut()
{
    m_waitTime = m_waitTime.addSecs(1);
    m_lcdLabel->setText(m_waitTime.toString("mm:ss"));

    if (m_waitTime.second() == 6) {
        m_timer.stop();
        m_labYourName->setText("神秘高手");

        if (QMessageBox::question(this, "匹配成功",
                "对手：神秘高手\n是否应战？",
                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            m_yourName = "神秘高手";
            fightInit();
        } else {
            initThis();
        }
    }
}

void JHLeiTai::initThis()
{
    m_waitTime = QTime(0, 0, 0);
    m_lcdLabel->setText("00:00");
    m_stack->setCurrentIndex(0);
}

void JHLeiTai::fightInit()
{
    m_stack->setCurrentIndex(1);

    if (jg) {
        jg->refUserData();
        m_myQixue = jg->userData.uQixue;
        m_myHpBar->setMaximum(static_cast<int>(m_myQixue));
        m_myHpBar->setValue(static_cast<int>(m_myQixue));
        if (jg) m_labMyName2->setText(jg->uName);
    }

    initSkillList();

    if (!m_ai) {
        m_ai = new BattleAI(this);
        connect(m_ai, &BattleAI::damage,    this, &JHLeiTai::beDamage);
        connect(m_ai, &BattleAI::nowQixue,  this, &JHLeiTai::receYourQixue);
        connect(m_ai, &BattleAI::baseInfo,  this, &JHLeiTai::receYourInfo);
        connect(this, &JHLeiTai::damage,    m_ai, &BattleAI::beDamage);
    }
    m_ai->initThis();
    m_ai->battleStart();
}

void JHLeiTai::initSkillList()
{
    // 清空旧技能按钮
    const QList<SkillBar *> old = m_skillBars;
    m_skillBars.clear();
    for (auto *bar : old) bar->deleteLater();

    const QStringList names = {"拳", "打", "脚", "踢"};
    auto *row = qobject_cast<QHBoxLayout *>(m_skillLayout->layout());

    for (int i = 0; i < names.size(); ++i) {
        auto *bar = new SkillBar(m_skillLayout);
        BaseKillInfo info;
        info.name      = names.at(i);
        info.kCD       = QString::number(1000 * i + 500);
        info.kShanghai = QString::number(100 * i + 100);
        bar->initThis(info);
        if (row) row->addWidget(bar);
        connect(bar, &SkillBar::damage, this, &JHLeiTai::receKillBarClick);
        m_skillBars.append(bar);
    }
}

void JHLeiTai::receKillBarClick(const QString &data)
{
    emit damage(data);
}

void JHLeiTai::beDamage(const QString &data)
{
    // "技能编号|伤害"
    const QStringList parts = data.split('|');
    if (parts.size() != 2) return;

    double dmg = parts.at(1).toDouble();
    // 御劲减伤
    if (jg) dmg *= (1.0 - jg->userData.uYujin);

    m_myQixue -= dmg;
    if (m_myQixue <= 0) { m_myQixue = 0; fightEnd(); }
    m_myHpBar->setValue(static_cast<int>(m_myQixue));
    emit nowQixue(QString::number(m_myQixue, 'f', 0));
}

void JHLeiTai::receYourQixue(const QString &qixue)
{
    const double v = qixue.toDouble();
    m_yourHpBar->setValue(static_cast<int>(qMax(0.0, v)));
    if (v <= 0) fightEnd();
}

void JHLeiTai::receYourInfo(const QString &info)
{
    // "名字|气血|化劲|御劲"
    const QStringList parts = info.split('|');
    if (parts.size() >= 2) {
        m_yourName      = parts.at(0);
        m_yourQixueFull = parts.at(1).toDouble();
        m_yourQixue     = m_yourQixueFull;
        m_yourHpBar->setMaximum(static_cast<int>(m_yourQixueFull));
        m_yourHpBar->setValue(static_cast<int>(m_yourQixueFull));
        m_labYourName2->setText(m_yourName);
    }
    startFight();
}

void JHLeiTai::startFight()
{
    for (auto *bar : m_skillBars)
        bar->setEnabled(true);
}

void JHLeiTai::fightEnd()
{
    if (m_ai) m_ai->deleteLater();
    m_ai = nullptr;

    const bool won = (m_myQixue > 0);
    QMessageBox::information(this, "战斗结束",
        won ? "🎉 胜利！" : "💀 败北……");

    if (jg && won) {
        jg->userData.uJifen += 10;
        jg->userData.uWeiwang += 5;
        jg->saveData();
    }
    initThis();
}
