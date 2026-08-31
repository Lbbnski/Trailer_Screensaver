#pragma once

#include <QSqlDatabase>
#include <QString>

namespace ssv {

// Opens (and, on first use, creates/migrates) the SQLite cache database.
// This is the persistence layer that lets discovery/detail-fetch progress
// survive between the short-lived process runs a screensaver naturally has
// — there is deliberately no long-running service keeping this warm; see
// docs/ARCHITECTURE.md's "idle-footprint invariant".
//
// All tables are keyed (fully or in part) by source_id so multiple
// IMetadataSource implementations' rows coexist without collision once a
// second source is added.
class CacheDatabase {
public:
    // Opens (creating parent directories and the file if needed) the
    // database at `path`, running any pending schema migrations. Returns a
    // default-constructed (invalid) CacheDatabase on failure — callers
    // should check isOpen().
    static CacheDatabase open(const QString& path);

    bool isOpen() const;
    QSqlDatabase& handle();

private:
    QSqlDatabase m_db;
    bool m_open = false;

    static bool migrate(QSqlDatabase& db);
};

} // namespace ssv
