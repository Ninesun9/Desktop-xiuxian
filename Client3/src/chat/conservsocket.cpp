#include "conservsocket.h"

#include <QHostInfo>

static constexpr char kHost[] = "107.174.220.99";
static constexpr quint16 kPort = 8524;

ConServSocket::ConServSocket(QObject *parent)
    : QObject(parent)
{}

void ConServSocket::initThis()
{
    auto *socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &ConServSocket::onReadyRead);
    connect(socket, &QTcpSocket::connected, this, [this]{ emit isOpen(true); });
    connect(socket, &QTcpSocket::disconnected, this, [this]{ emit isOpen(false); });

    // 使用 lambda 捕获 socket 指针
    auto reconnect = [socket]() {
        if (socket->state() != QAbstractSocket::ConnectedState &&
            socket->state() != QAbstractSocket::ConnectingState) {
            const auto addrs = QHostInfo::fromName(kHost).addresses();
            if (!addrs.isEmpty())
                socket->connectToHost(addrs.first(), kPort);
        }
    };

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, reconnect);
    timer->start(2000);
    reconnect();
}

void ConServSocket::send(const QByteArray &data)
{
    auto *socket = findChild<QTcpSocket *>();
    if (socket)
        socket->write(data);
}

void ConServSocket::onReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) return;

    // 协议：Base64 编码，以 '\n' 结尾
    QByteArray buf;
    for (int i = 0; i < 100; ++i) {
        buf.append(socket->readAll());
        if (buf.endsWith('\n')) {
            buf.chop(1);
            break;
        }
        // 等待更多数据到来（非阻塞：下次 readyRead 会再触发）
        if (!socket->waitForReadyRead(10))
            break;
    }

    if (!buf.isEmpty())
        emit receiveMessage(QString::fromUtf8(QByteArray::fromBase64(buf)));
}

void ConServSocket::linkToServer()
{
    // 保留向后兼容的槽，内部由 timer 驱动
}
