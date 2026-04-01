#include "mailstore.h"

void MailStore::loadDemo(const QString &userName)
{
    m_conversations = MailService::createDemoConversations(userName);
}

void MailStore::setConversations(const QVector<MailConversation> &conversations)
{
    m_conversations = conversations;
    for (MailConversation &conversation : m_conversations) {
        MailService::refreshConversationMeta(&conversation);
    }
}

QVector<MailConversation> MailStore::conversations() const
{
    return m_conversations;
}

QVector<MailConversation> MailStore::filteredConversations(const QString &keyword) const
{
    return MailService::filterConversations(m_conversations, keyword);
}

MailConversation *MailStore::findConversation(const QString &contactId)
{
    for (MailConversation &conversation : m_conversations) {
        if (conversation.contactId == contactId) {
            return &conversation;
        }
    }
    return nullptr;
}

const MailConversation *MailStore::findConversation(const QString &contactId) const
{
    for (const MailConversation &conversation : m_conversations) {
        if (conversation.contactId == contactId) {
            return &conversation;
        }
    }
    return nullptr;
}

void MailStore::upsertConversation(const MailConversation &conversation)
{
    for (MailConversation &current : m_conversations) {
        if (current.contactId == conversation.contactId) {
            current = conversation;
            MailService::refreshConversationMeta(&current);
            return;
        }
    }

    m_conversations.push_back(conversation);
    MailService::refreshConversationMeta(&m_conversations.back());
}

void MailStore::appendOutgoingMessage(const QString &contactId, const QString &senderName, const QString &body)
{
    MailConversation *conversation = findConversation(contactId);
    if (conversation == nullptr) {
        return;
    }

    conversation->messages.push_back(MailService::createOutgoingMessage(senderName, body));
    conversation->unreadCount = 0;
    MailService::refreshConversationMeta(conversation);
}

void MailStore::markConversationRead(const QString &contactId)
{
    MailConversation *conversation = findConversation(contactId);
    if (conversation == nullptr) {
        return;
    }

    conversation->unreadCount = 0;
}

void MailStore::renameOutgoingSender(const QString &senderName)
{
    for (MailConversation &conversation : m_conversations) {
        for (MailMessage &message : conversation.messages) {
            if (message.outgoing) {
                message.senderName = senderName;
            }
        }
        MailService::refreshConversationMeta(&conversation);
    }
}
