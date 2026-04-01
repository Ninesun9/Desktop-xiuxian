#include "popupmanager.h"

PopupManager *PopupManager::instance()
{
    static PopupManager inst;
    return &inst;
}

PopupManager::PopupManager(QObject *parent) : QObject(parent) {}

void PopupManager::show(const QString &title, const QString &msg)
{
    auto *p = new MessageShow;
    p->setInfomation(title, msg);
    add(p);
}

void PopupManager::show(const QString &title, const QString &msg, const QString &url)
{
    auto *p = new MessageShow;
    p->setInfomation(title, msg, url);
    add(p);
}

void PopupManager::add(MessageShow *popup)
{
    connect(popup, &MessageShow::sigClose,    this, &PopupManager::onClose);
    connect(popup, &MessageShow::sigClickUrl, this, &PopupManager::clickUrl);
    m_list.append(popup);
    restack();
}

void PopupManager::onClose(MessageShow *popup)
{
    disconnect(popup, nullptr, this, nullptr);
    m_list.removeOne(popup);
    popup->deleteLater();
}

void PopupManager::restack()
{
    for (auto *p : m_list)
        p->updatePosition();
}
