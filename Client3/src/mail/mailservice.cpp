#include "mailservice.h"

#include <QDateTime>

namespace {

MailConversation makeConversation(const QString &contactId,
                                  const QString &displayName,
                                  const QStringList &tags,
                                  const QVector<MailMessage> &messages,
                                  int unreadCount)
{
    MailConversation conversation;
    conversation.contactId = contactId;
    conversation.displayName = displayName;
    conversation.tags = tags;
    conversation.messages = messages;
    conversation.unreadCount = unreadCount;
    MailService::refreshConversationMeta(&conversation);
    return conversation;
}

} // namespace

QVector<MailConversation> MailService::createDemoConversations(const QString &userName)
{
    const QString playerName = userName.isEmpty() ? QStringLiteral("You") : userName;

    return {
        makeConversation(
            QStringLiteral("contact_qinglan"),
            QStringLiteral("Qinglan"),
            {QStringLiteral("Sect"), QStringLiteral("Online")},
            {
                {QStringLiteral("Qinglan"), QStringLiteral("Check the spirit field before midnight."), QStringLiteral("08:20"), false},
                {playerName, QStringLiteral("Noted. I will head over later."), QStringLiteral("08:23"), true}
            },
            0),
        makeConversation(
            QStringLiteral("contact_bailu"),
            QStringLiteral("Bailu"),
            {QStringLiteral("Chat")},
            {
                {QStringLiteral("Bailu"), QStringLiteral("I sorted a travel journal for you."), QStringLiteral("Yesterday"), false}
            },
            1),
        makeConversation(
            QStringLiteral("contact_danfang"),
            QStringLiteral("Alchemy Hall"),
            {QStringLiteral("Task"), QStringLiteral("Reply needed")},
            {
                {QStringLiteral("Alchemy Hall"), QStringLiteral("The medicine catalyst is ready. When will you pick it up?"), QStringLiteral("09:15"), false}
            },
            2),
        makeConversation(
            QStringLiteral("contact_pet"),
            QStringLiteral("Pet Assistant"),
            {QStringLiteral("System")},
            {
                {QStringLiteral("Pet Assistant"), QStringLiteral("The Qt mail drawer is ready for a real backend."), QStringLiteral("Just now"), false}
            },
            0)
    };
}

QVector<MailConversation> MailService::filterConversations(const QVector<MailConversation> &items, const QString &keyword)
{
    const QString normalized = keyword.trimmed();
    if (normalized.isEmpty()) {
        return items;
    }

    QVector<MailConversation> filtered;
    for (const MailConversation &item : items) {
        if (item.displayName.contains(normalized, Qt::CaseInsensitive)
            || item.contactId.contains(normalized, Qt::CaseInsensitive)
            || item.previewText.contains(normalized, Qt::CaseInsensitive)
            || item.tags.join(' ').contains(normalized, Qt::CaseInsensitive)) {
            filtered.push_back(item);
        }
    }
    return filtered;
}

QString MailService::renderConversationHtml(const MailConversation &conversation, const QString &currentUserName)
{
    QString html;
    html.append(QStringLiteral("<h3>%1</h3>").arg(conversation.displayName));
    if (!conversation.tags.isEmpty()) {
        html.append(QStringLiteral("<p><b>Tags:</b> %1</p>").arg(conversation.tags.join(QStringLiteral(" / "))));
    }

    for (const MailMessage &message : conversation.messages) {
        const QString speaker = message.outgoing ? currentUserName : message.senderName;
        html.append(QStringLiteral("<p><b>%1</b> <span style='color:#8f7a63;'>%2</span><br/>%3</p>")
                        .arg(speaker.toHtmlEscaped(),
                             message.timestamp.toHtmlEscaped(),
                             message.body.toHtmlEscaped()));
    }

    return html;
}

MailMessage MailService::createOutgoingMessage(const QString &senderName, const QString &body)
{
    MailMessage message;
    message.senderName = senderName;
    message.body = body;
    message.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm"));
    message.outgoing = true;
    return message;
}

void MailService::refreshConversationMeta(MailConversation *conversation)
{
    if (conversation == nullptr) {
        return;
    }

    if (conversation->messages.isEmpty()) {
        if (conversation->previewText.isEmpty()) {
            conversation->previewText = QStringLiteral("No messages");
        }
        return;
    }

    const MailMessage &lastMessage = conversation->messages.last();
    conversation->previewText = lastMessage.body;
    conversation->updatedAt = lastMessage.timestamp;
}
