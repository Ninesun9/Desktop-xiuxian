#pragma once

#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>

// 对接 apps/api（Fastify）的异步 HTTP 客户端
// 所有请求均非阻塞，通过 signal 回调结果。
class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);

    void setBaseUrl(const QString &url);   // e.g. "http://107.174.220.99:3000"
    void setAccessToken(const QString &token);

    // POST /api/v1/auth/device-login
    // { deviceId, deviceName, platform:"windows" }
    void deviceLogin(const QString &deviceId, const QString &deviceName);

    // GET /api/v1/pet/state
    void fetchPetState();

    // PATCH /api/v1/pet/state  { mood?, statusText? }
    void patchPetState(const QJsonObject &patch);

    // GET /api/v1/pet/rankings
    void fetchRankings();

signals:
    void loginSucceeded(const QString &accessToken, const QString &userId);
    void loginFailed(const QString &error);

    void petStateFetched(const QJsonObject &state);
    void petStatePatched(const QJsonObject &state);
    void rankingsFetched(const QJsonArray &items);

    void requestError(const QString &context, const QString &error);

private:
    QNetworkRequest makeRequest(const QString &path) const;
    void            handleReply(QNetworkReply *reply,
                                const QString &context,
                                std::function<void(const QByteArray &)> onSuccess);

    QNetworkAccessManager m_nam;
    QString               m_baseUrl  = "http://107.174.220.99:3000";
    QString               m_token;
};
