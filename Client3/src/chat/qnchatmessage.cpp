#include "qnchatmessage.h"

#include <QBuffer>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

QNChatMessage::QNChatMessage(QWidget *parent, const QString &name, const QString &head)
    : QWidget(parent)
    , m_name(name)
    , m_headBase64(head)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void QNChatMessage::setText(const QString &text, const QString &time,
                            int fontSize, UserType type)
{
    m_text     = text;
    m_time     = time;
    m_fontSize = fontSize > 0 ? fontSize : 12;
    m_type     = type;

    QFont f;
    f.setPointSize(m_fontSize);
    QFontMetrics fm(f);

    const int maxWidth = 260;
    const QRect br = fm.boundingRect(QRect(0, 0, maxWidth, 0),
                                     Qt::TextWordWrap, m_text);
    const int bubbleW = br.width()  + kBubblePad * 2;
    const int bubbleH = br.height() + kBubblePad * 2;
    const int totalW  = kAvatarSize + 8 + bubbleW;
    const int totalH  = qMax(kAvatarSize, bubbleH) + 6;

    m_size = QSize(totalW + 20, totalH);
    resize(m_size);
}

QSize QNChatMessage::sizeHint() const { return m_size; }

static QPixmap avatarFromBase64(const QString &b64, int size)
{
    if (b64.isEmpty()) {
        QPixmap p(size, size);
        p.fill(QColor(180, 140, 90));
        return p;
    }
    QImage img;
    img.loadFromData(QByteArray::fromBase64(b64.toLatin1()));
    return QPixmap::fromImage(img).scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                         Qt::SmoothTransformation);
}

void QNChatMessage::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QFont f;
    f.setPointSize(m_fontSize);
    p.setFont(f);
    QFontMetrics fm(f);

    const int maxWidth = 260;
    const QRect br = fm.boundingRect(QRect(0, 0, maxWidth, 0),
                                     Qt::TextWordWrap, m_text);
    const int bubbleW = br.width()  + kBubblePad * 2;
    const int bubbleH = br.height() + kBubblePad * 2;
    const int vOff    = (m_size.height() - qMax(kAvatarSize, bubbleH)) / 2;

    QPixmap avatar = avatarFromBase64(m_headBase64, kAvatarSize);

    if (m_type == User_She) {
        // 头像左，气泡右
        p.drawPixmap(0, vOff, kAvatarSize, kAvatarSize, avatar);
        const QRectF bubble(kAvatarSize + 8, vOff, bubbleW, bubbleH);
        QPainterPath path;
        path.addRoundedRect(bubble, kBubbleRadius, kBubbleRadius);
        p.fillPath(path, QColor(255, 255, 255, 220));
        p.setPen(Qt::darkGray);
        p.drawText(bubble.adjusted(kBubblePad, kBubblePad, -kBubblePad, -kBubblePad)
                       .toRect(),
                   Qt::TextWordWrap, m_text);
    } else {
        // 气泡左，头像右
        const int ax = m_size.width() - kAvatarSize;
        p.drawPixmap(ax, vOff, kAvatarSize, kAvatarSize, avatar);
        const QRectF bubble(ax - 8 - bubbleW, vOff, bubbleW, bubbleH);
        QPainterPath path;
        path.addRoundedRect(bubble, kBubbleRadius, kBubbleRadius);
        p.fillPath(path, QColor(149, 236, 105, 220));
        p.setPen(Qt::darkGray);
        p.drawText(bubble.adjusted(kBubblePad, kBubblePad, -kBubblePad, -kBubblePad)
                       .toRect(),
                   Qt::TextWordWrap, m_text);
    }
}
