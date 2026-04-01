#include "playerstatsreporter.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

#include "../secure_transport.h"

bool PlayerStatsReporter::sendUpdate(const PlayerStatsSnapshot &snapshot, QString *errorMessage)
{
    QString transportError;
    if (!SecureTransport::validateConfig(&transportError)) {
        if (errorMessage != nullptr) {
            *errorMessage = transportError;
        }
        return false;
    }

    QJsonObject payload;
    payload.insert("userid", snapshot.userId);
    payload.insert("username", snapshot.userName);
    payload.insert("shengming", snapshot.shengming);
    payload.insert("gongji", snapshot.gongji);
    payload.insert("fangyu", snapshot.fangyu);
    payload.insert("wuxing", snapshot.wuxing);
    payload.insert("jingjie", snapshot.jingjie);
    payload.insert("xiuwei", snapshot.xiuwei);

    QNetworkRequest request;
    SecureTransport::configureRequest(request, "/updata-new");
    const QByteArray encryptedPayload = SecureTransport::encryptPayload(QJsonDocument(payload).toJson(QJsonDocument::Compact));

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request, encryptedPayload);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));

    timer.start(1000);
    loop.exec();

    const bool timedOut = timer.isActive() == false && reply->isFinished() == false;
    if (timer.isActive()) {
        timer.stop();
    }

    bool success = false;
    if (timedOut) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("request timed out");
        }
        reply->abort();
    } else if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = reply->errorString();
        }
    } else {
        success = true;
    }

    reply->deleteLater();
    return success;
}
