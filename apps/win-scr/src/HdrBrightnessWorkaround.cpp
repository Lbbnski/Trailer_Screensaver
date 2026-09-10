#include "HdrBrightnessWorkaround.h"
#include "util/Logging.h"

#include <QString>

#include <windows.h>
#include <wingdi.h>

#include <vector>

namespace ssv {

namespace {

void setAdvancedColorEnabled(LUID adapterId, UINT32 targetId, bool enabled)
{
    DISPLAYCONFIG_SET_ADVANCED_COLOR_STATE state{};
    state.header.type = DISPLAYCONFIG_DEVICE_INFO_SET_ADVANCED_COLOR_STATE;
    state.header.size = sizeof(state);
    state.header.adapterId = adapterId;
    state.header.id = targetId;
    state.enableAdvancedColor = enabled ? 1 : 0;
    DisplayConfigSetDeviceInfo(&state.header);
}

// True if this display target is actually in HDR right now — not just
// "advanced color enabled", which recent Windows versions also set for a
// plain SDR monitor under "automatically manage color for apps" (WCG
// composition with no HDR luminance mapping at all). Confirmed directly on
// a real (non-HDR) display: advancedColorEnabled came back true at
// bitsPerColorChannel == 8, which is not HDR — toggling advanced color off
// and back on for a display like that would just be a pointless flicker,
// since the bug this works around is specifically about the HDR luminance/
// SDR-white-level mapping, not WCG.
//
// DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO_2's activeColorMode is
// the precise, documented way to tell SDR/WCG/HDR apart, but it's only
// supported starting with a Windows 11 24H2-era build — confirmed directly
// that it returns ERROR_INVALID_PARAMETER on an older build even though
// the SDK this project compiles against declares it. Falls back to the
// bits-per-channel heuristic (true HDR negotiates >=10bpc; plain WCG-on-SDR
// stays at 8bpc) when the OS doesn't support the precise query.
bool isActuallyInHdrMode(LUID adapterId, UINT32 targetId)
{
    DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2 info2{};
    info2.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO_2;
    info2.header.size = sizeof(info2);
    info2.header.adapterId = adapterId;
    info2.header.id = targetId;
    if (DisplayConfigGetDeviceInfo(&info2.header) == ERROR_SUCCESS)
        return info2.activeColorMode == DISPLAYCONFIG_ADVANCED_COLOR_MODE_HDR;

    DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO info{};
    info.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
    info.header.size = sizeof(info);
    info.header.adapterId = adapterId;
    info.header.id = targetId;
    if (DisplayConfigGetDeviceInfo(&info.header) != ERROR_SUCCESS)
        return false;

    return info.advancedColorEnabled && info.bitsPerColorChannel >= 10;
}

} // namespace

void tryResetHdrBrightnessOnExit()
{
    UINT32 numPaths = 0, numModes = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPaths, &numModes) != ERROR_SUCCESS || numPaths == 0) {
        logInfo(QStringLiteral("hdr-brightness-workaround: no active display paths found — nothing to do"));
        return;
    }

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(numPaths);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(numModes);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numPaths, paths.data(), &numModes, modes.data(), nullptr) != ERROR_SUCCESS) {
        logInfo(QStringLiteral("hdr-brightness-workaround: QueryDisplayConfig failed — nothing to do"));
        return;
    }

    bool toggledAny = false;
    for (UINT32 i = 0; i < numPaths; ++i) {
        const LUID adapterId = paths[i].targetInfo.adapterId;
        const UINT32 targetId = paths[i].targetInfo.id;

        if (!isActuallyInHdrMode(adapterId, targetId))
            continue;

        logInfo(QStringLiteral("hdr-brightness-workaround: toggling advanced color off/on for display target %1")
                    .arg(targetId));
        setAdvancedColorEnabled(adapterId, targetId, false);
        setAdvancedColorEnabled(adapterId, targetId, true);
        toggledAny = true;
    }

    if (!toggledAny)
        logInfo(QStringLiteral("hdr-brightness-workaround: ran, but no display was actually in HDR mode — nothing to do"));
}

} // namespace ssv
