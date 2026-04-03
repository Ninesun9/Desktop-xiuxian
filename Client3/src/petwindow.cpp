#include "petwindow.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QSettings>
#include <QVBoxLayout>

#include "augur/augurwindow.h"
#include "chat/chatwindow.h"
#include "core/gamelogic.h"
#include "core/playerprofilestore.h"
#include "jianghu/jianghu.h"
#include "jianghu/jianghuglobal.h"
#include "mail/maildrawer.h"
#include "rank/rankwindow.h"
#include "update/feedback.h"
#include "update/updatechecker.h"
#include "util/popupmanager.h"

// ── Construction ──────────────────────────────────────────────────────────────

PetWindow::PetWindow(QWidget *parent)
    : QWidget(parent)
{
    setupWindow();
    setupMenu();
    setupTimers();

    m_api.setBaseUrl("http://107.174.220.99:3000");
    connect(&m_api, &ApiClient::loginSucceeded, this, &PetWindow::onLoginSucceeded);
    connect(&m_api, &ApiClient::loginFailed,    this, &PetWindow::onLoginFailed);
    connect(&m_api, &ApiClient::petStateFetched,this, &PetWindow::onPetStateFetched);
    connect(&m_api, &ApiClient::requestError, this, [](const QString &ctx, const QString &err) {
        qWarning() << "[Api]" << ctx << err;
    });

    // 初始化江湖全局单例
    jg = new JiangHuGlobal(this);

    initPlayer();

    // 启动更新检查（静默）
    m_updateChecker = new UpdateChecker(this);
}

// ── Window setup ──────────────────────────────────────────────────────────────

void PetWindow::setupWindow()
{
    setWindowFlags(Qt::FramelessWindowHint
                 | Qt::Tool
                 | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    m_petLabel = new QLabel(this);
    m_petLabel->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_petLabel);

    resize(120, 120);
}

void PetWindow::setupMenu()
{
    m_actAlwaysOnTop.setText("置顶");
    m_actAlwaysOnTop.setCheckable(true);
    m_actAlwaysOnTop.setChecked(true);

    m_actStatus.setText("修炼状态");
    m_actRankings.setText("排行榜");
    m_actMail.setText("Mail");
    m_actChat.setText("聊天室");
    m_actJiangHu.setText("江湖");
    m_actAugur.setText("占卜");
    m_actFeedback.setText("意见反馈");
    m_actSwitchSkin.setText("变身");

    m_actAutoStart.setText("开机自启");
    m_actAutoStart.setCheckable(true);
    m_actAutoStart.setChecked(isAutoStartEnabled());

    m_actChangeName.setText("改名");
    m_actDeleteData.setText("自废修为");
    m_actExit.setText("结束修炼");

    m_menu.addAction(&m_actAlwaysOnTop);
    m_menu.addAction(&m_actStatus);
    m_menu.addSeparator();
    m_menu.addAction(&m_actMail);
    m_menu.addAction(&m_actChat);
    m_menu.addAction(&m_actRankings);
    m_menu.addAction(&m_actJiangHu);
    m_menu.addAction(&m_actAugur);
    m_menu.addAction(&m_actFeedback);
    m_menu.addSeparator();
    m_menu.addAction(&m_actSwitchSkin);
    m_menu.addAction(&m_actAutoStart);
    m_menu.addAction(&m_actChangeName);
    m_menu.addAction(&m_actDeleteData);
    m_menu.addSeparator();
    m_menu.addAction(&m_actExit);

    connect(&m_actAlwaysOnTop, &QAction::toggled,   this, &PetWindow::toggleAlwaysOnTop);
    connect(&m_actStatus,      &QAction::triggered, this, &PetWindow::showStatusInfo);
    connect(&m_actRankings,    &QAction::triggered, this, &PetWindow::showRankings);
    connect(&m_actMail,        &QAction::triggered, this, &PetWindow::toggleMailDrawer);
    connect(&m_actChat,        &QAction::triggered, this, &PetWindow::showChat);
    connect(&m_actJiangHu,     &QAction::triggered, this, &PetWindow::showJiangHu);
    connect(&m_actAugur,       &QAction::triggered, this, &PetWindow::showAugur);
    connect(&m_actFeedback,    &QAction::triggered, this, &PetWindow::showFeedback);
    connect(&m_actSwitchSkin,  &QAction::triggered, this, &PetWindow::switchSkin);
    connect(&m_actAutoStart,   &QAction::toggled,   this, &PetWindow::toggleAutoStart);
    connect(&m_actChangeName,  &QAction::triggered, this, &PetWindow::changePlayerName);
    connect(&m_actDeleteData,  &QAction::triggered, this, &PetWindow::confirmDeleteData);
    connect(&m_actExit,        &QAction::triggered, qApp, &QApplication::quit);
}

