#pragma once

#include <QLabel>
#include <QMainWindow>
#include <QListWidget>
#include <QTextEdit>
#include <QThread>

#include "conservsocket.h"
#include "emojipicker.h"
#include "qnchatmessage.h"
#include "wordfilter.h"

class ChatWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow() override;

    void setInfo(const QString &name, const QString &id);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *target, QEvent *event) override;

signals:
    void sendMsg(QByteArray);

private slots:
    void onSend();
    void onReceive(const QString &msg);
    void onSocketState(bool open);
    void onChooseHead();
    void onChooseEmoji();
    void addEmoji(const QString &e);

private:
    void addMessageShe(const QString &msg, const QString &name, const QString &head);
    void addMessageMe(const QString &msg, const QString &head);
    void dealMessage(QNChatMessage *w, QListWidgetItem *item,
                     const QString &text, const QString &time,
                     QNChatMessage::UserType type);
    void dealMessageTime(const QString &curTime);
    void buildUi();

    // ── UI ──────────────────────────────────────────────────────────────
    QListWidget *m_listWidget  = nullptr;
    QTextEdit   *m_inputEdit   = nullptr;
    QLabel      *m_stateLabel  = nullptr;

    // ── Deps ─────────────────────────────────────────────────────────────
    ConServSocket m_socket;
    QThread       m_socketThread;
    EmojiPicker   m_emojiPicker;
    WordFilter    m_wordFilter;

    QString m_name     = "null";
    QString m_uuid     = "uuid";
    QString m_headPng;          // Base64 头像
    QString m_lastMsgTime;
};
