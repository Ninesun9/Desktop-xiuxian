#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class ConServSocket : public QObject
{
    Q_OBJECT
public:
    explicit ConServSocket(QObject *parent = nullptr);

    void initThis();
    void send(const QByteArray &data);

    bool state = false;

public slots:
    void linkToServer();

signals:
    void receiveMessage(const QString &msg);
    void isOpen(bool open);

private slots:
    void onReadyRead();
};
