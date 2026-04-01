#include "messageshow.h"

#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

static constexpr int kShowInterval  =  2;   // ms per pixel while sliding in
static constexpr int kStayInterval  = 1000; // ms per tick while staying
static constexpr int kStayTicks     = 15;
static constexpr int kCloseInterval = 100;  // ms per fade step

MessageShow::MessageShow(QWidget *parent)
    : QDialog(parent)
    , m_showTimer(new QTimer(this))
    , m_stayTimer(new QTimer(this))
    , m_closeTimer(new QTimer(this))
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::SubWindow);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(320, 80);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *titleBar = new QWidget(this);
    titleBar->setFixedHeight(30);
    titleBar->setStyleSheet("background:rgb(10,10,10);");

    auto *titleRow = new QHBoxLayout(titleBar);
    titleRow->setContentsMargins(8, 0, 4, 0);

    m_titleLabel = new QLabel(titleBar);
    m_titleLabel->setStyleSheet("color:#fff; font:bold 13px 'Microsoft YaHei';");
    titleRow->addWidget(m_titleLabel, 1);

    auto *closeBtn = new QPushButton("×", titleBar);
    closeBtn->setFixedSize(24, 24);
    closeBtn->setStyleSheet("color:#fff; background:transparent; border:0; font:16px;");
    connect(closeBtn, &QPushButton::clicked, this, [this]{ emit sigClose(this); });
    titleRow->addWidget(closeBtn);

    root->addWidget(titleBar);

    m_msgLabel = new QLabel(this);
    m_msgLabel->setStyleSheet("background:rgb(50,50,50); color:#fff; font:13px 'Microsoft YaHei'; padding:6px;");
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setOpenExternalLinks(false);
    connect(m_msgLabel, &QLabel::linkActivated, this, &MessageShow::sigClickUrl);
    root->addWidget(m_msgLabel, 1);

    connect(m_showTimer,  &QTimer::timeout, this, &MessageShow::slotMsgMove);
    connect(m_stayTimer,  &QTimer::timeout, this, &MessageShow::slotMsgStay);
    connect(m_closeTimer, &QTimer::timeout, this, &MessageShow::slotMsgClose);
}

void MessageShow::setInfomation(const QString &title, const QString &msg)
{
    m_titleLabel->setText(title);
    m_msgLabel->setText(msg);
    m_msgLabel->setToolTip(msg);
}

void MessageShow::setInfomation(const QString &title, const QString &msg, const QString &url)
{
    m_titleLabel->setText(title);
    m_msgLabel->setText(QStringLiteral("<a style='color:#ccc;' href='%1'>%2</a>").arg(url, msg));
    m_msgLabel->setToolTip(msg);
}

void MessageShow::updatePosition()
{
    if (m_firstShow) {
        showMessage();
        m_firstShow = false;
    } else {
        m_endPoint.setY(m_endPoint.y() - height() - 2);
        m_currentY  -= height() + 2;
        move(m_endPoint.x(), m_currentY);
    }
}

void MessageShow::showMessage()
{
    m_showTimer->stop();
    m_stayTimer->stop();
    m_closeTimer->stop();
    setWindowOpacity(1.0);
    m_opacity = 1.0;

    const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
    m_endPoint.setX(avail.right() - width() - 4);
    m_endPoint.setY(avail.bottom() - height() - 4);
    m_currentY = avail.bottom();

    move(m_endPoint.x(), m_currentY);
    show();
    m_showTimer->start(kShowInterval);
}

void MessageShow::slotMsgMove()
{
    --m_currentY;
    move(m_endPoint.x(), m_currentY);
    if (m_currentY <= m_endPoint.y()) {
        m_showTimer->stop();
        if (!m_hovered)
            m_stayTimer->start(kStayInterval);
    }
}

void MessageShow::slotMsgStay()
{
    if (++m_stayCount >= kStayTicks) {
        m_stayTimer->stop();
        m_closeTimer->start(kCloseInterval);
    }
}

void MessageShow::slotMsgClose()
{
    m_opacity -= 0.2;
    if (m_opacity <= 0.0) {
        m_closeTimer->stop();
        emit sigClose(this);
    } else {
        setWindowOpacity(m_opacity);
    }
}

void MessageShow::enterEvent(QEnterEvent *)
{
    m_hovered = true;
    m_stayTimer->stop();
    m_closeTimer->stop();
    m_opacity = 1.0;
    setWindowOpacity(1.0);
}

void MessageShow::leaveEvent(QEvent *)
{
    m_hovered = false;
    m_stayCount = kStayTicks / 2;
    m_stayTimer->start(kStayInterval);
}
