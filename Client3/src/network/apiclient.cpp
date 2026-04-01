#include "apiclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <functional>

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
{}

void ApiClient::setBaseUrl(const QString &url)
{
    m_baseUrl = url;
}

void ApiClient::setAccessToken(const QString &token)
{
    m_token = token;
}

// ── helpers ────────────────────────────────────────────────────────────────

QNetworkRequest ApiClient::makeRequest(const QString &path) const
{
    QNetworkRequest req(QUrl(m_baseUrl + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    return req;
}

void ApiClient::handleReply(QNetworkReply *reply,
                            const QString &context,
                            std::function<void(const QByteArray &)> onSuccess)
{
    connect(reply, &QNetworkReply::finished, this, [this, reply, context, onSuccess]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit requestError(context, reply->errorString());
            return;
        }
        onSuccess(reply->readAll());
    });
}

// ── API calls ──────────────────────────────────────────────────────────────

void ApiClient::deviceLogin(const QString &deviceId, const QString &deviceName)
{
    QJsonObject body;
    body["deviceId"]   = deviceId;
    body["deviceName"] = deviceName;
    body["platform"]   = "windows";

    auto *reply = m_nam.post(makeRequest("/api/v1/auth/device-login"),
                             QJsonDocument(body).toJson(QJsonDocument::Compact));

    handleReply(reply, "device-login", [this](const QByteArray &data) {
        const auto doc = QJsonDocument::fromJson(data);
        const auto obj = doc.object();
        const QString token = obj["accessToken"].toString();
        const QString uid   = obj["user"].toObject()["id"].toString();
        if (token.isEmpty()) {
            emit loginFailed("响应中无 accessToken");
            return;
        }
        m_token = token;
        emit loginSucceeded(token, uid);
    });
}

void ApiClient::fetchPetState()
{
    auto *reply = m_nam.get(makeRequest("/api/v1/pet/state"));
    handleReply(reply, "fetch-pet-state", [this](const QByteArray &data) {
        emit petStateFetched(QJsonDocument::fromJson(data).object());
    });
}

void ApiClient::patchPetState(const QJsonObject &patch)
{
    QNetworkRequest req = makeRequest("/api/v1/pet/state");
    auto *reply = m_nam.sendCustomRequest(req, "PATCH",
                      QJsonDocument(patch).toJson(QJsonDocument::Compact));
    handleReply(reply, "patch-pet-state", [this](const QByteArray &data) {
        emit petStatePatched(QJsonDocument::fromJson(data).object());
    });
}

void ApiClient::fetchRankings()
{
    auto *reply = m_nam.get(makeRequest("/api/v1/pet/rankings"));
    handleReply(reply, "fetch-rankings", [this](const QByteArray &data) {
        const auto obj = QJsonDocument::fromJson(data).object();
        emit rankingsFetched(obj["items"].toArray());
    });
}