void PetWindow::setupTimers()
{
    connect(&m_timer1s,  &QTimer::timeout, this, &PetWindow::onTick1s);
    connect(&m_timer10s, &QTimer::timeout, this, &PetWindow::onTick10s);
    connect(&m_timer60s, &QTimer::timeout, this, &PetWindow::onTick60s);
    m_timer1s.start(1000);
    m_timer10s.start(10000);
    m_timer60s.start(60000);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void PetWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 0));
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void PetWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragOffset = frameGeometry().topLeft() - event->globalPosition().toPoint();
        m_dragging   = true;
    }
}

void PetWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
    repositionMailDrawer();
    event->accept();
}

void PetWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        move(event->globalPosition().toPoint() + m_dragOffset);
        repositionMailDrawer();
    }
}

void PetWindow::contextMenuEvent(QContextMenuEvent *event)
{
    m_menu.exec(event->globalPos());
    event->accept();
}

// ── Player init ───────────────────────────────────────────────────────────────

void PetWindow::initPlayer()
{
    if (!PlayerProfileStore::load(&m_profile) ||
        !PlayerProfileStore::hasValidUserId(m_profile.userId)) {
        registerNewPlayer();
        return;
    }

    m_api.deviceLogin(m_profile.userId, m_profile.userName);

    if (jg) {
        jg->uName = m_profile.userName;
        jg->uuid  = m_profile.userId;
    }

    m_skinPath = ":/pet.gif";
    m_movie    = new QMovie(m_skinPath, QByteArray(), this);
    m_petLabel->setMovie(m_movie);
    m_movie->start();
}

void PetWindow::registerNewPlayer()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        nullptr, "起名字", "请输入你的名字：",
        QLineEdit::Normal, QString(), &ok);

    if (!ok || name.trimmed().isEmpty()) { QApplication::quit(); return; }

    m_profile = PlayerProfileStore::createNew(name.trimmed());
    saveProfile();
    initPlayer();
}

void PetWindow::saveProfile()
{
    PlayerProfileStore::save(m_profile);
}

void PetWindow::repositionMailDrawer()
{
    if (!m_mailDrawer || !m_mailDrawer->isVisible()) return;
    const QPoint topRight = mapToGlobal(rect().topRight());
    m_mailDrawer->move(topRight.x() + 12, topRight.y() + 16);
}

// ── Ticks ─────────────────────────────────────────────────────────────────────

void PetWindow::onTick1s()
{
    GameLogic::tickXiuwei(m_profile);
    GameLogic::tickLingshi(m_profile);
}

void PetWindow::onTick10s()
{
    const bool leveled = GameLogic::tryLevelUp(m_profile);
    saveProfile();
    if (leveled) {
        PopupManager::instance()->show("境界突破",
            "恭喜晋升至：" + m_profile.jieDuanName());
    }
    if (m_loggedIn) {
        QJsonObject patch;
        patch["statusText"] = m_profile.jieDuanName()
            + " · 灵石 " + QString::number(static_cast<qint64>(m_profile.lingshi));
        patch["jingjie"] = m_profile.jingjie;
        patch["xiuwei"]  = m_profile.xiuwei;
        patch["lingshi"] = m_profile.lingshi;
        m_api.patchPetState(patch);
    }
}

void PetWindow::onTick60s()
{
    if (m_loggedIn)
        m_api.fetchPetState();
}

// ── API callbacks ─────────────────────────────────────────────────────────────

void PetWindow::onLoginSucceeded(const QString &token, const QString &userId)
{
    Q_UNUSED(userId)
    m_loggedIn = true;
    if (m_rankWindow)    m_rankWindow->setAccessToken(token);
    if (m_feedback)      m_feedback->setAccessToken(token);
    m_api.fetchPetState();
}

