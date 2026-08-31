; Inno Setup script for Steam Trailer Screensaver.
;
; Windows only scans top-level *.scr files directly in %SystemRoot%\System32
; for its screensaver picker (Display Settings > Screensaver), so this
; installer places the .scr and its one runtime dependency (libmpv-2.dll)
; there directly, in addition to a normal Program Files install. See
; docs/ARCHITECTURE.md's "Build & packaging" section for why win-scr release
; builds link Qt statically specifically to keep that System32 footprint to
; just those two files.
;
; Build inputs expected before compiling this script (see
; docs/ARCHITECTURE.md): a static-Qt release build of SteamTrailerSaver.scr,
; the settings GUI executable, and third_party/mpv/win64/bin/libmpv-2.dll.

#define MyAppName "Steam Trailer Screensaver"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Steam Trailer Screensaver Project"

[Setup]
AppId={{B3B8B6C9-6E31-4B7B-9C4B-2F1E9F5E7B10}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\SteamTrailerScreensaver
DefaultGroupName={#MyAppName}
OutputBaseFilename=SteamTrailerScreensaverSetup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
; Required for the System32 install step below.

[Files]
Source: "..\..\build\Release\SteamTrailerSaver.scr"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\build\Release\steam-trailer-saver-settings.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\third_party\mpv\win64\bin\libmpv-2.dll"; DestDir: "{app}"; Flags: ignoreversion

; Copies enumerable by Windows' screensaver picker.
Source: "..\..\build\Release\SteamTrailerSaver.scr"; DestDir: "{sys}"; Flags: ignoreversion uninsneveruninstall
Source: "..\..\third_party\mpv\win64\bin\libmpv-2.dll"; DestDir: "{sys}"; Flags: ignoreversion uninsneveruninstall

[Icons]
Name: "{group}\{#MyAppName} Settings"; Filename: "{app}\steam-trailer-saver-settings.exe"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: files; Name: "{sys}\SteamTrailerSaver.scr"
Type: files; Name: "{sys}\libmpv-2.dll"

[Run]
Filename: "{app}\steam-trailer-saver-settings.exe"; Description: "Configure genres and resolution now"; Flags: postinstall nowait skipifsilent unchecked
