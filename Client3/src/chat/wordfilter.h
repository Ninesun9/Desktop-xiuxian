#pragma once
#include <QObject>
#include <QStringList>

class WordFilter : public QObject
{
    Q_OBJECT
public:
    explicit WordFilter(QObject *parent = nullptr);
    QString filter(const QString &text) const;

private:
    QStringList m_keywords;
};
