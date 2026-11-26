; The launcher's Windows installer for players: Inno Setup (jrsoftware.org, free), built by
; tools\windows\package.cmd from the package folder it has just made:
;   iscc /DAppVersion=<git describe> /DDist=<the package folder> /O<out dir> tools\windows\installer.iss
; It installs to Program Files\MechWarrior 3 Rebuild with a Start-menu
; shortcut (a desktop one ticked by default) and an uninstaller. The game
; writes nothing there: the player's settings, saves, imported movies, music
; and a disc install live in %APPDATA%\BattleTech\MechWarrior 3, which the
; uninstaller leaves alone. No file of the original game is in the installer.

#ifndef AppVersion
  #define AppVersion "dev"
#endif
#ifndef Dist
  #error Give /DDist=<the package folder> (package.cmd does).
#endif
#ifndef Stem
  ; MW3-Rebuild, the name the launcher's updater takes
  #define Stem "MW3-Rebuild"
#endif

; The shortcut's name: BattleTech (2026-11-20), its comment naming the games for the Start menu's search.
#define Shortcut "BattleTech"
#define ShortcutComment "MechWarrior 3, Pirate's Moon and MechWarrior 1, rebuilt"

[Setup]
; The same AppId every version: a new installer upgrades the old install in place.
AppId={{920C98A6-550D-41A3-839E-540B93B8F40D}
AppName=MechWarrior 3 Rebuild
AppVersion={#AppVersion}
AppVerName=MechWarrior 3 Rebuild {#AppVersion}
AppPublisher=MechWarrior 3 Rebuild (a fan project)
AppComments=A rebuild of MechWarrior 3's program; it needs your own copy of the game.
DefaultDirName={autopf}\MechWarrior 3 Rebuild
; The folder page every time, an upgrade's too (the default hides it there),
; offering the folder already installed to.
DisableDirPage=no
DefaultGroupName=MechWarrior 3 Rebuild
DisableProgramGroupPage=yes
; Program Files needs administrator rights; a player without them can pick
; "only for me" (the user's own Programs folder) in the first dialog.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=mw3.ico
UninstallDisplayIcon={app}\mw3launcher.exe
UninstallDisplayName=MechWarrior 3 Rebuild
OutputBaseFilename={#Stem}-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
#if FileExists(AddBackslash(Dist) + "LICENSE.txt")
LicenseFile={#Dist}\LICENSE.txt
#endif

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#Dist}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; An earlier version's shortcuts, named MechWarrior 3 before the combined package became BattleTech.
Type: files; Name: "{autoprograms}\MechWarrior 3.lnk"
Type: files; Name: "{autodesktop}\MechWarrior 3.lnk"
; The launcher alone over an earlier package that carried the games (1.2.4 and before, or MW3 alone): their
; programs go, so the launcher does not start an old copy beside it; it downloads each game into its own folder
; (docs/launcher.md, Games on demand). Settings, pilots, saves, mods and the copied game files are in %APPDATA% and
; stay.
Type: files; Name: "{app}\mw3.exe"
Type: files; Name: "{app}\mw3import.exe"
Type: files; Name: "{app}\mw3indeo.exe"
Type: files; Name: "{app}\mw3pm.exe"
Type: files; Name: "{app}\mw1.exe"
Type: filesandordirs; Name: "{app}\mw1data"
; Elthial's MechWarrior (1989) port, which earlier packages could carry (opt-in): no package carries it now
; (launcher split b5), so this folder and THIRD-PARTY-MW1989.txt below are removed only for older installs.
Type: filesandordirs; Name: "{app}\MechWarrior1989"
Type: files; Name: "{app}\README-MW1.txt"
Type: files; Name: "{app}\THIRD-PARTY-MW1.txt"
Type: files; Name: "{app}\THIRD-PARTY-MW1989.txt"
Type: files; Name: "{app}\av*.dll"
Type: files; Name: "{app}\sw*.dll"

[Icons]
; The launcher is the shortcut; mw3.exe is installed
; and starts alone too, but has no shortcut (2026-11-02; launcher.md).
Name: "{autoprograms}\{#Shortcut}"; Filename: "{app}\mw3launcher.exe"; WorkingDir: "{app}"; Comment: "{#ShortcutComment}"
Name: "{autoprograms}\MechWarrior 3 Rebuild - Read me"; Filename: "{app}\README.txt"
Name: "{autodesktop}\{#Shortcut}"; Filename: "{app}\mw3launcher.exe"; WorkingDir: "{app}"; Comment: "{#ShortcutComment}"; Tasks: desktopicon

[Run]
Filename: "{app}\mw3launcher.exe"; Description: "{cm:LaunchProgram,{#Shortcut}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Only what the installer put in {app}; %APPDATA%\BattleTech\MechWarrior 3 stays.
Type: dirifempty; Name: "{app}"

[Messages]
UninstalledAll=%1 was removed from your computer.%n%nYour settings, pilots and saved games, and any MechWarrior 3 installed from your disc, are kept in %APPDATA%\BattleTech\MechWarrior 3. Delete that folder to remove them too.
