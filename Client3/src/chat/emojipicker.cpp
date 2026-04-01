#include "emojipicker.h"

#include <QFile>
#include <QGridLayout>
#include <QListWidget>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>

EmojiPicker::EmojiPicker(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("选择表情");
    setFixedSize(420, 320);

    m_list = new QListWidget(this);
    m_list->setViewMode(QListWidget::IconMode);
    m_list->setResizeMode(QListWidget::Adjust);
    m_list->setGridSize(QSize(40, 40));
    m_list->setWordWrap(false);
    m_list->setIconSize(QSize(28, 28));

    // 从资源文件或内置列表加载 emoji
    QStringList emojis;
    QFile f(":/emojis.txt");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&f);
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            for (const QChar &c : line)
                if (!c.isSpace())
                    emojis.append(c);
        }
    } else {
        // 内置常用 emoji
        emojis = {"😀","😂","😍","😎","😭","😡","👍","👎","❤️","💔",
                  "🎉","🔥","💪","✨","🌟","💯","🙏","🤔","😊","😴"};
    }

    for (const QString &e : emojis) {
        auto *item = new QListWidgetItem(e, m_list);
        item->setTextAlignment(Qt::AlignCenter);
    }

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        emit chooseEmoji(item->text());
        hide();
    });

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_list);
}
