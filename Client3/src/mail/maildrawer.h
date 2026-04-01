#ifndef MAILDRAWER_H
#define MAILDRAWER_H

#include <QTimer>
#include <QVector>
#include <QWidget>

#include "mailapiclient.h"
#include "mailstore.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextBrowser;
class QTextEdit;

class MailDrawer : public QWidget
{
    Q_OBJECT

public:
    explicit MailDrawer(QWidget *parent = nullptr);
    void setPlayerInfo(const QString &userName, const QString &userId);

private slots:
    void filterContacts(const QString &keyword);
    void openConversation(QListWidgetItem *item);
    void openContact(QListWidgetItem *item);
    void handleRequestAction(QListWidgetItem *item);
    void sendDraft();
    void startConversation();
    void sendContactRequest();
    void refreshRemoteData();
    void autoRefresh();

private:
    void buildUi();
    void seedDemoData();
    bool reloadFromApi(QString *errorMessage = nullptr);
    void updateSubtitle(const QString &detail = QString());
    void rebuildContactList(const QVector<MailConversation> &items);
    void rebuildKnownContacts();
    void rebuildPendingRequests();
    void renderCurrentConversation();
    QString buildContactLabel(const MailConversation &conversation) const;
    QString buildKnownContactLabel(const MailContact &contact) const;
    QString buildRequestLabel(const MailContactRequest &request) const;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QLineEdit *m_searchInput = nullptr;
    QLineEdit *m_targetInput = nullptr;
    QLineEdit *m_requestRemarkInput = nullptr;
    QListWidget *m_knownContacts = nullptr;
    QListWidget *m_pendingRequests = nullptr;
    QListWidget *m_contactList = nullptr;
    QTextBrowser *m_threadView = nullptr;
    QTextEdit *m_composer = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_requestButton = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_sendButton = nullptr;
    QString m_userName;
    QString m_userId;
    QString m_activeContactId;
    MailApiClient m_apiClient;
    MailStore m_store;
    QVector<MailContact> m_knownContactItems;
    QVector<MailContactRequest> m_pendingRequestItems;
    QTimer m_refreshTimer;
    bool m_remoteActive = false;
};

#endif // MAILDRAWER_H
