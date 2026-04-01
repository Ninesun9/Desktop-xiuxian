#include "PetWidget.h"
#include <QMouseEvent>
#include <QPainter>
#include <QDebug>
#include <QApplication>
#include <QAction>
#include <QDesktopServices>
#include <QUrl>
#include <QPainterPath>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QUuid>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

PetWidget::PetWidget(QWidget *parent) : QWidget(parent), m_currentState(PetState::Idle), m_currentFrameIdx(0) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    m_animTimer = new QTimer(this);
    connect(m_animTimer, &QTimer::timeout, this, &PetWidget::updateAnimation);
    
    m_bubbleTimer = new QTimer(this);
    m_bubbleTimer->setSingleShot(true);
    connect(m_bubbleTimer, &QTimer::timeout, this, &PetWidget::hideBubble);

    initAnimations();

    // 扩大窗口尺寸以容纳可能出现的气泡 (给气泡留出左侧和上方的空间)
    setFixedSize(400, 500);
    setupMenu();

#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    DWMNCRENDERINGPOLICY ncrp = DWMNCRP_DISABLED;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(ncrp));
    int cornerPref = 1; 
    DwmSetWindowAttribute(hwnd, 33, &cornerPref, sizeof(cornerPref));
    COLORREF borderColor = 0xFFFFFFFE;
    DwmSetWindowAttribute(hwnd, 34, &borderColor, sizeof(borderColor));
    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
