#include "Database.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

static void setOwnerOnlyPermissions(const QString& path, bool isDir = false) {
    QFileDevice::Permissions perms = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    if (isDir) {
        perms |= QFileDevice::ExeOwner;
    }
    QFile::setPermissions(path, perms);
}

bool Database::open() {
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    setOwnerOnlyPermissions(dataDir, true);

    const QString dbPath = dataDir + "/wallet.db";
    return open(dbPath, QStringLiteral(":/migrations"));
}

bool Database::open(const QString& dbPath, const QString& resourceDir) {
    const QString dataDir = QFileInfo(dbPath).absolutePath();
    QDir().mkpath(dataDir);
    setOwnerOnlyPermissions(dataDir, true);

#if defined(QT_DEBUG)
    qDebug() << "[DB] Opening database at:" << dbPath;
#endif

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "[DB] Failed to open:" << db.lastError().text();
        return false;
    }

    QSqlQuery pragma(db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA foreign_keys=ON");

    // Best-effort local hardening of database files.
    setOwnerOnlyPermissions(dbPath);
    setOwnerOnlyPermissions(dbPath + "-wal");
    setOwnerOnlyPermissions(dbPath + "-shm");

    qDebug() << "[DB] Connection open";
    return migrate(resourceDir);
}

void Database::close() {
    {
        QSqlDatabase db = QSqlDatabase::database();
        if (db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    qDebug() << "[DB] Connection closed";
}

QSqlDatabase Database::connection() { return QSqlDatabase::database(); }

void Database::checkpoint() {
    QSqlQuery q(QSqlDatabase::database());
    q.exec(QStringLiteral("PRAGMA wal_checkpoint(PASSIVE)"));
}

// ── Migration runner ──────────────────────────────────────

int Database::currentVersion() {
    QSqlQuery q(QSqlDatabase::database());
    q.exec("PRAGMA user_version");
    if (q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

void Database::setVersion(int version) {
    QSqlQuery q(QSqlDatabase::database());
    // PRAGMA doesn't support parameter binding
    q.exec(QString("PRAGMA user_version = %1").arg(version));
}

QList<Database::Migration> Database::discoverMigrations(const QString& resourceDir) {
    QList<Migration> migrations;
    static const QRegularExpression pattern("^V(\\d+)_.+\\.sql$");

    QDirIterator it(resourceDir, {"*.sql"}, QDir::Files);
    while (it.hasNext()) {
        it.next();
        const QString fileName = it.fileName();
        auto match = pattern.match(fileName);
        if (match.hasMatch()) {
            Migration m;
            m.version = match.captured(1).toInt();
            m.path = it.filePath();
            m.name = fileName;
            migrations.append(m);
        }
    }

    // Sort by version number
    std::sort(migrations.begin(), migrations.end(),
              [](const Migration& a, const Migration& b) { return a.version < b.version; });

    return migrations;
}

bool Database::runMigration(const Migration& m) {
    QFile file(m.path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[DB] Cannot open migration file:" << m.path;
        return false;
    }

    QString sql = QString::fromUtf8(file.readAll());
    QSqlDatabase db = QSqlDatabase::database();

    // Strip SQL comment lines before splitting
    sql.remove(QRegularExpression("--[^\n]*"));

    // Split on semicolons and execute each statement
    // (SQLite doesn't support multi-statement exec)
    const QStringList statements = sql.split(';', Qt::SkipEmptyParts);
    for (const QString& stmt : statements) {
        const QString trimmed = stmt.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        QSqlQuery q(db);
        if (!q.exec(trimmed)) {
            qWarning() << "[DB] Migration" << m.name << "failed:" << q.lastError().text();
            qWarning() << "[DB] Statement:" << trimmed.left(200);
            return false;
        }
    }

    return true;
}

bool Database::migrate(const QString& resourceDir) {
    QSqlDatabase db = QSqlDatabase::database();
    const int current = currentVersion();
    const auto migrations = discoverMigrations(resourceDir);

    qDebug() << "[DB] Current schema version:" << current
             << "| Available migrations:" << migrations.size();

    int applied = 0;
    for (const auto& m : migrations) {
        if (m.version <= current) {
            continue;
        }

        qDebug() << "[DB] Applying" << m.name << "(V" << m.version << ")";

        if (!db.transaction()) {
            qWarning() << "[DB] Failed to begin transaction for" << m.name << ":"
                       << db.lastError().text();
            return false;
        }

        if (!runMigration(m)) {
            db.rollback();
            qWarning() << "[DB] Migration failed — stopping at V" << current;
            return false;
        }

        setVersion(m.version);
        if (!db.commit()) {
            qWarning() << "[DB] Failed to commit migration" << m.name << ":"
                       << db.lastError().text();
            db.rollback();
            return false;
        }
        applied++;
    }

    if (applied > 0) {
        qDebug() << "[DB] Applied" << applied << "migration(s), now at V" << currentVersion();
    } else {
        qDebug() << "[DB] Schema up to date";
    }

    return true;
}
