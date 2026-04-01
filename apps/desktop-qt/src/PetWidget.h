#ifndef PETWIDGET_H
#define PETWIDGET_H

#include <QWidget>
#include <QMenu>
#include <QPixmap>
#include <QPoint>
#include <QTimer>
#include <QMap>
#include <QVector>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSettings>
#include <QUuid>

enum class PetState {
    Idle,
    Attentive,
    Resting,
    Speaking,
    Walking,
    Lurking
};

class PetWidget : public QWidget {
    Q_OBJECT

public:
    explicit PetWidget(QWidget *parent = nullptr);
    ~PetWidget();

    void setPetState(PetState state);
    void showBubble(const QString &text, int durationMs = 3000);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void openConsole();
    void toggleTopmost();
    void quitApp();
    void updateAnimation();
    void hideBubble();
    
    // Network Slots
    void loginToBackend();
    void onLoginFinished(QNetworkReply *reply);
    void fetchPetState();
    void onPetStateFinished(QNetworkReply *reply);
    void showRankings();
    void onRankingsFinished(QNetworkReply *reply);
    void resetData();
    
    // Client Slots
    void switchSkin();
    void toggleAutoStart(bool checked);

private:
    void setupMenu();
    void initAnimations();
    bool isAutoStartEnabled();

    // Animation state
    QTimer *m_animTimer;
    PetState m_currentState;
    int m_currentFrameIdx;
    QMap<PetState, QVector<QPixmap>> m_animations;
    QMap<PetState, int> m_animIntervals;
    QPixmap m_customSkin;
    
    // Bubble state
    QString m_bubbleText;
    QTimer *m_bubbleTimer;

    // Window logic
    QPoint m_dragPosition;
    bool m_isDragging = false;
    bool m_isTopmost = true;

    QMenu *m_contextMenu;

    // Network & State
    QNetworkAccessManager *m_networkManager;
    QTimer *m_pollTimer;
    QString m_apiBaseUrl;
    QString m_deviceId;
    QString m_accessToken;
    
    // Pet Stats
    double m_energy = 100.0;
    double m_intimacy = 100.0;
    QString m_mood = "开心";
    int m_jingjie = 0;
    double m_xiuwei = 0.0;
};

#endif // PETWIDGET_H