#endif

    setPetState(PetState::Idle);
    
    // 测试：启动直接展示一句话
    showBubble("主人，你终于把我唤醒啦！");
    
    // 初始化网络请求
    m_networkManager = new QNetworkAccessManager(this);
    m_pollTimer = new QTimer(this);
    m_apiBaseUrl = "http://107.174.220.99:3000";
    
    QSettings settings("XiuxianDesktop", "DesktopCompanion");
    m_deviceId = settings.value("deviceId", "").toString();
    if (m_deviceId.isEmpty()) {
        m_deviceId = QUuid::createUuid().toString();
        settings.setValue("deviceId", m_deviceId);
    }
    
    QString savedSkin = settings.value("customSkin", "").toString();
    if (!savedSkin.isEmpty()) {
        QPixmap loaded(savedSkin);
        if (!loaded.isNull()) {
            m_customSkin = loaded.scaled(276, 428, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    
    connect(m_pollTimer, &QTimer::timeout, this, &PetWidget::fetchPetState);
    loginToBackend();
}

PetWidget::~PetWidget() {}

void PetWidget::initAnimations() {
    auto loadAnimation = [](const QVector<int>& frameNumbers) {
        QVector<QPixmap> frames;
        for (int num : frameNumbers) {
            QString path = QString("D:/code/xiuxian/皮肤/ST-%1.png").arg(num, 2, 10, QChar('0'));
            // Remove prefixed 0 for numbers >= 10 if necessary based on actual files. 
            // In original JS: ST-01 to ST-09, then ST-010 or ST-10? Wait, JS says: ST-010.png, let's just use sprintf logic:
            if(num >= 10) path = QString("D:/code/xiuxian/皮肤/ST-0%1.png").arg(num);
            
            QPixmap pm(path);
            if (!pm.isNull()) frames.push_back(pm.scaled(276, 428, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            else qWarning() << "Missing frame: " << path;
        }
        return frames;
    };

    m_animations[PetState::Idle] = loadAnimation({1,2,3,4,5,6,7,8,9,10,11,12});
    m_animIntervals[PetState::Idle] = 160;

    m_animations[PetState::Attentive] = loadAnimation({43,44,45,46,47,48,49,50,51});
    m_animIntervals[PetState::Attentive] = 110;

    m_animations[PetState::Resting] = loadAnimation({10,11,12,11});
    m_animIntervals[PetState::Resting] = 320;

    m_animations[PetState::Speaking] = loadAnimation({43,45,47,49,51,49,47,45});
    m_animIntervals[PetState::Speaking] = 90;

    m_animations[PetState::Walking] = loadAnimation({1,3,5,7,9,7,5,3});
    m_animIntervals[PetState::Walking] = 120;

    m_animations[PetState::Lurking] = loadAnimation({12,11,10,11});
    m_animIntervals[PetState::Lurking] = 420;
}

void PetWidget::setPetState(PetState state) {
    if (!m_animations.contains(state) || m_animations[state].isEmpty()) return;
    
    m_currentState = state;
    m_currentFrameIdx = 0;
    m_animTimer->start(m_animIntervals[state]);
    update();
}

void PetWidget::updateAnimation() {
    if (m_animations[m_currentState].isEmpty()) return;
    m_currentFrameIdx = (m_currentFrameIdx + 1) % m_animations[m_currentState].size();
    update();
}

void PetWidget::showBubble(const QString &text, int durationMs) {
    m_bubbleText = text;
    if (durationMs > 0) m_bubbleTimer->start(durationMs);
    update();
}

void PetWidget::hideBubble() {
    m_bubbleText.clear();
    update();
}

void PetWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 将桌宠画在右下角偏中的位置，尺寸276x428，整个窗口400x500
    int petX = width() - 276 - 10;
    int petY = height() - 428;

    if (!m_customSkin.isNull()) {
        painter.drawPixmap(petX, petY, m_customSkin);
    } else if (!m_animations[m_currentState].isEmpty()) {
        painter.drawPixmap(petX, petY, m_animations[m_currentState][m_currentFrameIdx]);
    }

    // 画文字气泡 (如果有)
    if (!m_bubbleText.isEmpty()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 220)); // 半透明白底
        
        QRect bubbleRect(20, petY + 50, 200, 80); // 气泡位置
        // 画一个带小圆角的矩形
        QPainterPath path;
        path.addRoundedRect(bubbleRect, 10, 10);
        
        // 画出指向桌宠的小箭头
        QPainterPath tail;
        tail.moveTo(220, petY + 80);
        tail.lineTo(petX + 20, petY + 100);
        tail.lineTo(220, petY + 110);
        path.addPath(tail);

        painter.drawPath(path);

        // 写字
        painter.setPen(QColor(50, 50, 50));
        QFont font = painter.font();
        font.setPointSize(10);
        font.setBold(true);
        painter.setFont(font);
        
        painter.drawText(bubbleRect.adjusted(10, 10, -10, -10), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_bubbleText);
    }
}

void PetWidget::setupMenu() {
    m_contextMenu = new QMenu(this);

    QAction *statusAction = new QAction("修仙状态 (同步云端)", this);
    connect(statusAction, &QAction::triggered, this, [this](){
        QString info = QString("灵识状态: %1\n灵力值/饱食度: %2\n亲密度: %3\n境界层数: %4\n当前修为: %5")
                       .arg(m_mood)
                       .arg(m_energy)
                       .arg(m_intimacy)
                       .arg(m_jingjie)
                       .arg(m_xiuwei);
        QMessageBox::information(this, "云端修炼状态", info);
    });
    m_contextMenu->addAction(statusAction);

    QAction *consoleAction = new QAction("打开飞书终端", this);
    connect(consoleAction, &QAction::triggered, this, &PetWidget::openConsole);
    m_contextMenu->addAction(consoleAction);

    m_contextMenu->addSeparator();

    QAction *rankAction = new QAction("风云榜 (排行榜)", this);
    connect(rankAction, &QAction::triggered, this, &PetWidget::showRankings);
    m_contextMenu->addAction(rankAction);
    
    QAction *jianghuAction = new QAction("江湖 (开发中)", this);
    connect(jianghuAction, &QAction::triggered, this, [this](){ QMessageBox::information(this, "提示", "人在江湖飘，该功能敬请期待~"); });
    m_contextMenu->addAction(jianghuAction);

    QAction *augurAction = new QAction("卜卦", this);
    connect(augurAction, &QAction::triggered, this, [this](){ QMessageBox::information(this, "卜卦", "天机不可泄漏，该功能敬请期待~"); });
    m_contextMenu->addAction(augurAction);

    QAction *skinAction = new QAction("易容换形 (变身)", this);
    connect(skinAction, &QAction::triggered, this, &PetWidget::switchSkin);
    m_contextMenu->addAction(skinAction);

    m_contextMenu->addSeparator();
    
    QAction *autoStartAction = new QAction("伴随开机 (自启)", this);
    autoStartAction->setCheckable(true);
    autoStartAction->setChecked(isAutoStartEnabled());
    connect(autoStartAction, &QAction::triggered, this, &PetWidget::toggleAutoStart);
    m_contextMenu->addAction(autoStartAction);

    QAction *topmostAction = new QAction("取消置顶", this);
    connect(topmostAction, &QAction::triggered, this, &PetWidget::toggleTopmost);
    m_contextMenu->addAction(topmostAction);

    m_contextMenu->addSeparator();

    QAction *resetAction = new QAction("自废修为 (删档)", this);
    connect(resetAction, &QAction::triggered, this, &PetWidget::resetData);
    m_contextMenu->addAction(resetAction);

    QAction *quitAction = new QAction("结束修炼", this);
    connect(quitAction, &QAction::triggered, this, &PetWidget::quitApp);
    m_contextMenu->addAction(quitAction);
}

void PetWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void PetWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void PetWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
        showBubble("哎呀，被你点到了！", 2000);
        setPetState(PetState::Speaking);
        QTimer::singleShot(2000, this, [this](){ setPetState(PetState::Idle); });
    }
}

