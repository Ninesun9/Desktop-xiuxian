#include "rankwindow.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QPushButton>
#include <QVBoxLayout>

RankWindow::RankWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("排行榜");
    setFixedSize(480, 520);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    // 按钮栏
    auto *btnRow = new QHBoxLayout;
    auto *btnXiuwei   = new QPushButton("修为榜",  this);
    auto *btnShengming= new QPushButton("声名榜",  this);
    auto *btnGongji   = new QPushButton("攻击榜",  this);
    auto *btnFangyu   = new QPushButton("防御榜",  this);
    btnRow->addWidget(btnXiuwei);
    btnRow->addWidget(btnShengming);
    btnRow->addWidget(btnGongji);
    btnRow->addWidget(btnFangyu);
    root->addLayout(btnRow);

    m_browser = new QTextBrowser(this);
    root->addWidget(m_browser, 1);

    connect(btnXiuwei,    &QPushButton::clicked, this, &RankWindow::fetchXiuwei);
    connect(btnShengming, &QPushButton::clicked, this, &RankWindow::fetchShengming);
    connect(btnGongji,    &QPushButton::clicked, this, &RankWindow::fetchGongji);
    connect(btnFangyu,    &QPushButton::clicked, this, &RankWindow::fetchFangyu);
    connect(&m_nam, &QNetworkAccessManager::finished, this, &RankWindow::onReply);

    fetchXiuwei(); // 默认加载修为榜
}

void RankWindow::setAccessToken(const QString &token)
{
    m_token = token;
}

void RankWindow::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

// ── Fetch helpers ─────────────────────────────────────────────────────────────

void RankWindow::fetchXiuwei()
{
    // 优先使用新 API，无 token 时 fallback 旧服务器
    if (!m_token.isEmpty())
        fetchNewApi("/api/v1/pet/rankings");
    else
        fetchLegacy("http://yinciyunji.com:8522/rank/xiuwei");
}

void RankWindow::fetchShengming()
{
    fetchLegacy("http://yinciyunji.com:8522/rank/shengming");
}

void RankWindow::fetchGongji()
{
    fetchLegacy("http://yinciyunji.com:8522/rank/gongji");
}

void RankWindow::fetchFangyu()
{
    fetchLegacy("http://yinciyunji.com:8522/rank/fangyu");
}

void RankWindow::fetchLegacy(const QString &url)
{
    m_nam.get(QNetworkRequest(QUrl(url)));
}

void RankWindow::fetchNewApi(const QString &path)
{
    QNetworkRequest req(QUrl("http://127.0.0.1:3000" + path));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    m_nam.get(req);
}

// ── Response ──────────────────────────────────────────────────────────────────

void RankWindow::onReply(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        m_browser->setHtml("<p style='color:red;'>网络错误：" + reply->errorString() + "</p>");
        return;
    }

    const QByteArray data = reply->readAll();

    // 尝试解析新 API 的 JSON
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject() && doc.object().contains("items")) {
        QString html = "<table width='100%' cellpadding='6'>"
                       "<tr><th>#</th><th>名字</th><th>境界</th><th>修为</th></tr>";
        const QJsonArray items = doc.object()["items"].toArray();
        int rank = 1;
        for (const QJsonValue &v : items) {
            const QJsonObject obj = v.toObject();
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>")
                        .arg(rank++)
                        .arg(obj["userName"].toString().toHtmlEscaped())
                        .arg(obj["jingjie"].toInt())
                        .arg(obj["xiuwei"].toDouble(), 0, 'e', 2);
        }
        html += "</table>";
        m_browser->setHtml(html);
    } else {
        // 旧服务器返回的是直接 HTML
        m_browser->setHtml(QString::fromUtf8(data));
    }
}
