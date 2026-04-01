#pragma once

#include <QDialog>
#include <QNetworkAccessManager>
#include <QTextEdit>

class FeedBack : public QDialog
{
    Q_OBJECT
public:
    explicit FeedBack(QWidget *parent = nullptr);

    void setBaseUrl(const QString &url);
    void setAccessToken(const QString &token);

private slots:
    void onSend();

private:
    QTextEdit            *m_edit  = nullptr;
    QNetworkAccessManager m_nam;
    QString               m_baseUrl = "http://127.0.0.1:3000";
    QString               m_token;
};
