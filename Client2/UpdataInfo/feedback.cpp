#include "feedback.h"
#include "ui_feedback.h"
#include <QMessageBox>

#include "../secure_transport.h"
FeedBack::FeedBack(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FeedBack)
{
    ui->setupUi(this);
    m_httpManager = new QNetworkAccessManager;
}

FeedBack::~FeedBack()
{
    delete ui;
}

void FeedBack::on_send_clicked()
{
    QString errorMessage;
    if (!SecureTransport::validateConfig(&errorMessage)) {
        QMessageBox::warning(this, "配置错误", errorMessage);
        return;
    }

    SecureTransport::configureRequest(m_httpRequest, "/feedback");
    m_httpReply = m_httpManager->post(m_httpRequest, ui->textEdit->toPlainText().toUtf8());
    // post请求
    QEventLoop _loop;
    QTimer _timer;
    connect(m_httpReply, SIGNAL(finished()), &_loop, SLOT(quit()));
    connect(&_timer, SIGNAL(timeout()), &_loop, SLOT(quit()));
    // eventloop用于阻塞等待消息，并结合timer进行超时处理
    _timer.start(1000);
    _loop.exec();
    // 这里的eventloop只是阻塞此段，并不会阻塞其他块的代码，比如我不加定时器，让这个eventloop一直exec，还是可以出发相关的槽函数
    if(_timer.isActive())
    {
        _timer.stop();
        // 处理函数
        QMessageBox::information(this,"成功","提交成功");
        ui->textEdit->setText("");
    }
    else
    {
        qDebug() <<  "time Out";
        //超时结果
    }
    _loop.deleteLater();
    _timer.deleteLater();
    // 释放资源
    m_httpReply->abort();
    // 断开连接
}