void PetWidget::contextMenuEvent(QContextMenuEvent *event) {
    m_contextMenu->actions().at(2)->setText(m_isTopmost ? "取消置顶" : "保持置顶");
    m_contextMenu->exec(event->globalPos());
}

void PetWidget::openConsole() {
    QDesktopServices::openUrl(QUrl("http://localhost:1420"));
}

void PetWidget::toggleTopmost() {
    m_isTopmost = !m_isTopmost;
    if (m_isTopmost) {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    } else {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    }
    show();
}

void PetWidget::quitApp() {
    QApplication::quit();
}

void PetWidget::loginToBackend() {
    QUrl url(m_apiBaseUrl + "/api/v1/auth/device-login");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json["deviceId"] = m_deviceId;
    json["deviceName"] = "desktop-qt-companion";
    json["platform"] = "windows";

    QJsonDocument doc(json);
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onLoginFinished(reply); });
}

void PetWidget::onLoginFinished(QNetworkReply *reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        QJsonObject obj = doc.object();
        m_accessToken = obj["accessToken"].toString();
        
        qDebug() << "Login successful. Access Token parsed.";
        fetchPetState(); // 先获取一次
        m_pollTimer->start(10000); // 然后每10秒拉取一次
    } else {
        qWarning() << "Login failed: " << reply->errorString();
    }
    reply->deleteLater();
}

void PetWidget::fetchPetState() {
    if (m_accessToken.isEmpty()) return;

    QUrl url(m_apiBaseUrl + "/api/v1/pet/state");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onPetStateFinished(reply); });
}

void PetWidget::onPetStateFinished(QNetworkReply *reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        QJsonObject obj = doc.object();
        
        m_energy = obj["energy"].toDouble();
        m_intimacy = obj["intimacy"].toDouble();
        m_mood = obj["mood"].toString();
        m_jingjie = obj["jingjie"].toInt();
        m_xiuwei = obj["xiuwei"].toDouble();

        // 动态调整桌宠表情和动画
        if (m_energy <= 25) {
            if (m_currentState != PetState::Resting) setPetState(PetState::Resting);
        } else if (m_energy >= 70) {
            if (m_currentState != PetState::Attentive && m_currentState != PetState::Speaking && m_currentState != PetState::Walking) {
                setPetState(PetState::Attentive);
            }
        } else {
            if (m_currentState == PetState::Resting || m_currentState == PetState::Attentive) {
                setPetState(PetState::Idle);
            }
        }
    } else {
        qWarning() << "Fetch pet state failed: " << reply->errorString();
    }
    reply->deleteLater();
    reply->deleteLater();
}

