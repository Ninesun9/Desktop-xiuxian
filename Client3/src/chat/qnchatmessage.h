#pragma once

#include <QWidget>

// 气泡式聊天消息控件（QPainter 自绘）
class QNChatMessage : public QWidget
{
    Q_OBJECT
public:
    enum UserType { User_Me, User_She };

    explicit QNChatMessage(QWidget *parent,
                           const QString &name  = QString(),
                           const QString &head  = QString());

    void setText(const QString &text, const QString &time,
                 int fontSize, UserType type);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString  m_text;
    QString  m_time;
    QString  m_name;
    QString  m_headBase64;   // Base64 PNG
    int      m_fontSize = 12;
    UserType m_type     = User_She;
    QSize    m_size;

    static constexpr int kAvatarSize  = 36;
    static constexpr int kBubblePad   = 10;
    static constexpr int kBubbleRadius = 8;
};
