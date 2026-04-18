#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <QDateTime>
#include <QObject>
#include <QString>

inline QString formatRelativeTime(qint64 blockTime) {
    QDateTime txTime = QDateTime::fromSecsSinceEpoch(blockTime);
    QDateTime now = QDateTime::currentDateTime();
    qint64 secsAgo = txTime.secsTo(now);

    if (secsAgo < 0) {
        return QObject::tr("Just now");
    }

    if (secsAgo < 60) {
        return QObject::tr("Just now");
    } else if (secsAgo < 3600) {
        int mins = static_cast<int>(secsAgo / 60);
        return QObject::tr("%1 min ago").arg(mins);
    } else if (secsAgo < 86400) {
        int hours = static_cast<int>(secsAgo / 3600);
        if (hours == 1) {
            return QObject::tr("1 hour ago");
        }
        return QObject::tr("%1 hours ago").arg(hours);
    }

    int days = static_cast<int>(secsAgo / 86400);
    if (days == 1) {
        return QObject::tr("Yesterday");
    } else if (days < 7) {
        return QObject::tr("%1 days ago").arg(days);
    } else if (days < 30) {
        int weeks = days / 7;
        return (weeks == 1) ? QObject::tr("1 week ago") : QObject::tr("%1 weeks ago").arg(weeks);
    }

    return txTime.toString("MMM d, yyyy");
}

#endif // TIMEUTILS_H
