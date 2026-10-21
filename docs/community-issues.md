# What broke for players

A watch-list from the community's reports and unofficial fixes (web search,
2026-10-20, Owen: "knowing what broke for people could point us toward
problems that we'll almost certainly encounter too"). Anecdotes, not
confirmed bugs; each is checked against the port when its code is reached.

| Report | Where | What the port says so far |
|---|---|---|
| Physics and "rubberbanding" go wrong on fast machines; players cap at 60 fps | VOGONS, a WineD3D + ImageCFG recipe | Matches the per-frame steps found in the port (falls, slopes, jets, blend-in, lava, smoke): all now per 1/30 s (`fixes.md`, Frame rate) |
| Frequent crashes when hitting mechs (2016, d3d mode) | VOGONS | Open; the crater clipper's freed-buffer reads past 128 entries and its fixed side tables (`craters.md`, fixed in the port) are candidates for misses that crater the ground. Damage and the hit path are ported with crash fixes (equipment critical's float bits, `fixes.md`); check the rest of the hit path when weapons' second half lands |
| Crash when the mech overheats or on an alpha strike (Windows XP) | MajorGeeks compatibility DB | Open. Heat and power are ported; an overheat shuts down through 0x41cc30. Look at what the shutdown and alpha fire reach that the port has not (HUD, sound, the weapon manager's fire-all) |
| Display freezes when cresting hills or riverbanks | MajorGeeks | Open; possibly the ground probes (`movement-ground.md`: the never-written probe width, now a fix) or the renderer's clipping. Also a candidate: the crater clipper's walk (0x510da0) loops forever, freezing the game, when a terrain polygon's corner lies on a crater's outline to within a float's error (`craters.md`; 3 to 10 in 1,200 random walks); a shot hitting the ground on a slope or bank makes exactly such craters. Fixed in the port |
| Pressing Esc at a resolution above 640x480 crashes; Options from the pause menu crashes above ~1000 lines; 1152x864 broken | VOGONS (teleguy's resolution patch), PCGamingWiki widescreen fix | Partly explained (menus.md, Resolution): the pause screen makes the option screens at the mission's size, and their still background is drawn with that size as the source rectangle of a 1280x960 picture, so above 960 lines (exactly 1280x1024 and 1600x1200) the draw reads past it. The original's switch to 640x480 before the first frame should clip it, so the crash is likely in a draw before the switch or in the switch's own screen grab (zVideo, not ported). The port cuts every source rectangle at its image (FIX (B), fixes.md). Esc above 640x480: the pause screen itself, not ported |
| Three resolutions in the menu plus three hidden (1152x864, 1280x1024, 1600x1200) set only in the registry | VOGONS | Confirmed (settings.md): InGameVMode, the mode a mission starts in, is saved and read as any number, and the mode switch 0x40d810 knows 8, 9, 10 as 1152x864, 1280x1024, 1600x1200; the VMode the menus use is never saved |
| Internal aspect ratio fixed at 4:3 | PCGamingWiki | Projection is ported (`render-matrix.md`); widescreen would be a FIX (B) candidate |
| DDrawCompat: the picture sometimes freezes while the game runs | VOGONS | A wrapper issue, not the game's; the port does not use DirectDraw |

Sources: [VOGONS: MW3 on Win10/11](https://www.vogons.org/viewtopic.php?p=1420207),
[VOGONS: physics and rubberbanding](https://www.vogons.org/viewtopic.php?p=510553),
[VOGONS: low fps and crashes](https://www.vogons.org/viewtopic.php?p=510594),
[VOGONS: resolution patch](https://www.vogons.org/viewtopic.php?p=495930),
[PCGamingWiki widescreen fix](https://community.pcgamingwiki.com/files/file/2621-mechwarrior-3-widescreenultrawidesuperwidemultimonitor-fix/),
[MajorGeeks compatibility reports](https://www.majorgeeks.com/compatdb/details/mechwarrior_3_windows.html).
