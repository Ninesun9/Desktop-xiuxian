#include "updatechecker.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QNetworkRequest>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

static constexpr char kVersionUrl[] = "http://xiuxian.fuh.ink:8522/lastVersion";

UpdateChecker::UpdateChecker(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("版本更新");
    setFixedSize(440, 360);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    m_browser = new QTextBrowser(this);
    root->addWidget(m_browser, 1);

    auto *btnRow  = new QHBoxLayout;
    auto *openBtn = new QPushButton("下载更新", this);
    auto *closeBtn= new QPushButton("关闭", this);
    btnRow->addStretch(1);
    btnRow->addWidget(openBtn);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    connect(openBtn,  &QPushButton::clicked, this, &UpdateChecker::openLink);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::hide);
    connect(&m_nam,   &QNetworkAccessManager::finished, this, &UpdateChecker::onReply);

    // 立即检查一次，然后每小时再检查
    checkNow();
    connect(&m_timer, &QTimer::timeout, this, &UpdateChecker::checkNow);
    m_timer.start(3600 * 1000);
}

void UpdateChecker::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void UpdateChecker::checkNow()
{
    m_nam.get(QNetworkRequest(QUrl(kVersionUrl)));
}

void UpdateChecker::onReply(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
        return;

    // 格式：版本号|下载链接|HTML内容
    const QStringList parts = QString::fromUtf8(reply->readAll()).split('|');
    if (parts.size() < 3) return;

    m_browser->setHtml(parts.at(2));
    m_downloadLink = parts.at(1);

    if (qApp->applicationVersion() != parts.at(0))
        show(); // 有新版本才弹出
}

void UpdateChecker::openLink()
{
    if (!m_downloadLink.isEmpty())
        QDesktopServices::openUrl(QUrl(m_downloadLink));
}
