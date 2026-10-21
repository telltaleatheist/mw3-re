# Playtest triage

Owen's runs of the real program (`build/mw3 -nofmv`), what he saw, and who
takes it. Items close with the commit that fixes them.

## 2026-10-21, first campaign run (New Campaign → pilot → briefing → c1m1)

Fixed the same morning: the menus' images (28430f1), lost clicks (65285b6),
the briefing's load freeze (5a55624).

| # | Seen | Area | Owner | State |
|---|---|---|---|---|
| 1 | No ripple on the main menu buttons on hover | 2D render, 0x00558dd0 | mac 2 | fixed 74b15e3 (mac2/ripple; render-ripple.md says what to see) |
| 2 | "Ok to delete pilot?" is a native OS alert, not the game's box | message box 0x004ac150 | mac 3 | open |
| 3 | Escape in a mission turns the screen purple instead of the pause menu | pause menu, the frame clear | mac 3 | open |
| 4 | The mech isn't visible: a couple of odd polygons where the cockpit view should be | view modes, the cockpit model (MechData +0x228, 0x00430ad0) | mac 2 | open |
| 5 | X doesn't switch views (should start inside, X to third person) | view modes | mac 2 | open |
| 6 | Enter doesn't cycle the weapon groups | controls → weapons | mac 2 | open |
| 7 | Numpad 0 doesn't show the zoom reticle | HUD zoom window | mac 3 | open |
| 8 | Weapons stay green on the HUD: no recharge shown | HUD weapon lines | mac 3 | open |
| 9 | "Shutting down" plays but the mech never shuts down; then the speed locks, no slowing, firing or manual shutdown | heat shutdown, the action queue | mac 2 | fixed 5e05304: the power-down's Stop (0x426c10) was never called, so the throttle stayed; heat.md |
| 10 | HUD images not found in the mission: dmg_*_blk, hud_othererrow_* | the image library after the mission load | mac 3 | open |
| 11 | Trucks, APCs and mobile HQs fail to build | vehicles (AI controls) | AI area | waiting |
