#include "mailapiclient.h"

#include <QCryptographicHash>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace {

QString defaultApiBaseUrl()
{
    const QString envValue = qEnvironmentVariable("XIUXIAN_MAIL_API_BASE_URL");
    return envValue.isEmpty() ? QStringLiteral("http://127.0.0.1:3000") : envValue;
}

QString normalizePreview(const QString &value)
{
    const QString simplified = value.simplified();
    return simplified.isEmpty() ? QStringLiteral("No content") : simplified;
}

QString normalizePresence(const QString &value)
{
    return value.isEmpty() ? QStringLiteral("unknown") : value;
}

} // namespace

MailApiClient::MailApiClient()
    : m_apiBaseUrl(defaultApiBaseUrl())
{
}

void MailApiClient::setPlayerIdentity(const QString &userId, const QString &userName)
{
    if (m_userId != userId) {
        m_accessToken.clear();
    }

    m_userId = userId;
    m_userName = userName;
    if (m_userId.isEmpty()) {
        m_deviceId.clear();
        return;
    }

    m_deviceId = QString::fromLatin1(QCryptographicHash::hash(m_userId.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString MailApiClient::apiBaseUrl() const
{
    return m_apiBaseUrl;
}

bool MailApiClient::isConfigured() const
{
    return !m_userId.isEmpty() && !m_deviceId.isEmpty() && !m_apiBaseUrl.isEmpty();
}

bool MailApiClient::fetchContacts(QVector<MailContact> *contacts, QString *errorMessage)
{
    if (contacts == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Missing contacts output");
        }
        return false;
    }

    if (!ensureAccessToken(errorMessage)) {
        return false;
    }

    QByteArray responseBody;
    int statusCode = 0;
    if (!requestJson(QStringLiteral("GET"), QStringLiteral("/api/v1/social/contacts"), QByteArray(), &responseBody, &statusCode, errorMessage)) {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Contacts request failed: %1").arg(statusCode);
        }
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Contacts response is not a JSON object");
        }
        return false;
    }

    QVector<MailContact> nextContacts;
    const QJsonArray items = doc.object().value(QStringLiteral("items")).toArray();
    for (const QJsonValue &value : items) {
        if (value.isObject()) {
            nextContacts.push_back(parseContact(value.toObject()));
        }
    }

    *contacts = nextContacts;
    return true;
}

bool MailApiClient::fetchContactRequests(QVector<MailContactRequest> *requests, QString *errorMessage)
{
    if (requests == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Missing contact requests output");
        }
        return false;
    }

    if (!ensureAccessToken(errorMessage)) {
        return false;
    }

    QByteArray responseBody;
    int statusCode = 0;
    if (!requestJson(QStringLiteral("GET"), QStringLiteral("/api/v1/social/contact-requests"), QByteArray(), &responseBody, &statusCode, errorMessage)) {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Contact requests request failed: %1").arg(statusCode);
        }
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Contact requests response is not a JSON object");
        }
        return false;
    }

    QVector<MailContactRequest> nextRequests;
    const QJsonArray items = doc.object().value(QStringLiteral("items")).toArray();
    for (const QJsonValue &value : items) {
        if (value.isObject()) {
            nextRequests.push_back(parseContactRequest(value.toObject()));
        }
    }

    *requests = nextRequests;
    return true;
}

bool MailApiClient::createContactRequest(const QString &target, const QString &remark, QString *errorMessage)
{
    if (!ensureAccessToken(errorMessage)) {
        return false;
    }

    const QJsonObject payload{{QStringLiteral("target"), target}, {QStringLiteral("remark"), remark}};
    QByteArray responseBody;
    int statusCode = 0;
    if (!requestJson(QStringLiteral("POST"), QStringLiteral("/api/v1/social/contact-requests"), QJsonDocument(payload).toJson(QJsonDocument::Compact), &responseBody, &statusCode, errorMessage)) {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Create request failed: %1").arg(statusCode);
        }
        return false;
    }

    return true;
}

bool MailApiClient::acceptContactRequest(const QString &requestId, QString *errorMessage)
{
    if (!ensureAccessToken(errorMessage)) {
        return false;
    }

    QByteArray responseBody;
    int statusCode = 0;
    const QString path = QStringLiteral("/api/v1/social/contact-requests/%1/accept").arg(QString::fromUtf8(QUrl::toPercentEncoding(requestId)));
    if (!requestJson(QStringLiteral("POST"), path, QByteArray("{}"), &responseBody, &statusCode, errorMessage)) {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Accept request failed: %1").arg(statusCode);
        }
        return false;
    }

    return true;
}

bool MailApiClient::rejectContactRequest(const QString &requestId, QString *errorMessage)
{
    if (!ensureAccessToken(errorMessage)) {
        return false;
    }

    QByteArray responseBody;
    int statusCode = 0;
    const QString path = QStringLiteral("/api/v1/social/contact-requests/%1/reject").arg(QString::fromUtf8(QUrl::toPercentEncoding(requestId)));
    if (!requestJson(QStringLiteral("POST"), path, QByteArray("{}"), &responseBody, &statusCode, errorMessage)) {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Reject request failed: %1").arg(statusCode);
        }
        return false;
    }

    return true;
}


