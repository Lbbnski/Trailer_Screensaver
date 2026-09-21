#include "PlaybackHistoryDialog.h"

#include "config/ConfigPaths.h"
#include "sources/steam/YoutubeFallbackResolver.h"
#include "util/Logging.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace ssv {

namespace {
constexpr int kHistoryLimit = 500;
enum Column { ColTitle, ColDeveloper, ColSource, ColPlayedAt, ColBlocked, ColCount };
} // namespace

PlaybackHistoryDialog::PlaybackHistoryDialog(QWidget* parent)
    : QDialog(parent)
    , m_db(CacheDatabase::open(ConfigPaths::cacheDatabasePath()))
{
    setWindowTitle(tr("Recently Played Trailers"));
    resize(720, 480);

    auto* root = new QVBoxLayout(this);

    if (!m_db.isOpen()) {
        root->addWidget(new QLabel(tr("Could not open the cache database."), this));
        auto* buttons = new QHBoxLayout();
        auto* closeButton = new QPushButton(tr("Close"), this);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
        buttons->addStretch();
        buttons->addWidget(closeButton);
        root->addLayout(buttons);
        return;
    }
    m_repo = std::make_unique<CacheRepository>(m_db);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(ColCount);
    m_table->setHorizontalHeaderLabels({tr("Title"), tr("Developer"), tr("Source"), tr("Played"), tr("Blocked")});
    m_table->horizontalHeader()->setSectionResizeMode(ColTitle, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &PlaybackHistoryDialog::onSelectionChanged);
    root->addWidget(m_table);

    m_reportPathLabel = new QLabel(
        tr("Reports are written to: %1").arg(ConfigPaths::reportedTrailersLogPath()), this);
    m_reportPathLabel->setWordWrap(true);
    root->addWidget(m_reportPathLabel);

    auto* buttons = new QHBoxLayout();
    m_openButton = new QPushButton(tr("Open Store Page"), this);
    connect(m_openButton, &QPushButton::clicked, this, &PlaybackHistoryDialog::onOpenStorePage);
    buttons->addWidget(m_openButton);

    m_blockButton = new QPushButton(tr("Block This Game"), this);
    connect(m_blockButton, &QPushButton::clicked, this, &PlaybackHistoryDialog::onToggleBlocked);
    buttons->addWidget(m_blockButton);

    m_reportButton = new QPushButton(tr("Report as Not a Game Trailer"), this);
    connect(m_reportButton, &QPushButton::clicked, this, &PlaybackHistoryDialog::onReport);
    buttons->addWidget(m_reportButton);

    buttons->addStretch();
    auto* closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(closeButton);
    root->addLayout(buttons);

    reloadHistory();
    updateButtonStates();
}

void PlaybackHistoryDialog::reloadHistory()
{
    m_entries = m_repo->recentPlaybackHistory(kHistoryLimit);

    m_table->setRowCount(m_entries.size());
    for (int row = 0; row < m_entries.size(); ++row) {
        const auto& e = m_entries[row];
        m_table->setItem(row, ColTitle, new QTableWidgetItem(e.title));
        m_table->setItem(row, ColDeveloper, new QTableWidgetItem(e.developer));
        m_table->setItem(row, ColSource, new QTableWidgetItem(e.sourceId));
        m_table->setItem(row, ColPlayedAt, new QTableWidgetItem(
            QLocale::system().toString(QDateTime::fromSecsSinceEpoch(e.playedAt), QLocale::ShortFormat)));
        m_table->setItem(row, ColBlocked, new QTableWidgetItem(e.blocked ? tr("Yes") : QString()));
    }
}

const CacheRepository::PlaybackHistoryEntry* PlaybackHistoryDialog::selectedEntry() const
{
    const auto selected = m_table->selectionModel() ? m_table->selectionModel()->selectedRows() : QModelIndexList();
    if (selected.isEmpty())
        return nullptr;
    const int row = selected.first().row();
    if (row < 0 || row >= m_entries.size())
        return nullptr;
    return &m_entries[row];
}

void PlaybackHistoryDialog::onSelectionChanged()
{
    updateButtonStates();
}

void PlaybackHistoryDialog::updateButtonStates()
{
    const auto* entry = selectedEntry();
    m_openButton->setEnabled(entry && !entry->storeUrl.isEmpty());
    m_blockButton->setEnabled(entry != nullptr);
    m_reportButton->setEnabled(entry != nullptr);
    m_blockButton->setText(entry && entry->blocked ? tr("Unblock This Game") : tr("Block This Game"));
}

void PlaybackHistoryDialog::onOpenStorePage()
{
    const auto* entry = selectedEntry();
    if (entry && !entry->storeUrl.isEmpty())
        QDesktopServices::openUrl(QUrl(entry->storeUrl));
}

void PlaybackHistoryDialog::onToggleBlocked()
{
    const auto* entry = selectedEntry();
    if (!entry)
        return;

    const QString sourceId = entry->sourceId;
    const QString nativeId = entry->nativeId;
    const bool wasBlocked = entry->blocked;

    if (wasBlocked)
        m_repo->unblockGame(sourceId, nativeId);
    else
        m_repo->blockGame(sourceId, nativeId, QDateTime::currentSecsSinceEpoch());

    // Reflect the new state in every row for this game, not just the
    // selected one — the same game can appear multiple times in the
    // history list.
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries[row].sourceId == sourceId && m_entries[row].nativeId == nativeId) {
            m_entries[row].blocked = !wasBlocked;
            m_table->item(row, ColBlocked)->setText(!wasBlocked ? tr("Yes") : QString());
        }
    }
    updateButtonStates();
}

void PlaybackHistoryDialog::onReport()
{
    const auto* entry = selectedEntry();
    if (!entry)
        return;

    bool ok = false;
    const QString note = QInputDialog::getMultiLineText(
        this, tr("Report as Not a Game Trailer"),
        tr("What was wrong with \"%1\"? (optional — a movie trailer, a Let's Play, "
           "the wrong game entirely, ...)").arg(entry->title),
        QString(), &ok);
    if (!ok)
        return;

    QJsonObject record{
        {"title", entry->title},
        {"developer", entry->developer},
        {"sourceId", entry->sourceId},
        {"nativeId", entry->nativeId},
        {"storeUrl", entry->storeUrl},
        {"videoUrl", entry->videoUrl},
        {"playedAt", QDateTime::fromSecsSinceEpoch(entry->playedAt).toString(Qt::ISODate)},
        {"userNote", note},
    };
    if (const auto query = m_repo->fallbackQueryUsed(entry->sourceId, entry->nativeId))
        record["fallbackQueryUsed"] = *query;

    appendDiagnosticRecord(ConfigPaths::reportedTrailersLogPath(), record);

    // Takes effect immediately, not just for whoever reads the report later:
    // this exact video is never played again (the game itself stays
    // eligible and simply picks a different trailer).
    const QString videoId = YoutubeFallbackResolver::videoIdFromUrl(entry->videoUrl);
    if (!videoId.isEmpty())
        m_repo->rejectVideo(videoId, QDateTime::currentSecsSinceEpoch());

    QMessageBox::information(this, tr("Report Recorded"),
        tr("Thanks — this video won't be played again, and the report has been saved to:\n%1\n\n"
           "Share that file to help improve the filtering.")
            .arg(ConfigPaths::reportedTrailersLogPath()));
}

} // namespace ssv
