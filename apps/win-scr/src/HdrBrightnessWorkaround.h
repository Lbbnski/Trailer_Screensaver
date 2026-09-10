#pragma once

namespace ssv {

// Experimental, opt-in mitigation for a well-known Windows 11 "Auto HDR"
// bug (see docs/LIMITATIONS.md): after a fullscreen app hands control back
// to the desktop, Windows sometimes fails to correctly restore the "SDR
// content brightness" scalar it applies while Auto HDR is engaged — the
// Settings > Display > HDR slider still shows its configured value, but
// the actual brightness applied to the desktop is wrong until the user
// manually nudges that slider. Confirmed directly with a user that it's
// not specific to this app (happens with unrelated fullscreen apps/games
// too), only with Auto HDR on, and that nudging the slider fixes it
// instantly — the classic symptom of Windows failing to recompute the SDR
// mapping on its own after a fullscreen exit.
//
// There is no public Windows API to directly set the SDR-brightness scalar
// itself: DISPLAYCONFIG_SDR_WHITE_LEVEL (queried via
// DisplayConfigGetDeviceInfo) is read-only, with no corresponding "set"
// info type. The only documented lever that comes close is toggling a
// display's Advanced Color (HDR/WCG) state off and back on via
// DisplayConfigSetDeviceInfo + DISPLAYCONFIG_SET_ADVANCED_COLOR_STATE —
// the same public CCD API third-party HDR-toggle utilities use — which
// forces Windows to recompute the SDR mapping for that display, the same
// class of event as touching the slider.
//
// UNVERIFIED against a real HDR display: this was written and built
// without one available. Call it only when
// AdvancedConfig::workaroundHdrBrightnessReset is explicitly enabled, and
// expect a brief flicker on every HDR-enabled display it touches.
void tryResetHdrBrightnessOnExit();

} // namespace ssv
