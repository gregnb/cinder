#ifndef IDLDB_H
#define IDLDB_H

#include <QList>
#include <QPair>
#include <QString>

class QSqlDatabase;

class IdlDb {
  public:
    // Store an IDL JSON string for a program. Replaces existing entry.
    static bool upsertIdl(const QString& programId, const QString& programName,
                          const QString& idlJson);

    // Retrieve cached IDL JSON for a program. Returns empty if not cached.
    static QString getIdlJson(const QString& programId);

    // Store a "no IDL" marker (empty idl_json) to prevent re-fetching.
    static bool markNoIdl(const QString& programId);

    // Check if we already know this program has no IDL.
    static bool isMarkedNoIdl(const QString& programId);

    // Get all cached entries: (programId, idlJson). Empty idl_json = no-IDL marker.
    static QList<QPair<QString, QString>> getAll();

    // Count cached IDL entries.
    static int countAll();

  private:
    static QSqlDatabase db();
};

#endif // IDLDB_H
