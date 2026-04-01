#include "wordfilter.h"

#include <QFile>
#include <QTextStream>

WordFilter::WordFilter(QObject *parent) : QObject(parent)
{
    // 从资源文件加载敏感词（若存在）
    QFile f(":/SensitiveWords.txt");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&f);
        while (!stream.atEnd()) {
            const QString word = stream.readLine().trimmed();
            if (!word.isEmpty())
                m_keywords.append(word);
        }
    }
}

QString WordFilter::filter(const QString &text) const
{
    QString result = text;
    for (const QString &word : m_keywords)
        result.replace(word, "真棒", Qt::CaseInsensitive);
    return result;
}