bool MailApiClient::fetchConversations(QVector<MailConversation> *conversations, QString *errorMessage)
{
    if (conversations == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Missing conversations output");
        }
        return false;
    }

    if (!ensureAccessToken(errorMessage)) {
        return false;
    }

    QByteArray responseBody;
    int statusCode = 0;
    if (!requestJson(QStringLiteral("GET"), QStringLiteral("/api/v1/social/conversations"), QByteArray(), &responseBody, &statusCode, errorMessage)) {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Conversation request failed: %1").arg(statusCode);
        }
        return false;
    }

    const QJsonDocument conversationDoc = QJsonDocument::fromJson(responseBody);
    if (!conversationDoc.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Conversation response is not a JSON object");
        }
        return false;
    }

    QVector<MailConversation> nextConversations;
    const QJsonArray conversationItems = conversationDoc.object().value(QStringLiteral("items")).toArray();
    for (const QJsonValue &conversationValue : conversationItems) {
        if (!conversationValue.isObject()) {
            continue;
        }

        MailConversation conversation = parseConversation(conversationValue.toObject());

        QByteArray messagesBody;
        int messagesStatus = 0;
        const QString messagesPath = QStringLiteral("/api/v1/social/conversations/%1/messages").arg(QString::fromUtf8(QUrl::toPercentEncoding(conversation.contactId)));
        if (!requestJson(QStringLiteral("GET"), messagesPath, QByteArray(), &messagesBody, &messagesStatus, errorMessage)) {
            return false;
        }
        if (messagesStatus < 200 || messagesStatus >= 300) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Message request failed: %1").arg(messagesStatus);
            }
            return false;
        }

        const QJsonDocument messagesDoc = QJsonDocument::fromJson(messagesBody);
        const QJsonArray messageItems = messagesDoc.object().value(QStringLiteral("items")).toArray();
        for (const QJsonValue &messageValue : messageItems) {
            if (messageValue.isObject()) {
                conversation.messages.push_back(parseMessage(messageValue.toObject()));
            }
        }

        MailService::refreshConversationMeta(&conversation);
        nextConversations.push_back(conversation);
    }

    *conversations = nextConversations;
    return true;
}

bool MailApiClient::sendMessage(const QString &conversationId, const QString &body, MailConversation *conversation, QString *errorMessage)
{
    if (conversation == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Missing conversation output");
        }
        return false;
    }

    if (!ensureAccessToken(errorMessage)) {
        return false;
    }

    const QJsonObject payload{{QStringLiteral("kind"), QStringLiteral("text")}, {QStringLiteral("content"), body}, {QStringLiteral("requiresAck"), false}};
    QByteArray responseBody;
    int statusCode = 0;
    const QString path = QStringLiteral("/api/v1/social/conversations/%1/messages").arg(QString::fromUtf8(QUrl::toPercentEncoding(conversationId)));
    if (!requestJson(QStringLiteral("POST"), path, QJsonDocument(payload).toJson(QJsonDocument::Compact), &responseBody, &statusCode, errorMessage)) {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Send message failed: %1").arg(statusCode);
        }
        return false;
    }

    conversation->messages.push_back(MailService::createOutgoingMessage(m_userName.isEmpty() ? QStringLiteral("You") : m_userName, body));
    conversation->unreadCount = 0;
    MailService::refreshConversationMeta(conversation);
    return true;
}

bool MailApiClient::markConversationRead(const QString &conversationId, QString *errorMessage)
{
    if (!ensureAccessToken(errorMessage)) {
        return false;
    }

    QByteArray responseBody;
    int statusCode = 0;
    const QString path = QStringLiteral("/api/v1/social/conversations/%1/read").arg(QString::fromUtf8(QUrl::toPercentEncoding(conversationId)));
    if (!requestJson(QStringLiteral("POST"), path, QByteArray("{}"), &responseBody, &statusCode, errorMessage)) {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Read sync failed: %1").arg(statusCode);
        }
        return false;
    }

    return true;
}

bool MailApiClient::createConversation(const QString &participantUserId, MailConversation *conversation, QString *errorMessage)
{
    if (conversation == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Missing conversation output");
        }
        return false;
    }

    if (!ensureAccessToken(errorMessage)) {
        return false;
    }

    const QJsonObject payload{{QStringLiteral("participantUserId"), participantUserId}};
    QByteArray responseBody;
    int statusCode = 0;
    if (!requestJson(QStringLiteral("POST"), QStringLiteral("/api/v1/social/conversations"), QJsonDocument(payload).toJson(QJsonDocument::Compact), &responseBody, &statusCode, errorMessage)) {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Create conversation failed: %1").arg(statusCode);
        }
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    if (!doc.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Conversation create response is not a JSON object");
        }
        return false;
    }

    *conversation = parseConversation(doc.object());
    return true;
}


