#include "guastacked.h"

#include <QLabel>

GuaStacked::GuaStacked(QWidget *parent)
    : QStackedWidget(parent)
{
    // page 0 = 阴（━ ━），page 1 = 阳（───）
    auto *yin  = new QLabel("━  ━", this);
    auto *yang = new QLabel("─────", this);
    yin->setAlignment(Qt::AlignCenter);
    yang->setAlignment(Qt::AlignCenter);
    yin->setStyleSheet("font:bold 20px; color:#8b6914;");
    yang->setStyleSheet("font:bold 20px; color:#8b6914;");
    addWidget(yin);
    addWidget(yang);

    setFixedHeight(36);
    connect(&m_timer, &QTimer::timeout, this, &GuaStacked::refUI);
    m_timer.start(100);
}

void GuaStacked::setGuaXiang(int gua)
{
    m_gua = gua;
}

void GuaStacked::refUI()
{
    if (m_gua > 1)
        return; // 未设置

    if (m_count >= 120) {
        m_timer.stop();
        setCurrentIndex(m_gua);
        emit refUiFinish();
    } else {
        setCurrentIndex(m_count % 2);
        ++m_count;
        m_timer.start(m_count); // 逐渐减速
    }
}
