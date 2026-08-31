#include "SettingsDialog.h"
#include "GenreListModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ssv {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Steam Trailer Screensaver Settings"));
    buildUi();
    loadConfig();
}

void SettingsDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);

    // --- Playback ---
    auto* playbackBox = new QGroupBox(tr("Playback"), this);
    auto* playbackForm = new QFormLayout(playbackBox);

    m_resolutionCombo = new QComboBox(playbackBox);
    m_resolutionCombo->addItem(tr("Maximum available"), QStringLiteral("max"));
    m_resolutionCombo->addItem(tr("480p (lower bandwidth)"), QStringLiteral("480p"));
    playbackForm->addRow(tr("Video resolution:"), m_resolutionCombo);

    m_monitorModeCombo = new QComboBox(playbackBox);
    m_monitorModeCombo->addItem(tr("Independent trailer per monitor"), QStringLiteral("independent"));
    m_monitorModeCombo->addItem(tr("Primary monitor only"), QStringLiteral("primaryOnly"));
    playbackForm->addRow(tr("Multi-monitor mode:"), m_monitorModeCombo);

    m_mutedCheck = new QCheckBox(tr("Mute audio"), playbackBox);
    playbackForm->addRow(QString(), m_mutedCheck);

    m_hardwareDecodeCheck = new QCheckBox(tr("Use hardware video decoding"), playbackBox);
    playbackForm->addRow(QString(), m_hardwareDecodeCheck);

    m_debugOverlayCheck = new QCheckBox(tr("Show debug log overlay"), playbackBox);
    playbackForm->addRow(QString(), m_debugOverlayCheck);

    root->addWidget(playbackBox);

    // --- Filter ---
    auto* filterBox = new QGroupBox(tr("Genre && Age Filter"), this);
    auto* filterLayout = new QVBoxLayout(filterBox);

    auto* modeRow = new QHBoxLayout();
    m_allowListRadio = new QRadioButton(tr("Only show these genres"), filterBox);
    m_blockListRadio = new QRadioButton(tr("Hide these genres"), filterBox);
    m_allowListRadio->setChecked(true);
    modeRow->addWidget(m_allowListRadio);
    modeRow->addWidget(m_blockListRadio);
    filterLayout->addLayout(modeRow);

    m_genreModel = new GenreListModel(this);
    m_genreList = new QListView(filterBox);
    m_genreList->setModel(m_genreModel);
    filterLayout->addWidget(m_genreList);

    auto* ageRow = new QHBoxLayout();
    ageRow->addWidget(new QLabel(tr("Maximum age rating:"), filterBox));
    m_maxAgeSlider = new QSlider(Qt::Horizontal, filterBox);
    m_maxAgeSlider->setRange(0, 18);
    m_maxAgeSpin = new QSpinBox(filterBox);
    m_maxAgeSpin->setRange(0, 18);
    connect(m_maxAgeSlider, &QSlider::valueChanged, m_maxAgeSpin, &QSpinBox::setValue);
    connect(m_maxAgeSpin, &QSpinBox::valueChanged, m_maxAgeSlider, &QSlider::setValue);
    ageRow->addWidget(m_maxAgeSlider);
    ageRow->addWidget(m_maxAgeSpin);
    filterLayout->addLayout(ageRow);

    m_preferPopularCheck = new QCheckBox(tr("Prefer popular / most-played games"), filterBox);
    filterLayout->addWidget(m_preferPopularCheck);

    root->addWidget(filterBox);

    // --- Sources ---
    // Only Steam exists today; this box is deliberately structured as a
    // checkable list so a future second IMetadataSource implementation adds
    // one more checkbox here rather than needing new UI.
    auto* sourcesBox = new QGroupBox(tr("Trailer Sources"), this);
    auto* sourcesLayout = new QVBoxLayout(sourcesBox);
    m_steamSourceCheck = new QCheckBox(tr("Steam (with YouTube fallback for games with no Steam trailer)"), sourcesBox);
    m_steamSourceCheck->setChecked(true);
    m_steamSourceCheck->setEnabled(false); // only source available right now
    sourcesLayout->addWidget(m_steamSourceCheck);
    root->addWidget(sourcesBox);

    // --- Buttons ---
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void SettingsDialog::loadConfig()
{
    const Config cfg = Config::load();

    const int resIdx = m_resolutionCombo->findData(cfg.playback.maxResolution == MaxResolution::P480
                                                         ? QStringLiteral("480p") : QStringLiteral("max"));
    m_resolutionCombo->setCurrentIndex(resIdx >= 0 ? resIdx : 0);

    const int monIdx = m_monitorModeCombo->findData(cfg.playback.monitorMode == MonitorMode::PrimaryOnly
                                                          ? QStringLiteral("primaryOnly") : QStringLiteral("independent"));
    m_monitorModeCombo->setCurrentIndex(monIdx >= 0 ? monIdx : 0);

    m_mutedCheck->setChecked(cfg.playback.muted);
    m_hardwareDecodeCheck->setChecked(cfg.playback.hardwareDecode);
    m_debugOverlayCheck->setChecked(cfg.advanced.debugOverlay);

    m_allowListRadio->setChecked(cfg.filter.mode == GenreFilter::Mode::AllowList);
    m_blockListRadio->setChecked(cfg.filter.mode == GenreFilter::Mode::BlockList);
    m_genreModel->setCheckedGenres(cfg.filter.genres);

    m_maxAgeSlider->setValue(cfg.filter.maxAge);
    m_maxAgeSpin->setValue(cfg.filter.maxAge);

    m_preferPopularCheck->setChecked(cfg.filter.preferPopular);
}

Config SettingsDialog::collectConfig() const
{
    Config cfg = Config::load(); // preserve fields this dialog doesn't expose (advanced.*, sources.steam.*)

    cfg.playback.maxResolution = m_resolutionCombo->currentData().toString() == QStringLiteral("480p")
                                      ? MaxResolution::P480 : MaxResolution::Max;
    cfg.playback.monitorMode = m_monitorModeCombo->currentData().toString() == QStringLiteral("primaryOnly")
                                    ? MonitorMode::PrimaryOnly : MonitorMode::Independent;
    cfg.playback.muted = m_mutedCheck->isChecked();
    cfg.playback.hardwareDecode = m_hardwareDecodeCheck->isChecked();
    cfg.advanced.debugOverlay = m_debugOverlayCheck->isChecked();

    cfg.filter.mode = m_blockListRadio->isChecked() ? GenreFilter::Mode::BlockList : GenreFilter::Mode::AllowList;
    cfg.filter.genres = m_genreModel->checkedGenres();
    cfg.filter.maxAge = m_maxAgeSpin->value();
    cfg.filter.preferPopular = m_preferPopularCheck->isChecked();

    return cfg;
}

void SettingsDialog::onAccept()
{
    const Config cfg = collectConfig();
    if (cfg.save()) {
        accept();
        return;
    }
    QMessageBox::warning(this, tr("Couldn't save settings"),
                          tr("Settings could not be written to disk. Check that the configuration "
                             "directory is writable and try again."));
}

} // namespace ssv
