#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;

// 表情选择器 —— 从资源文件 :/emojis.txt 加载 emoji 列表
class EmojiPicker : public QDialog
{
    Q_OBJECT
public:
    explicit EmojiPicker(QWidget *parent = nullptr);

signals:
    void chooseEmoji(const QString &emoji);

private:
    QListWidget *m_list = nullptr;
};
