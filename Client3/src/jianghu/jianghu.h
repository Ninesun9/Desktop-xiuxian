#pragma once

#include <QDialog>
#include "jianghuglobal.h"
#include "jhleitai.h"
#include "userinfo.h"

class JiangHu : public QDialog
{
    Q_OBJECT
public:
    explicit JiangHu(QWidget *parent = nullptr);
    void setInfo(const QString &userName, const QString &userId);

private:
    UserInfo  *m_userInfo = nullptr;
    JHLeiTai  *m_leitai   = nullptr;
};
