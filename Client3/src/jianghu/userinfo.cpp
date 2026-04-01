#include "userinfo.h"

#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

UserInfo::UserInfo(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("角色属性");
    setFixedSize(300, 380);

    auto *form = new QFormLayout(this);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(10);

    if (!jg) return;

    jg->refUserData();
    const UserData &d = jg->userData;

    auto add = [&](const QString &label, double val) {
        form->addRow(label, new QLabel(QString::number(val, 'f', 2), this));
    };

    form->addRow(new QLabel("<b>" + jg->uName + "</b>", this));
    add("体质",   d.uTizhi);
    add("根骨",   d.uGengu);
    add("身法",   d.uShenfa);
    form->addRow(new QLabel("─── 面板 ───", this));
    add("伤害",   d.uShanghai);
    add("暴击",   d.uBaoji);
    add("气血",   d.uQixue);
    add("内力",   d.uNeili);
    add("化劲",   d.uHuajin);
    add("御劲",   d.uYujin);
    form->addRow(new QLabel("─── 积分 ───", this));
    add("贡献值", d.uJianggong);
    add("威望",   d.uWeiwang);
    add("擂台积分", d.uJifen);
}