void PetWindow::onLoginFailed(const QString &error)
{
    qWarning() << "[Login]" << error;
}

void PetWindow::onPetStateFetched(const QJsonObject &)
{
    // 可用服务端 mood/level 覆盖本地展示（按需实现）
}

// ── Menu actions ──────────────────────────────────────────────────────────────

void PetWindow::showStatusInfo()
{
    QMessageBox::information(this, "修炼状态",
        m_profile.userName + "\n"
        "灵石：" + QString::number(static_cast<qint64>(m_profile.lingshi)) + " 枚\n"
        "境界：" + m_profile.jieDuanName() + "\n"
        "修为：" + QString::number(m_profile.xiuwei,    'e', 3) + "\n"
        "生命：" + QString::number(m_profile.shengming, 'f', 1) + "\n"
        "攻击：" + QString::number(m_profile.gongji,    'f', 1) + "\n"
        "防御：" + QString::number(m_profile.fangyu,    'f', 1) + "\n"
        "悟性：" + QString::number(m_profile.wuxing,    'f', 4));
}

void PetWindow::toggleAlwaysOnTop(bool checked)
{
    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::Tool;
    if (checked) flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    show();
}

void PetWindow::switchSkin()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "选择皮肤 GIF", QString(), "GIF 动图 (*.gif)");
    if (path.isEmpty()) return;

    m_skinPath = path;
    if (m_movie) { m_movie->stop(); m_movie->deleteLater(); }
    m_movie = new QMovie(m_skinPath, QByteArray(), this);
    m_petLabel->setMovie(m_movie);
    m_movie->start();
}

void PetWindow::showRankings()
{
    if (!m_rankWindow) m_rankWindow = new RankWindow(this);
    m_rankWindow->show();
    m_rankWindow->raise();
}

void PetWindow::toggleMailDrawer()
{
    if (!m_mailDrawer) {
        m_mailDrawer = new MailDrawer(this);
        m_mailDrawer->setPlayerInfo(m_profile.userName, m_profile.userId);
    }

    if (m_mailDrawer->isVisible()) {
        m_mailDrawer->hide();
        return;
    }

    repositionMailDrawer();
    m_mailDrawer->show();
    m_mailDrawer->raise();
}

void PetWindow::showChat()
{
    if (!m_chatWindow) {
        m_chatWindow = new ChatWindow(this);
        m_chatWindow->setInfo(m_profile.userName, m_profile.userId);
    }
    m_chatWindow->show();
    m_chatWindow->raise();
}

void PetWindow::showJiangHu()
{
    if (!m_jianghu) {
        m_jianghu = new JiangHu(this);
        m_jianghu->setInfo(m_profile.userName, m_profile.userId);
    }
    m_jianghu->show();
    m_jianghu->raise();
}

void PetWindow::showAugur()
{
    if (!m_augurWindow) m_augurWindow = new AugurWindow(this);
    m_augurWindow->show();
    m_augurWindow->raise();
}

void PetWindow::showFeedback()
{
    if (!m_feedback) {
        m_feedback = new FeedBack(this);
        m_feedback->setBaseUrl("http://107.174.220.99:3000");
    }
    m_feedback->show();
    m_feedback->raise();
}

void PetWindow::confirmDeleteData()
{
    if (QMessageBox::question(this, "自废修为", "这将删除全部存档，确定吗？")
        != QMessageBox::Yes) return;
    if (QMessageBox::question(this, "再次确认", "真的真的要删档吗？")
        != QMessageBox::Yes) return;
    QFile::remove(PlayerProfileStore::configFilePath());
    QApplication::quit();
}

void PetWindow::toggleAutoStart(bool checked)
{
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);
    const QString key = "XiuxianPet";
    if (checked)
        reg.setValue(key, QApplication::applicationFilePath().replace('/', '\\'));
    else
        reg.remove(key);
}

bool PetWindow::isAutoStartEnabled() const
{
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);
    return reg.contains("XiuxianPet");
}

void PetWindow::changePlayerName()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, "改名", "新名字：",
        QLineEdit::Normal, m_profile.userName, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    m_profile.userName = name.trimmed();
    saveProfile();
    if (jg) jg->uName = m_profile.userName;
}
