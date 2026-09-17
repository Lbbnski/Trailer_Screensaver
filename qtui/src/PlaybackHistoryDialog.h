#pragma once

#include "cache/CacheDatabase.h"
#include "cache/CacheRepository.h"

#include <QDialog>

#include <memory>

class QTableWidget;
class QPushButton;
class QLabel;

namespace ssv {

// Shows everything recorded in playback_history (see CacheRepository::
// recentPlaybackHistory), newest first, so the user can:
//  - look up a game they liked the trailer for (opens its store/game page),
//  - block a specific game from ever being offered again, and
//  - report a played "trailer" that wasn't actually a game trailer (a
//    movie/TV trailer, a Let's Play, ...), writing what this app knows
//    about it to ConfigPaths::reportedTrailersLogPath() for a developer to
//    look at later.
//
// Opens its own CacheDatabase connection (rather than being handed one)
// since it's launched from SettingsDialog, which only ever deals with
// Config — same "each independently-constructed piece opens its own
// connection" pattern PlaybackSession already uses (CacheDatabase::open()
// gives every instance its own uniquely-named QSqlDatabase connection, safe
// to do more than once against the same file thanks to WAL mode).
class PlaybackHistoryDialog : public QDialog {
    Q_OBJECT
public:
    explicit PlaybackHistoryDialog(QWidget* parent = nullptr);

private slots:
    void onSelectionChanged();
    void onOpenStorePage();
    void onToggleBlocked();
    void onReport();

private:
    void reloadHistory();
    void updateButtonStates();
    // The entry backing the currently-selected row, or nullptr if none.
    const CacheRepository::PlaybackHistoryEntry* selectedEntry() const;

    CacheDatabase m_db;
    std::unique_ptr<CacheRepository> m_repo;

    QList<CacheRepository::PlaybackHistoryEntry> m_entries;

    QTableWidget* m_table = nullptr;
    QPushButton* m_openButton = nullptr;
    QPushButton* m_blockButton = nullptr;
    QPushButton* m_reportButton = nullptr;
    QLabel* m_reportPathLabel = nullptr;
};

} // namespace ssv
