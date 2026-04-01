#include "maildrawer.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextEdit>
#include <QVBoxLayout>

MailDrawer::MailDrawer(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedSize(420, 560);
    buildUi();
    m_refreshTimer.setInterval(30000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &MailDrawer::autoRefresh);
    m_refreshTimer.start();
    seedDemoData();
}

void MailDrawer::setPlayerInfo(const QString &userName, const QString &userId)
{
    m_userName = userName;
    m_userId = userId;
    m_apiClient.setPlayerIdentity(m_userId, m_userName);

    if (!m_store.conversations().isEmpty()) {
        m_store.renameOutgoingSender(m_userName.isEmpty() ? QStringLiteral("You") : m_userName);
    }

    QString errorMessage;
    if (!reloadFromApi(&errorMessage)) {
        m_remoteActive = false;
        if (m_store.conversations().isEmpty()) {
            seedDemoData();
        }
        updateSubtitle(errorMessage.isEmpty() ? QStringLiteral("demo mode") : errorMessage);
    } else {
        updateSubtitle(QStringLiteral("connected"));
    }

    rebuildKnownContacts();
    rebuildPendingRequests();
    rebuildContactList(m_store.filteredConversations(m_searchInput->text()));
    renderCurrentConversation();
}

void MailDrawer::filterContacts(const QString &keyword)
{
    rebuildContactList(m_store.filteredConversations(keyword));
}

void MailDrawer::openConversation(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    m_activeContactId = item->data(Qt::UserRole).toString();
    m_store.markConversationRead(m_activeContactId);
    if (m_remoteActive) {
        QString errorMessage;
        if (!m_apiClient.markConversationRead(m_activeContactId, &errorMessage)) {
            updateSubtitle(QStringLiteral("read sync: %1").arg(errorMessage));
        }
    }
    rebuildContactList(m_store.filteredConversations(m_searchInput->text()));
    renderCurrentConversation();
}

void MailDrawer::openContact(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    m_targetInput->setText(item->data(Qt::UserRole).toString());
    startConversation();
}

void MailDrawer::handleRequestAction(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    const QString requestId = item->data(Qt::UserRole).toString();
    const QString action = item->data(Qt::UserRole + 1).toString();
    if (requestId.isEmpty() || action.isEmpty() || !m_remoteActive) {
        return;
    }

    QString errorMessage;
    const bool ok = action == QStringLiteral("accept") ? m_apiClient.acceptContactRequest(requestId, &errorMessage)
                                                        : m_apiClient.rejectContactRequest(requestId, &errorMessage);
    if (!ok) {
        updateSubtitle(QStringLiteral("request %1 failed: %2").arg(action, errorMessage));
        return;
    }

    refreshRemoteData();
}

void MailDrawer::sendDraft()
{
    const QString body = m_composer->toPlainText().trimmed();
    if (body.isEmpty()) {
        return;
    }

    MailConversation *conversation = m_store.findConversation(m_activeContactId);
    if (conversation == nullptr) {
        return;
    }

    QString errorMessage;
    if (m_remoteActive) {
        if (!m_apiClient.sendMessage(m_activeContactId, body, conversation, &errorMessage)) {
            m_remoteActive = false;
            m_store.appendOutgoingMessage(m_activeContactId, m_userName.isEmpty() ? QStringLiteral("You") : m_userName, body);
            updateSubtitle(QStringLiteral("send fallback: %1").arg(errorMessage));
        } else {
            updateSubtitle(QStringLiteral("connected"));
        }
    } else {
        m_store.appendOutgoingMessage(m_activeContactId, m_userName.isEmpty() ? QStringLiteral("You") : m_userName, body);
    }

    m_composer->clear();
    renderCurrentConversation();
    rebuildContactList(m_store.filteredConversations(m_searchInput->text()));
}

