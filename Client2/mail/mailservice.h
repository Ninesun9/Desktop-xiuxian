#ifndef MAILSERVICE_H
#define MAILSERVICE_H

#include <QString>
#include <QStringList>
#include <QVector>

struct MailMessage
{
    QString senderName;
    QString body;
    QString timestamp;
    bool outgoing = false;
};

struct MailConversation
{
    QString contactId;
    QString displayName;
    QStringList tags;
    QVector<MailMessage> messages;
    QString previewText;
    QString updatedAt;
    int unreadCount = 0;
};

class MailService
{
public:
    static QVector<MailConversation> createDemoConversations(const QString &userName);
    static QVector<MailConversation> filterConversations(const QVector<MailConversation> &items, const QString &keyword);
    static QString renderConversationHtml(const MailConversation &conversation, const QString &currentUserName);
    static MailMessage createOutgoingMessage(const QString &senderName, const QString &body);
    static void refreshConversationMeta(MailConversation *conversation);
};

#endif // MAILSERVICE_H
