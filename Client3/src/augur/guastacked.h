#pragma once

#include <QStackedWidget>
#include <QTimer>

// 单爻动画：在 0/1 间随机闪烁 120 次后固定到目标值
class GuaStacked : public QStackedWidget
{
    Q_OBJECT

public:
    explicit GuaStacked(QWidget *parent = nullptr);

    // gua = 0（阴）或 1（阳）
    void setGuaXiang(int gua);

signals:
    void refUiFinish();

private slots:
    void refUI();

private:
    QTimer m_timer;
    int    m_gua   = 99;
    int    m_count = 10;
};
