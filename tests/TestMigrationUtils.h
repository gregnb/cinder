#ifndef TEST_MIGRATION_UTILS_H
#define TEST_MIGRATION_UTILS_H

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>

namespace TestMigrationUtils {

    inline void copyAllMigrationsToDir(const QString& targetDir) {
        QDir().mkpath(targetDir);

        QDirIterator it(":/migrations", {"V*.sql"}, QDir::Files);
        while (it.hasNext()) {
            it.next();
            QFile src(it.filePath());
            if (!src.open(QIODevice::ReadOnly)) {
                continue;
            }

            QFile dst(targetDir + "/" + it.fileName());
            if (!dst.open(QIODevice::WriteOnly)) {
                continue;
            }

            dst.write(src.readAll());
        }
    }

    inline int latestMigrationVersion(const QString& resourceDir = QStringLiteral(":/migrations")) {
        static const QRegularExpression pattern("^V(\\d+)_.+\\.sql$");

        int latest = 0;
        QDirIterator it(resourceDir, {"V*.sql"}, QDir::Files);
        while (it.hasNext()) {
            it.next();
            const auto match = pattern.match(it.fileName());
            if (!match.hasMatch()) {
                continue;
            }

            latest = std::max(latest, match.captured(1).toInt());
        }

        return latest;
    }

} // namespace TestMigrationUtils

#endif // TEST_MIGRATION_UTILS_H
