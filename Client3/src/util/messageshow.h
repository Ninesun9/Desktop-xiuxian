#pragma once

#include <QDialog>
#include <QLabel>
#include <QTimer>

// 右下角浮动弹窗，自动滑入停留后淡出
class MessageShow : public QDialog
{
    Q_OBJECT

public:
    explicit MessageShow(QWidget *parent = nullptr);

    void setInfomation(const QString &title, const QString &msg);
    void setInfomation(const QString &title, const QString &msg, const QString &url);

    // 由 PopupManager 调用，通知位置（避免重叠）
    void updatePosition();

signals:
    void sigClose(MessageShow *self);
    void sigClickUrl(const QString &url);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void slotMsgMove();
    void slotMsgStay();
    void slotMsgClose();

private:
    void showMessage();

    QLabel  *m_titleLabel = nullptr;
    QLabel  *m_msgLabel   = nullptr;

    QTimer  *m_showTimer  = nullptr;
    QTimer  *m_stayTimer  = nullptr;
    QTimer  *m_closeTimer = nullptr;

    QPoint   m_endPoint;
    int      m_currentY    = 0;
    int      m_stayCount   = 0;
    double   m_opacity     = 1.0;
    bool     m_firstShow   = true;
    bool     m_hovered     = false;
};
