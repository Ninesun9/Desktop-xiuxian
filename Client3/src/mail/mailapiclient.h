#ifndef MAILAPICLIENT_H
#define MAILAPICLIENT_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include "mailservice.h"

struct MailContact
{
    QString userId;
    QString displayName;
    QString remark;
    QString presence;
    bool transportReady = false;
};

struct MailContactRequest
{
    QString id;
    QString requesterUserId;
    QString requesterName;
    QString targetUserId;
    QString targetName;
    QString createdAt;
    QString status;
};

class MailApiClient
{
public:
    MailApiClient();

    void setPlayerIdentity(const QString &userId, const QString &userName);
    QString apiBaseUrl() const;
    bool isConfigured() const;

    bool fetchContacts(QVector<MailContact> *contacts, QString *errorMessage = nullptr);
    bool fetchContactRequests(QVector<MailContactRequest> *requests, QString *errorMessage = nullptr);
    bool createContactRequest(const QString &target, const QString &remark, QString *errorMessage = nullptr);
    bool acceptContactRequest(const QString &requestId, QString *errorMessage = nullptr);
    bool rejectContactRequest(const QString &requestId, QString *errorMessage = nullptr);
    bool fetchConversations(QVector<MailConversation> *conversations, QString *errorMessage = nullptr);
    bool sendMessage(const QString &conversationId, const QString &body, MailConversation *conversation, QString *errorMessage = nullptr);
    bool markConversationRead(const QString &conversationId, QString *errorMessage = nullptr);
    bool createConversation(const QString &participantUserId, MailConversation *conversation, QString *errorMessage = nullptr);

private:
    bool ensureAccessToken(QString *errorMessage);
    bool requestJson(const QString &method,
                     const QString &path,
                     const QByteArray &body,
                     QByteArray *responseBody,
                     int *statusCode,
                     QString *errorMessage,
                     bool authorized = true);
    MailContact parseContact(const QJsonObject &contactObject) const;
    MailContactRequest parseContactRequest(const QJsonObject &requestObject) const;
    MailConversation parseConversation(const QJsonObject &conversationObject) const;
    MailMessage parseMessage(const QJsonObject &messageObject) const;
    QString bearerToken() const;

    QString m_userId;
    QString m_userName;
    QString m_deviceId;
    QString m_apiBaseUrl;
    QString m_accessToken;
};

#endif // MAILAPICLIENT_H
