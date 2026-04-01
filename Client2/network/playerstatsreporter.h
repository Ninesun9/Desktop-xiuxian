#ifndef PLAYERSTATSREPORTER_H
#define PLAYERSTATSREPORTER_H

#include <QString>

struct PlayerStatsSnapshot
{
    QString userId;
    QString userName;
    double shengming = 0;
    double gongji = 0;
    double fangyu = 0;
    QString wuxing;
    int jingjie = 0;
    QString xiuwei;
};

class PlayerStatsReporter
{
public:
    static bool sendUpdate(const PlayerStatsSnapshot &snapshot, QString *errorMessage = nullptr);
};

#endif // PLAYERSTATSREPORTER_H