void MailDrawer::startConversation()
{
    const QString participantUserId = m_targetInput->text().trimmed();
    if (participantUserId.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (m_remoteActive) {
        MailConversation conversation;
        if (!m_apiClient.createConversation(participantUserId, &conversation, &errorMessage)) {
            updateSubtitle(QStringLiteral("open failed: %1").arg(errorMessage));
            return;
        }

        m_store.upsertConversation(conversation);
        m_activeContactId = conversation.contactId;
        updateSubtitle(QStringLiteral("connected"));
    } else {
        MailConversation conversation;
        conversation.contactId = QStringLiteral("local_%1").arg(participantUserId);
        conversation.displayName = participantUserId;
        conversation.tags = QStringList() << QStringLiteral("Local draft");
        conversation.previewText = QStringLiteral("No messages");
        m_store.upsertConversation(conversation);
        m_activeContactId = conversation.contactId;
        updateSubtitle(QStringLiteral("local draft thread"));
    }

    m_targetInput->clear();
    rebuildContactList(m_store.filteredConversations(m_searchInput->text()));
    renderCurrentConversation();
}

void MailDrawer::sendContactRequest()
{
    const QString target = m_targetInput->text().trimmed();
    if (target.isEmpty()) {
        return;
    }

    if (!m_remoteActive) {
        updateSubtitle(QStringLiteral("request only works in remote mode"));
        return;
    }

    QString errorMessage;
    if (!m_apiClient.createContactRequest(target, m_requestRemarkInput->text().trimmed(), &errorMessage)) {
        updateSubtitle(QStringLiteral("request failed: %1").arg(errorMessage));
        return;
    }

    m_targetInput->clear();
    m_requestRemarkInput->clear();
    refreshRemoteData();
}

void MailDrawer::refreshRemoteData()
{
    QString errorMessage;
    if (!reloadFromApi(&errorMessage)) {
        updateSubtitle(QStringLiteral("refresh failed: %1").arg(errorMessage));
        return;
    }

    updateSubtitle(QStringLiteral("refreshed"));
    rebuildKnownContacts();
    rebuildPendingRequests();
    rebuildContactList(m_store.filteredConversations(m_searchInput->text()));
    renderCurrentConversation();
}

void MailDrawer::autoRefresh()
{
    if (!isVisible() || !m_remoteActive) {
        return;
    }

    QString errorMessage;
    if (!reloadFromApi(&errorMessage)) {
        updateSubtitle(QStringLiteral("auto refresh: %1").arg(errorMessage));
        return;
    }

    rebuildKnownContacts();
    rebuildPendingRequests();
    rebuildContactList(m_store.filteredConversations(m_searchInput->text()));
    renderCurrentConversation();
}


void MailDrawer::buildUi()
{
    setStyleSheet(
        "MailDrawer {"
        "background-color: rgba(250, 244, 231, 242);"
        "border: 1px solid rgba(120, 90, 50, 120);"
        "border-radius: 18px;"
        "}"
        "QLabel { color: rgb(74, 52, 34); }"
        "QLineEdit, QTextEdit, QTextBrowser, QListWidget {"
        "background-color: rgba(255, 252, 247, 235);"
        "border: 1px solid rgba(120, 90, 50, 70);"
        "border-radius: 10px;"
        "padding: 6px;"
        "}"
        "QPushButton {"
        "background-color: rgb(209, 145, 71);"
        "color: white;"
        "border: 0;"
        "border-radius: 10px;"
        "padding: 8px 14px;"
        "}"
        "QPushButton:hover { background-color: rgb(190, 126, 54); }"
        "QListWidget::item { padding: 8px; border-radius: 8px; }"
        "QListWidget::item:selected { background-color: rgba(209, 145, 71, 70); }"
    );

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    QFrame *headerCard = new QFrame(this);
    QVBoxLayout *headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(12, 12, 12, 12);
    headerLayout->setSpacing(4);

    QHBoxLayout *titleRow = new QHBoxLayout();
    m_titleLabel = new QLabel(QStringLiteral("Mail"), headerCard);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    titleRow->addWidget(m_titleLabel);
    titleRow->addStretch(1);
    m_refreshButton = new QPushButton(QStringLiteral("Refresh"), headerCard);
    titleRow->addWidget(m_refreshButton);

    m_subtitleLabel = new QLabel(QStringLiteral("Desktop mail drawer"), headerCard);
    m_subtitleLabel->setStyleSheet("color: rgb(125, 101, 78);");

    headerLayout->addLayout(titleRow);
    headerLayout->addWidget(m_subtitleLabel);
    rootLayout->addWidget(headerCard);

    QHBoxLayout *targetLayout = new QHBoxLayout();
    targetLayout->setSpacing(8);
    m_targetInput = new QLineEdit(this);
    m_targetInput->setPlaceholderText(QStringLiteral("User ID"));
    targetLayout->addWidget(m_targetInput, 1);
    m_openButton = new QPushButton(QStringLiteral("Open"), this);
    targetLayout->addWidget(m_openButton);
    rootLayout->addLayout(targetLayout);

    QHBoxLayout *requestLayout = new QHBoxLayout();
    requestLayout->setSpacing(8);
    m_requestRemarkInput = new QLineEdit(this);
    m_requestRemarkInput->setPlaceholderText(QStringLiteral("Remark for request"));
    requestLayout->addWidget(m_requestRemarkInput, 1);
    m_requestButton = new QPushButton(QStringLiteral("Add Contact"), this);
    requestLayout->addWidget(m_requestButton);
    rootLayout->addLayout(requestLayout);

    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(QStringLiteral("Search conversation"));
    rootLayout->addWidget(m_searchInput);

    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(12);

    QVBoxLayout *sidebarLayout = new QVBoxLayout();
    sidebarLayout->setSpacing(8);

    QLabel *knownContactsTitle = new QLabel(QStringLiteral("Contacts"), this);
    sidebarLayout->addWidget(knownContactsTitle);
    m_knownContacts = new QListWidget(this);
    m_knownContacts->setMinimumWidth(160);
    m_knownContacts->setMaximumWidth(180);
    m_knownContacts->setMaximumHeight(110);
    sidebarLayout->addWidget(m_knownContacts);

    QLabel *requestsTitle = new QLabel(QStringLiteral("Requests"), this);
    sidebarLayout->addWidget(requestsTitle);
    m_pendingRequests = new QListWidget(this);
    m_pendingRequests->setMinimumWidth(160);
    m_pendingRequests->setMaximumWidth(180);
    m_pendingRequests->setMaximumHeight(110);
    sidebarLayout->addWidget(m_pendingRequests);

    QLabel *conversationsTitle = new QLabel(QStringLiteral("Conversations"), this);
    sidebarLayout->addWidget(conversationsTitle);
    m_contactList = new QListWidget(this);
    m_contactList->setMinimumWidth(160);
    m_contactList->setMaximumWidth(180);
    sidebarLayout->addWidget(m_contactList, 1);
    contentLayout->addLayout(sidebarLayout);

    QVBoxLayout *threadLayout = new QVBoxLayout();
    threadLayout->setSpacing(10);

    m_threadView = new QTextBrowser(this);
    m_threadView->setOpenExternalLinks(false);
    threadLayout->addWidget(m_threadView, 1);

    m_composer = new QTextEdit(this);
    m_composer->setPlaceholderText(QStringLiteral("Leave a note for the current contact. Real mail APIs come next."));
    m_composer->setFixedHeight(96);
    threadLayout->addWidget(m_composer);

    m_sendButton = new QPushButton(QStringLiteral("Send"), this);
    threadLayout->addWidget(m_sendButton, 0, Qt::AlignRight);

    contentLayout->addLayout(threadLayout, 1);
    rootLayout->addLayout(contentLayout, 1);

    connect(m_refreshButton, &QPushButton::clicked, this, &MailDrawer::refreshRemoteData);
    connect(m_openButton, &QPushButton::clicked, this, &MailDrawer::startConversation);
    connect(m_requestButton, &QPushButton::clicked, this, &MailDrawer::sendContactRequest);
    connect(m_targetInput, &QLineEdit::returnPressed, this, &MailDrawer::startConversation);
    connect(m_searchInput, &QLineEdit::textChanged, this, &MailDrawer::filterContacts);
    connect(m_knownContacts, &QListWidget::itemClicked, this, &MailDrawer::openContact);
    connect(m_pendingRequests, &QListWidget::itemClicked, this, &MailDrawer::handleRequestAction);
    connect(m_contactList, &QListWidget::itemClicked, this, &MailDrawer::openConversation);
    connect(m_sendButton, &QPushButton::clicked, this, &MailDrawer::sendDraft);
}

void MailDrawer::seedDemoData()
{
    m_remoteActive = false;
    m_knownContactItems.clear();
    m_pendingRequestItems.clear();
    m_store.loadDemo(m_userName);
    const QVector<MailConversation> conversations = m_store.conversations();
    rebuildKnownContacts();
    rebuildPendingRequests();
    rebuildContactList(conversations);
    if (!conversations.isEmpty()) {
        m_activeContactId = conversations.first().contactId;
    }
    updateSubtitle(QStringLiteral("demo mode"));
    renderCurrentConversation();
}

bool MailDrawer::reloadFromApi(QString *errorMessage)
{
    QVector<MailContact> contacts;
    if (!m_apiClient.fetchContacts(&contacts, errorMessage)) {
        return false;
    }

    QVector<MailContactRequest> requests;
    if (!m_apiClient.fetchContactRequests(&requests, errorMessage)) {
        return false;
    }

    QVector<MailConversation> conversations;
    if (!m_apiClient.fetchConversations(&conversations, errorMessage)) {
        return false;
    }

    if (conversations.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("remote inbox is empty");
        }
        return false;
    }

    m_knownContactItems = contacts;
    m_pendingRequestItems = requests;
    m_store.setConversations(conversations);
    m_remoteActive = true;
    if (m_store.findConversation(m_activeContactId) == nullptr) {
        m_activeContactId = conversations.first().contactId;
    }
    return true;
}

