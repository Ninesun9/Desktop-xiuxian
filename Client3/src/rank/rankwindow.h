#pragma once

#include <QDialog>
#include <QTextBrowser>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class RankWindow : public QDialog
{
    Q_OBJECT
public:
    explicit RankWindow(QWidget *parent = nullptr);

    // 注入 ApiClient 的 accessToken（供后端 /api/v1/pet/rankings 使用）
    void setAccessToken(const QString &token);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void fetchXiuwei();
    void fetchShengming();
    void fetchGongji();
    void fetchFangyu();
    void onReply(QNetworkReply *reply);

private:
    void fetchLegacy(const QString &path);  // 旧服务器 yinciyunji.com
    void fetchNewApi(const QString &path);  // 新 API 127.0.0.1:3000

    QTextBrowser         *m_browser = nullptr;
    QNetworkAccessManager m_nam;
    QString               m_token;
};
