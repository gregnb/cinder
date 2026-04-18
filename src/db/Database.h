#ifndef DATABASE_H
#define DATABASE_H

#include <QList>
#include <QSqlDatabase>
#include <QString>

class Database {
  public:
    static bool open();
    static bool open(const QString& dbPath, const QString& resourceDir);
    static void close();
    static QSqlDatabase connection();

    // Exposed for testing — run migrations from a custom resource dir
    static bool migrate(const QString& resourceDir = ":/migrations");
    static int currentVersion();

    // Force WAL checkpoint so cross-process writes become visible to this connection.
    // Call before polling queries that read data written by the MCP subprocess.
    static void checkpoint();

  private:
    struct Migration {
        int version;
        QString path;
        QString name;
    };

    static void setVersion(int version);
    static QList<Migration> discoverMigrations(const QString& resourceDir);
    static bool runMigration(const Migration& m);
};

#endif // DATABASE_H