void MailDrawer::updateSubtitle(const QString &detail)
{
    const QString playerLabel = m_userName.isEmpty() ? QStringLiteral("Unnamed") : m_userName;
    const QString sourceLabel = m_remoteActive ? QStringLiteral("remote") : QStringLiteral("local");
    QString text = QStringLiteral("%1 | %2 | %3").arg(playerLabel, m_userId, sourceLabel);
    if (!detail.isEmpty()) {
        text.append(QStringLiteral(" | %1").arg(detail.left(40)));
    }
    m_subtitleLabel->setText(text);
}

void MailDrawer::rebuildContactList(const QVector<MailConversation> &items)
{
    m_contactList->clear();
    for (const MailConversation &conversation : items) {
        QListWidgetItem *item = new QListWidgetItem(buildContactLabel(conversation), m_contactList);
        item->setData(Qt::UserRole, conversation.contactId);
        item->setToolTip(conversation.tags.join(QStringLiteral(" / ")));
        if (conversation.contactId == m_activeContactId) {
            m_contactList->setCurrentItem(item);
        }
    }

    if (m_contactList->currentItem() == nullptr && m_contactList->count() > 0) {
        m_contactList->setCurrentRow(0);
        m_activeContactId = m_contactList->currentItem()->data(Qt::UserRole).toString();
    }
}