bool MailApiClient::ensureAccessToken(QString *errorMessage)
{
    if (!isConfigured()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Mail API client is not configured");
        }
        return false;
    }

    if (!m_accessToken.isEmpty()) {
        return true;
    }

    const QJsonObject payload{{QStringLiteral("deviceId"), m_deviceId}, {QStringLiteral("deviceName"), m_userName.isEmpty() ? QStringLiteral("Xiuxian Qt") : m_userName}, {QStringLiteral("platform"), QStringLiteral("windows")}};
    QByteArray responseBody;
    int statusCode = 0;
    if (!requestJson(QStringLiteral("POST"), QStringLiteral("/api/v1/auth/device-login"), QJsonDocument(payload).toJson(QJsonDocument::Compact), &responseBody, &statusCode, errorMessage, false)) {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Device login failed: %1").arg(statusCode);
        }
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(responseBody);
    const QString token = doc.object().value(QStringLiteral("accessToken")).toString();
    if (token.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Device login response did not contain accessToken");
        }
        return false;
    }

    m_accessToken = token;
    return true;
}

bool MailApiClient::requestJson(const QString &method, const QString &path, const QByteArray &body, QByteArray *responseBody, int *statusCode, QString *errorMessage, bool authorized)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(m_apiBaseUrl + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (authorized && !bearerToken().isEmpty()) {
        request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(bearerToken()).toUtf8());
    }

    QNetworkReply *reply = nullptr;
    if (method == QStringLiteral("GET")) {
        reply = manager.get(request);
    } else if (method == QStringLiteral("POST")) {
        reply = manager.post(request, body);
    } else {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unsupported HTTP method: %1").arg(method);
        }
        return false;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(3500);
    loop.exec();

    if (timer.isActive()) {
        timer.stop();
    } else {
        reply->abort();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("HTTP request timeout: %1 %2").arg(method, path);
        }
    }

    if (statusCode != nullptr) {
        *statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    if (responseBody != nullptr) {
        *responseBody = reply->readAll();
    }

    const bool networkOk = reply->error() == QNetworkReply::NoError || (statusCode != nullptr && *statusCode >= 400);
    if (!networkOk) {
        if (errorMessage != nullptr && errorMessage->isEmpty()) {
            *errorMessage = reply->errorString();
        }
        reply->deleteLater();
        return false;
    }

    reply->deleteLater();
    return true;
}

MailContact MailApiClient::parseContact(const QJsonObject &contactObject) const
{
    MailContact contact;
    contact.userId = contactObject.value(QStringLiteral("contactUserId")).toString();
    contact.displayName = contactObject.value(QStringLiteral("displayName")).toString();
    contact.remark = contactObject.value(QStringLiteral("remark")).toString();
    contact.presence = normalizePresence(contactObject.value(QStringLiteral("presence")).toString());
    contact.transportReady = contactObject.value(QStringLiteral("transportReady")).toBool();
    return contact;
}

MailContactRequest MailApiClient::parseContactRequest(const QJsonObject &requestObject) const
{
    MailContactRequest request;
    request.id = requestObject.value(QStringLiteral("id")).toString();
    request.requesterUserId = requestObject.value(QStringLiteral("requesterUserId")).toString();
    request.requesterName = requestObject.value(QStringLiteral("requesterName")).toString();
    request.targetUserId = requestObject.value(QStringLiteral("targetUserId")).toString();
    request.targetName = requestObject.value(QStringLiteral("targetName")).toString();
    request.createdAt = requestObject.value(QStringLiteral("createdAt")).toString();
    request.status = requestObject.value(QStringLiteral("status")).toString();
    return request;
}

MailConversation MailApiClient::parseConversation(const QJsonObject &conversationObject) const
{
    MailConversation conversation;
    conversation.contactId = conversationObject.value(QStringLiteral("id")).toString();
    conversation.displayName = conversationObject.value(QStringLiteral("title")).toString();
    if (conversation.displayName.isEmpty()) {
        conversation.displayName = conversationObject.value(QStringLiteral("participantName")).toString();
    }
    if (conversation.displayName.isEmpty()) {
        conversation.displayName = conversationObject.value(QStringLiteral("participantUserId")).toString();
    }
    conversation.previewText = normalizePreview(conversationObject.value(QStringLiteral("lastMessagePreview")).toString());
    conversation.updatedAt = conversationObject.value(QStringLiteral("updatedAt")).toString();
    conversation.unreadCount = conversationObject.value(QStringLiteral("unreadCount")).toInt();
    conversation.tags = QStringList() << QStringLiteral("Remote") << conversationObject.value(QStringLiteral("participantUserId")).toString();
    return conversation;
}

MailMessage MailApiClient::parseMessage(const QJsonObject &messageObject) const
{
    MailMessage message;
    const QString senderUserId = messageObject.value(QStringLiteral("senderUserId")).toString();
    message.senderName = senderUserId == m_userId ? (m_userName.isEmpty() ? QStringLiteral("You") : m_userName) : senderUserId;
    message.body = messageObject.value(QStringLiteral("content")).toString();
    message.timestamp = messageObject.value(QStringLiteral("createdAt")).toString();
    message.outgoing = senderUserId == m_userId;
    return message;
}

QString MailApiClient::bearerToken() const
{
    return m_accessToken;
}
