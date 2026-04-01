#ifndef MAILSTORE_H
#define MAILSTORE_H

#include <QString>
#include <QVector>

#include "mailservice.h"

class MailStore
{
public:
    void loadDemo(const QString &userName);
    void setConversations(const QVector<MailConversation> &conversations);

    QVector<MailConversation> conversations() const;
    QVector<MailConversation> filteredConversations(const QString &keyword) const;

    MailConversation *findConversation(const QString &contactId);
    const MailConversation *findConversation(const QString &contactId) const;

    void upsertConversation(const MailConversation &conversation);
    void appendOutgoingMessage(const QString &contactId, const QString &senderName, const QString &body);
    void markConversationRead(const QString &contactId);
    void renameOutgoingSender(const QString &senderName);

private:
    QVector<MailConversation> m_conversations;
};

#endif // MAILSTORE_H
