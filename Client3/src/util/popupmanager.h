#pragma once

#include <QList>
#include <QObject>
#include "messageshow.h"

class PopupManager : public QObject
{
    Q_OBJECT

public:
    static PopupManager *instance();

    void show(const QString &title, const QString &msg);
    void show(const QString &title, const QString &msg, const QString &url);

signals:
    void clickUrl(const QString &url);

private slots:
    void onClose(MessageShow *popup);

private:
    explicit PopupManager(QObject *parent = nullptr);
    void     add(MessageShow *popup);
    void     restack();

    QList<MessageShow *> m_list;
};
