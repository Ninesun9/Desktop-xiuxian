#pragma once

#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTextBrowser>
#include <QTimer>

class UpdateChecker : public QDialog
{
    Q_OBJECT
public:
    explicit UpdateChecker(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void checkNow();
    void onReply(QNetworkReply *reply);
    void openLink();

private:
    QTextBrowser         *m_browser = nullptr;
    QNetworkAccessManager m_nam;
    QTimer                m_timer;
    QString               m_downloadLink;
};