void PetWidget::showRankings() {
    QUrl url(m_apiBaseUrl + "/api/v1/pet/rankings");
    QNetworkRequest request(url);
    if (!m_accessToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    }
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onRankingsFinished(reply); });
}

void PetWidget::onRankingsFinished(QNetworkReply *reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        QJsonArray items = doc.object()["items"].toArray();
        
        QString text = "=== 天下群雄榜 ===\n\n";
        for (int i = 0; i < items.size(); ++i) {
            QJsonObject obj = items[i].toObject();
            text += QString("第 %1 名: 境界 %2 | 修为 %3 | [%4]\n")
                    .arg(i + 1)
                    .arg(obj["jingjie"].toInt())
                    .arg(obj["xiuwei"].toDouble())
                    .arg(obj["userId"].toString().left(8));
        }
        QMessageBox::information(this, "风云榜", text.isEmpty() ? "暂无修士上榜" : text);
    } else {
        QMessageBox::warning(this, "风云榜", "天机被蒙蔽了：" + reply->errorString());
    }
    reply->deleteLater();
}

void PetWidget::resetData() {
    auto reply1 = QMessageBox::question(this, "自废修为", "这将会删除云端的全部数据，确定操作吗？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply1 == QMessageBox::Yes) {
        auto reply2 = QMessageBox::question(this, "再次确认", "真的真的真的要删档重修吗？所有积攒的修为都将立刻灰飞烟灭！", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply2 == QMessageBox::Yes) {
            QUrl url(m_apiBaseUrl + "/api/v1/pet/state");
            QNetworkRequest request(url);
            request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
            
            QNetworkReply *reply = m_networkManager->sendCustomRequest(request, "DELETE");
            connect(reply, &QNetworkReply::finished, this, [this, reply]() {
                QSettings settings("XiuxianDesktop", "DesktopCompanion");
                settings.remove("deviceId"); // 强行遗忘自身身份
                QMessageBox::information(this, "轮回", "天道轮回，一切重新开始。再见。");
                QApplication::quit();
                reply->deleteLater();
            });
        }
    }
}

void PetWidget::switchSkin() {
    QString fileName = QFileDialog::getOpenFileName(this, "选择变身图片", QDir::homePath(), "Images (*.png *.jpg *.bmp)");
    if (!fileName.isEmpty()) {
        QPixmap loaded(fileName);
        if (!loaded.isNull()) {
            m_customSkin = loaded.scaled(276, 428, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QSettings settings("XiuxianDesktop", "DesktopCompanion");
            settings.setValue("customSkin", fileName);
            m_animTimer->stop(); // 如果有自定义图片，暂停自带帧动画
            update();
        } else {
            QMessageBox::warning(this, "变身失败", "这件衣服好像不合身(图片无效)。");
        }
    } else {
        // 取消变身，恢复原状
        m_customSkin = QPixmap();
        QSettings settings("XiuxianDesktop", "DesktopCompanion");
        settings.remove("customSkin");
        if (m_animations.contains(m_currentState)) {
            m_animTimer->start(m_animIntervals[m_currentState]);
        }
        update();
    }
}

bool PetWidget::isAutoStartEnabled() {
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return settings.contains("XiuxianDesktopCompanion");
}

void PetWidget::toggleAutoStart(bool checked) {
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (checked) {
        QString appPath = QDir::toNativeSeparators(QApplication::applicationFilePath());
        settings.setValue("XiuxianDesktopCompanion", "\"" + appPath + "\"");
    } else {
        settings.remove("XiuxianDesktopCompanion");
    }
}
