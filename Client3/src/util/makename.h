#pragma once
#include <QString>

class MakeName
{
public:
    static QString getName();
    static qreal   getWuxing(int jieduan);
    static qreal   getRandData(qreal data, qreal fanwei = 0.9);
};
