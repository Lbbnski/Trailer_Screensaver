#pragma once

#include "config/Config.h"

#include <QDialog>

class QComboBox;
class QCheckBox;
class QSpinBox;
class QSlider;
class QListView;
class QRadioButton;

namespace ssv {

class GenreListModel;
class MpvGLWidget;

// The one settings UI, reused from three different entry points: Windows'
// `/c` / `/c:HWND` screensaver-configure contract, the Linux xscreensaver
// hack's `--configure` escape hatch (its own .xml capplet only covers
// resolution/age/blacklist-toggle — full genre multi-select doesn't map to
// that widget schema), and the standalone settings-gui app. Centralizing it
// here means it's written once instead of three times.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    // Loads the current on-disk Config into the UI.
    void loadConfig();

private slots:
    void onAccept();

private:
    void buildUi();
    Config collectConfig() const;

    QComboBox* m_resolutionCombo = nullptr;
    QComboBox* m_monitorModeCombo = nullptr;
    QCheckBox* m_mutedCheck = nullptr;
    QCheckBox* m_hardwareDecodeCheck = nullptr;
    QCheckBox* m_debugOverlayCheck = nullptr;

    QRadioButton* m_allowListRadio = nullptr;
    QRadioButton* m_blockListRadio = nullptr;
    QListView* m_genreList = nullptr;
    GenreListModel* m_genreModel = nullptr;
    QSlider* m_maxAgeSlider = nullptr;
    QSpinBox* m_maxAgeSpin = nullptr;
    QCheckBox* m_preferPopularCheck = nullptr;

    QCheckBox* m_steamSourceCheck = nullptr; // the only source today; future sources add siblings here
};

} // namespace ssv
