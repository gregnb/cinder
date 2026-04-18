#ifndef NOTIFICATIONDB_H
#define NOTIFICATIONDB_H

#include <QList>
#include <QString>

struct NotificationRecord {
    int id = 0;
    QString type;
    QString title;
    QString body;
    QString signature;
    QString token;
    QString amount;
    QString fromAddr;
    bool isRead = false;
    qint64 createdAt = 0;
};

class QSqlDatabase;

class NotificationDb {
  public:
    static bool insertNotification(const QString& type, const QString& title, const QString& body,
                                   const QString& signature = {}, const QString& token = {},
                                   const QString& amount = {}, const QString& fromAddr = {});

    static QList<NotificationRecord> getRecentRecords(int limit = 50);

    static int countUnread();

    static bool markRead(int id);

    static bool markAllRead();

    static bool existsForSignature(const QString& signature);

  private:
    static QSqlDatabase db();
};

#endif // NOTIFICATIONDB_H
