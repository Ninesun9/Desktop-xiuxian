#include "jianghu.h"

#include <QPushButton>
#include <QVBoxLayout>

JiangHu::JiangHu(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("江湖");
    setFixedSize(280, 160);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *userInfoBtn = new QPushButton("角色属性", this);
    auto *leitaiBtn   = new QPushButton("进入擂台", this);

    layout->addWidget(userInfoBtn);
    layout->addWidget(leitaiBtn);

    connect(userInfoBtn, &QPushButton::clicked, this, [this] {
        if (m_userInfo) m_userInfo->deleteLater();
        m_userInfo = new UserInfo(this);
        m_userInfo->show();
    });

    connect(leitaiBtn, &QPushButton::clicked, this, [this] {
        if (m_leitai) m_leitai->deleteLater();
        m_leitai = new JHLeiTai(this);
        m_leitai->show();
    });
}

void JiangHu::setInfo(const QString &userName, const QString &userId)
{
    if (!jg) return;
    jg->uName = userName;
    jg->uuid  = userId;
}
