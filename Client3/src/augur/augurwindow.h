#pragma once

#include <QDialog>
#include <QList>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "guastacked.h"

class AugurWindow : public QDialog
{
    Q_OBJECT

public:
    explicit AugurWindow(QWidget *parent = nullptr);

private slots:
    void onStart();
    void showGuaWen();

private:
    QList<GuaStacked *> m_guaList;
    QList<int>          m_guaXiang;
    QTextBrowser       *m_browser = nullptr;

    // LCG 参数（与 Client2 一致）
    int m_m = 1 << 20;
    int m_a = 9;
    int m_b = 7;
    int m_x = 5;
};
