#include "chatwindow.h"

#include <QApplication>
#include <QBuffer>
#include <QDateTime>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QVBoxLayout>

ChatWindow::ChatWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("聊天");
    setWindowFlag(Qt::Window);
    resize(600, 800);

    // 加载头像
    QSettings cfg(QDir::homePath() + "/AppData/Roaming/xiuxian_chat.ini",
                  QSettings::IniFormat);
    m_headPng = cfg.value("xiuxian/head").toString();

    buildUi();

    // 把 socket 移到独立线程，避免阻塞 UI
    m_socket.moveToThread(&m_socketThread);
    connect(&m_socketThread, &QThread::started, &m_socket, &ConServSocket::initThis);
    connect(&m_socket, &ConServSocket::receiveMessage, this, &ChatWindow::onReceive);
    connect(&m_socket, &ConServSocket::isOpen,         this, &ChatWindow::onSocketState);
    connect(this, &ChatWindow::sendMsg, &m_socket, &ConServSocket::send);
    m_socketThread.start();

    connect(&m_emojiPicker, &EmojiPicker::chooseEmoji, this, &ChatWindow::addEmoji);

    m_inputEdit->installEventFilter(this);
}

ChatWindow::~ChatWindow()
{
    m_socketThread.quit();
    m_socketThread.wait();
}

void ChatWindow::setInfo(const QString &name, const QString &id)
{
    m_name = name;
    m_uuid = id;
}

// ── Build UI ────────────────────────────────────────────────────────────────

void ChatWindow::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // 状态行
    auto *topRow = new QHBoxLayout;
    m_stateLabel = new QLabel("● 连接中…", central);
    m_stateLabel->setStyleSheet("color:gray;");

    auto *headBtn = new QPushButton("头像", central);
    headBtn->setFixedWidth(48);
    auto *emojiBtn = new QPushButton("😊", central);
    emojiBtn->setFixedWidth(36);

    topRow->addWidget(m_stateLabel, 1);
    topRow->addWidget(emojiBtn);
    topRow->addWidget(headBtn);
    root->addLayout(topRow);

    // 消息列表
    m_listWidget = new QListWidget(central);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setSpacing(4);
    root->addWidget(m_listWidget, 1);

    // 输入框 + 发送按钮
    auto *inputRow = new QHBoxLayout;
    m_inputEdit = new QTextEdit(central);
    m_inputEdit->setFixedHeight(80);
    auto *sendBtn = new QPushButton("发送\n(Ctrl+↵)", central);
    sendBtn->setFixedSize(72, 80);

    inputRow->addWidget(m_inputEdit, 1);
    inputRow->addWidget(sendBtn);
    root->addLayout(inputRow);

    connect(sendBtn,  &QPushButton::clicked,  this, &ChatWindow::onSend);
    connect(headBtn,  &QPushButton::clicked,  this, &ChatWindow::onChooseHead);
    connect(emojiBtn, &QPushButton::clicked,  this, &ChatWindow::onChooseEmoji);
}

// ── Events ──────────────────────────────────────────────────────────────────

void ChatWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
}

bool ChatWindow::eventFilter(QObject *target, QEvent *event)
{
    if (target == m_inputEdit && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Return &&
            ke->modifiers() & Qt::ControlModifier) {
            onSend();
            return true;
        }
    }
    return QMainWindow::eventFilter(target, event);
}

// ── Slots ────────────────────────────────────────────────────────────────────

void ChatWindow::onSend()
{
    QString msg = m_inputEdit->toPlainText();
    msg.remove('\r'); msg.remove('\n');
    msg.remove(' ');  msg.remove('\t');
    if (msg.isEmpty()) return;
    if (msg.size() > 100) msg.chop(msg.size() - 100);

    m_inputEdit->clear();

    // 协议: ID|昵称|头像Base64|消息，Base64+'\n'
    const QString full = m_uuid + '|' + m_name + '|' + m_headPng
                         + '|' + m_wordFilter.filter(msg);
    QByteArray ba = full.toUtf8().toBase64();
    ba.append('\n');
    emit sendMsg(ba);
}

void ChatWindow::onReceive(const QString &msg)
{
    const QStringList parts = msg.split('|');
    if (parts.size() < 4 || parts.at(3).size() > 110)
        return;

    QApplication::alert(this);
    if (parts.at(0) == m_uuid)
        addMessageMe(parts.at(3), parts.at(2));
    else
        addMessageShe(parts.at(3), parts.at(1), parts.at(2));
}

void ChatWindow::onSocketState(bool open)
{
    m_stateLabel->setText(open ? "● 已连接" : "● 已断线");
    m_stateLabel->setStyleSheet(open ? "color:green;" : "color:red;");
}

void ChatWindow::onChooseHead()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "选择头像", QString(), "图片 (*.png *.jpg *.bmp)");
    if (path.isEmpty()) return;

    QImage img(path);
    if (img.isNull()) return;

    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.scaled(64, 64, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
       .save(&buf, "PNG");
    m_headPng = ba.toBase64();

    QSettings cfg(QDir::homePath() + "/AppData/Roaming/xiuxian_chat.ini",
                  QSettings::IniFormat);
    cfg.setValue("xiuxian/head", m_headPng);
}

void ChatWindow::onChooseEmoji()
{
    m_emojiPicker.show();
}

void ChatWindow::addEmoji(const QString &e)
{
    m_inputEdit->insertPlainText(e);
}

// ── Message rendering ────────────────────────────────────────────────────────

void ChatWindow::dealMessageTime(const QString &curTime)
{
    if (m_lastMsgTime == curTime) return;
    m_lastMsgTime = curTime;

    auto *timeItem = new QListWidgetItem(m_listWidget);
    auto *timeLabel = new QLabel(curTime, m_listWidget);
    timeLabel->setAlignment(Qt::AlignCenter);
    timeLabel->setStyleSheet("color:gray; font-size:11px;");
    timeItem->setSizeHint(QSize(m_listWidget->width(), 24));
    m_listWidget->setItemWidget(timeItem, timeLabel);
}

void ChatWindow::dealMessage(QNChatMessage *w, QListWidgetItem *item,
                             const QString &text, const QString &time,
                             QNChatMessage::UserType type)
{
    w->setText(text, time, 13, type);
    item->setSizeHint(w->sizeHint());
    m_listWidget->setItemWidget(item, w);
}

void ChatWindow::addMessageShe(const QString &msg, const QString &name, const QString &head)
{
    if (msg.isEmpty()) return;
    const QString time = QString::number(QDateTime::currentSecsSinceEpoch());
    dealMessageTime(time);

    auto *w    = new QNChatMessage(m_listWidget->viewport(), name, head);
    auto *item = new QListWidgetItem(m_listWidget);
    dealMessage(w, item, msg, time, QNChatMessage::User_She);
    m_listWidget->scrollToBottom();
}

void ChatWindow::addMessageMe(const QString &msg, const QString &head)
{
    if (msg.isEmpty()) return;
    const QString time = QString::number(QDateTime::currentSecsSinceEpoch());
    dealMessageTime(time);

    auto *w    = new QNChatMessage(m_listWidget->viewport(), m_name, head);
    auto *item = new QListWidgetItem(m_listWidget);
    dealMessage(w, item, msg, time, QNChatMessage::User_Me);
    m_listWidget->scrollToBottom();
}