void MailDrawer::rebuildKnownContacts()
{
    m_knownContacts->clear();
    for (const MailContact &contact : m_knownContactItems) {
        QListWidgetItem *item = new QListWidgetItem(buildKnownContactLabel(contact), m_knownContacts);
        item->setData(Qt::UserRole, contact.userId);
        item->setToolTip(contact.userId);
    }
}

void MailDrawer::rebuildPendingRequests()
{
    m_pendingRequests->clear();
    for (const MailContactRequest &request : m_pendingRequestItems) {
        QListWidgetItem *acceptItem = new QListWidgetItem(buildRequestLabel(request) + QStringLiteral("\n[Accept]"), m_pendingRequests);
        acceptItem->setData(Qt::UserRole, request.id);
        acceptItem->setData(Qt::UserRole + 1, QStringLiteral("accept"));

        QListWidgetItem *rejectItem = new QListWidgetItem(buildRequestLabel(request) + QStringLiteral("\n[Reject]"), m_pendingRequests);
        rejectItem->setData(Qt::UserRole, request.id);
        rejectItem->setData(Qt::UserRole + 1, QStringLiteral("reject"));
    }
}

void MailDrawer::renderCurrentConversation()
{
    const MailConversation *conversation = m_store.findConversation(m_activeContactId);
    if (conversation == nullptr) {
        m_threadView->setHtml(QStringLiteral("<p>No matching conversation.</p>"));
        return;
    }

    const QString currentUserName = m_userName.isEmpty() ? QStringLiteral("You") : m_userName;
    m_threadView->setHtml(MailService::renderConversationHtml(*conversation, currentUserName));
}

QString MailDrawer::buildContactLabel(const MailConversation &conversation) const
{
    QString label = conversation.displayName;
    if (!conversation.previewText.isEmpty()) {
        label.append(QStringLiteral("\n%1").arg(conversation.previewText.left(14)));
    }
    if (conversation.unreadCount > 0) {
        label.append(QStringLiteral("\n[%1 unread]").arg(conversation.unreadCount));
    } else if (!conversation.updatedAt.isEmpty()) {
        label.append(QStringLiteral("\n%1").arg(conversation.updatedAt.left(16)));
    }
    return label;
}

QString MailDrawer::buildKnownContactLabel(const MailContact &contact) const
{
    const QString primary = contact.remark.isEmpty() ? contact.displayName : contact.remark;
    QString label = primary.isEmpty() ? contact.userId : primary;
    if (!contact.displayName.isEmpty() && contact.displayName != primary) {
        label.append(QStringLiteral("\n%1").arg(contact.displayName.left(14)));
    }
    label.append(QStringLiteral("\n%1").arg(contact.presence));
    return label;
}

QString MailDrawer::buildRequestLabel(const MailContactRequest &request) const
{
    const QString primary = request.requesterName.isEmpty() ? request.requesterUserId : request.requesterName;
    return QStringLiteral("%1\n%2").arg(primary, request.createdAt.left(16));
}
