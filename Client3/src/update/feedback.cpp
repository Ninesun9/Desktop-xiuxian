#include "feedback.h"

#include <QEventLoop>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

FeedBack::FeedBack(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("意见反馈");
    setFixedSize(380, 260);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    m_edit = new QTextEdit(this);
    m_edit->setPlaceholderText("请输入您的意见或建议…");
    root->addWidget(m_edit, 1);

    auto *btnRow  = new QHBoxLayout;
    auto *sendBtn = new QPushButton("提交", this);
    auto *cancelBtn = new QPushButton("取消", this);
    btnRow->addStretch(1);
    btnRow->addWidget(sendBtn);
    btnRow->addWidget(cancelBtn);
    root->addLayout(btnRow);

    connect(sendBtn,   &QPushButton::clicked, this, &FeedBack::onSend);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::hide);
}

void FeedBack::setBaseUrl(const QString &url) { m_baseUrl = url; }
void FeedBack::setAccessToken(const QString &token) { m_token = token; }

void FeedBack::onSend()
{
    const QString text = m_edit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    QNetworkRequest req(QUrl(m_baseUrl + "/feedback"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain; charset=utf-8");
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    auto *reply = m_nam.post(req, text.toUtf8());

    // 等待响应，最多 3 秒
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout,        &loop, &QEventLoop::quit);
    timer.start(3000);
    loop.exec();

    reply->deleteLater();

    if (timer.isActive()) {
        timer.stop();
        QMessageBox::information(this, "反馈", "提交成功，感谢！");
        m_edit->clear();
        hide();
    } else {
        QMessageBox::warning(this, "反馈", "提交超时，请稍后再试。");
    }
}
