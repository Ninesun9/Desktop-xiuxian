#pragma once

#include <QMenu>
#include <QTimer>
#include <QWidget>

#include "core/playerprofile.h"
#include "network/apiclient.h"

// 前向声明（避免头文件循环）
class QLabel;
class QMovie;
class AugurWindow;
class ChatWindow;
class FeedBack;
class JiangHu;
class MailDrawer;
class RankWindow;
class UpdateChecker;

class PetWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PetWindow(QWidget *parent = nullptr);
    ~PetWindow() override = default;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onTick1s();
    void onTick10s();
    void onTick60s();

    void onLoginSucceeded(const QString &token, const QString &userId);
    void onLoginFailed(const QString &error);
    void onPetStateFetched(const QJsonObject &state);

    // 菜单动作
    void showStatusInfo();
    void toggleAlwaysOnTop(bool checked);
    void switchSkin();
    void showRankings();
    void toggleMailDrawer();
    void showChat();
    void showJiangHu();
    void showAugur();
    void showFeedback();
    void confirmDeleteData();
    void toggleAutoStart(bool checked);
    void changePlayerName();

private:
    void setupWindow();
    void setupMenu();
    void setupTimers();
    void initPlayer();
    void registerNewPlayer();
    void saveProfile();
    void repositionMailDrawer();
    bool isAutoStartEnabled() const;

    // ── Player ───────────────────────────────────────────────────────────
    PlayerProfile m_profile;

    // ── Pet display ──────────────────────────────────────────────────────
    QLabel  *m_petLabel  = nullptr;
    QMovie  *m_movie     = nullptr;
    QString  m_skinPath;

    // ── Context menu ─────────────────────────────────────────────────────
    QMenu    m_menu;
    QAction  m_actAlwaysOnTop;
    QAction  m_actStatus;
    QAction  m_actRankings;
    QAction  m_actMail;
    QAction  m_actChat;
    QAction  m_actJiangHu;
    QAction  m_actAugur;
    QAction  m_actFeedback;
    QAction  m_actSwitchSkin;
    QAction  m_actAutoStart;
    QAction  m_actChangeName;
    QAction  m_actDeleteData;
    QAction  m_actExit;

    // ── Drag ─────────────────────────────────────────────────────────────
    QPoint   m_dragOffset;
    bool     m_dragging = false;

    // ── Timers ───────────────────────────────────────────────────────────
    QTimer   m_timer1s;
    QTimer   m_timer10s;
    QTimer   m_timer60s;

    // ── Network ──────────────────────────────────────────────────────────
    ApiClient m_api;
    bool      m_loggedIn = false;

    // ── Sub-windows (lazy created) ────────────────────────────────────────
    MailDrawer    *m_mailDrawer    = nullptr;
    ChatWindow    *m_chatWindow    = nullptr;
    JiangHu       *m_jianghu      = nullptr;
    AugurWindow   *m_augurWindow   = nullptr;
    FeedBack      *m_feedback      = nullptr;
    RankWindow    *m_rankWindow    = nullptr;
    UpdateChecker *m_updateChecker = nullptr;
};
