#include "augurwindow.h"

#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTimer>

#include "../core/playerprofilestore.h"

AugurWindow::AugurWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("占卜");
    setFixedSize(360, 480);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    // 六爻动画
    for (int i = 0; i < 6; ++i) {
        auto *gua = new GuaStacked(this);
        root->addWidget(gua);
        m_guaList.append(gua);
    }
    connect(m_guaList.last(), &GuaStacked::refUiFinish, this, &AugurWindow::showGuaWen);

    m_browser = new QTextBrowser(this);
    root->addWidget(m_browser, 1);

    auto *startBtn = new QPushButton("占卜", this);
    root->addWidget(startBtn);
    connect(startBtn, &QPushButton::clicked, this, &AugurWindow::onStart);
}

void AugurWindow::onStart()
{
    m_guaXiang.clear();

    // 种子：玩家 userId 特征值 + 当日日期
    PlayerProfile profile;
    int seed = 2222;
    if (PlayerProfileStore::load(&profile))
        seed = PlayerProfileStore::seedFromUserId(profile.userId, seed);

    // Qt6: 用 QRandomGenerator 替代 qsrand/qrand
    QRandomGenerator rng(static_cast<quint32>(
        QDateTime::currentDateTime().toString("yyyyMMdd").toInt() + seed));

    for (int i = 0; i < 6; ++i) {
        int gua = static_cast<int>(rng.generate() % 2);
        m_guaXiang.append(gua);
        m_guaList.at(i)->setGuaXiang(gua);

        // 每爻启动后等 2 秒再启下一爻（与原版一致）
        QEventLoop loop;
        QTimer t;
        connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
        t.start(2000);
        loop.exec();
    }
}

void AugurWindow::showGuaWen()
{
    QString fileName = "i_";
    for (int i = 0; i < 6; ++i)
        fileName.append(QString::number(m_guaXiang.at(i)));

    QFile f(":/gua_ci/" + fileName);
    if (!f.open(QIODevice::ReadOnly)) {
        m_browser->setText("（暂无卦辞资源）");
        return;
    }

    QString html;
    {
        QString line = f.readLine();
        line.remove("原文");
        html.append("<h2>" + line.trimmed() + "</h2>");
    }
    {
        QString line = f.readLine();
        html.append("<h3>" + line.trimmed() + "</h3>");
    }
    {
        QString line = f.readLine();
        if (line.startsWith("象"))
            html.append("<p>" + line.trimmed() + "</p>");
    }

    bool inUseful = false;
    while (!f.atEnd()) {
        QString line = f.readLine().trimmed();
        if (line.startsWith("传")) {
            inUseful = true;
            line.remove("传统");
            html.append("<h3>" + line + "</h3>");
            continue;
        }
        if (line.startsWith("台"))
            inUseful = false;
        if (inUseful)
            html.append(line + "<br>");
    }
    m_browser->setHtml(html);
}
