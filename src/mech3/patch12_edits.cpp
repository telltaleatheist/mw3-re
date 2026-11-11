// The 1.2 data layer's record edits (docs/patch12.md): the values, keys and
// items the patch changed in 1.0's reader entries, as a table
// (patch12_edits.inc, written by tools/analysis/patch12_edits.py).
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "mech3/patch12.h"
#include "mech3/patch12_tree.h"

namespace mech3::patch12 {
namespace {

using zrdr::Node;

enum Op { Set, Erase, Insert };

struct Edit {
  const char *archive, *entry;
  Op op;
  // Keys down from the entry's record: KEY (the item after the string KEY),
  // KEY#n (after its n-th), @i (item i from 0). Insert's last step names
  // what the new items go after (empty: the start) or, @i, before.
  const char* path;
  const char* a;  // Set, Erase: the old value; Insert: the key, or empty
  const char* b;  // Set: the new value; Insert: the value
};

constexpr Edit kEdits[] = {
#include "mech3/patch12_edits.inc"
};

// Ours, on any data (not 1.2's), by the original's records (no new images):
// - MOUSE SENSITIVITY (Owen, 2026-11-02; moved 11-07 from the graphics and
//   audio screen to here): the volumes' slider record under INVERT MOUSE Y,
//   just below the frame (the column's rows fill it), its label spaced as the
//   original's ("I N V E R T   M O U S E") and aligned with the column's.
// - UPGRADES (Owen, 2026-11-07): a button under CONTROLS inside its bracket,
//   GRAPHICS & AUDIO's record, and its page, UPGRADES_DIALOG: the game
//   options' background, ACCEPT and BACK, the credits' frame (option1_lines'
//   nine boxes would stand empty) with the shell's textbox70 under each
//   switch's on/off row (TORSO AUTO RETURN's, its arrows at the box's ends).
constexpr Edit kOurs[] = {
    // - EDITOR (Owen, 2026-11-09; docs/editor.md, The way in): the pause's
    //   button under OPTIONS, in the buttons' zig-zag (84 down, RESUME's and
    //   REPLAY's column), RESUME's record; bound only in a mission the MODS
    //   screen launched (pause_menu.cpp).
    {"zbd/reader.zbd", "dlgpause.zrd", Insert, R"(PAUSEDIALOG/BUTTONS/OPTIONS)", R"("EDITOR")",
     R"(("POSITION" (24 352) "BITMAP" ("circlebtn_small") "LABEL" ("E D I T O R" "offset" (47 35) "font" ("main_red")) "ROLLOVER" ("SOUND" ("menu_rollover" 1.0)) "ACTIVATE" ("SOUND" ("menu_select" 1.0))))"},
    {"zbd/reader.zbd", "dlgoptions.zrd", Insert, R"(GAME_OPTION_DIALOG/BUTTONS/CMD_INVERT_MOUSE_Y)", R"("MOUSE_SENSITIVITY")",
     R"(("POSITION" (250 352) "BITMAP" ("sliderunfilled") "LABEL" ("M O U S E   S E N S I T I V I T Y" "offset" (-185 1) "font" ("ia_dark_grey")) "ROLLOVER" () "ACTIVATE" ("SOUND" ("menu_select" 1.0)) "FILLBITMAP" ("sliderfilled")))"},
    {"zbd/reader.zbd", "dlgoptions.zrd", Insert, R"(GAME_OPTION_DIALOG/BUTTONS/CONTROLS_CFG)", R"("UPGRADES")",
     R"(("POSITION" (400 160) "BITMAP" ("circlebtn_tiny") "LABEL" ("U P G R A D E S" "offset" (22 8) "font" ("ia_small_green")) "RADIUS" (10) "MAXRADIUS" (100) "ROLLOVER" ("SOUND" ("menu_rollover")) "ACTIVATE" ("SOUND" ("menu_select"))))"},
    {"zbd/reader.zbd", "dlgoptions.zrd", Insert, R"(GFX_AUDIO_OPTIONDIALOG)", R"("UPGRADES_DIALOG")",
     R"(("IMAGE_PATH" ("..\data\common\images\dialogs\options;..\data\common\images\dialogs\credits;..\data\common\images\dialogs\shared") "PRIMITIVES" ("BACKGROUND" ("BITMAP" ("common_background") "POSITION" (0 0)) "LINES" ("BITMAP" ("pilot_lines") "POSITION" (443 326)) "LINES1" ("BITMAP" ("credit_lines") "POSITION" (-97 18)) "BOX1" ("BITMAP" ("textbox70") "POSITION" (229 64)) "BOX2" ("BITMAP" ("textbox70") "POSITION" (229 94)) "BOX3" ("BITMAP" ("textbox70") "POSITION" (229 124)) "BOX4" ("BITMAP" ("textbox70") "POSITION" (229 154)) "BOX5" ("BITMAP" ("textbox70") "POSITION" (229 184)) "BOX6" ("BITMAP" ("textbox70") "POSITION" (229 214))) "BUTTONS" ("ACCEPT" ("POSITION" (384 279) "BITMAP" ("circlebtn_small") "LABEL" ("MSG_ACCEPT_BTN" "offset" (47 35) "font" ("main_red")) "ROLLOVER" ("SOUND" ("menu_rollover")) "ACTIVATE" ("SOUND" ("menu_select"))) "BACK" ("POSITION" (452 366) "BITMAP" ("circlebtn_small") "LABEL" ("MSG_BACK_BTN" "offset" (47 35) "font" ("main_red")) "ROLLOVER" ("SOUND" ("menu_rollover")) "ACTIVATE" ("SOUND" ("menu_select"))) "WATER" ("POSITION" (232 64) "LABEL" (("" "offset" (11 5) "font" ("ia_small_green")) ("W A T E R   M O V E M E N T" "offset" (-167 5) "font" ("ia_dark_grey")) ("U P G R A D E S" "offset" (-162 -40) "font" ("main_red"))) "BITMAP" () "ROLLOVER" () "ACTIVATE" ("SOUND" ("menu_select")) "TOGGLE_LEFT" ("POSITION" (-2 4) "BITMAP" ("arrowleft") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowleft_r")) "ACTIVATE" ("SOUND" ("box_rollover") "BITMAP" ("arrowleft"))) "TOGGLE_RIGHT" ("POSITION" (76 4) "BITMAP" ("arrowright") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowright_r")) "ACTIVATE" ("SOUND" ("box_rollover") "BITMAP" ("arrowright")))) "MFB_REGROUP" ("POSITION" (232 94) "LABEL" (("" "offset" (11 5) "font" ("ia_small_green")) ("M F B   R E G R O U P" "offset" (-167 5) "font" ("ia_dark_grey"))) "BITMAP" () "ROLLOVER" () "ACTIVATE" ("SOUND" ("menu_select")) "TOGGLE_LEFT" ("POSITION" (-2 4) "BITMAP" ("arrowleft") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowleft_r")) "ACTIVATE" ("SOUND" ("box_rollover") "BITMAP" ("arrowleft"))) "TOGGLE_RIGHT" ("POSITION" (76 4) "BITMAP" ("arrowright") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowright_r")) "ACTIVATE" ("SOUND" ("box_rollover") "BITMAP" ("arrowright")))) "IMPROVED_AI" ("POSITION" (232 124) "LABEL" (("" "offset" (11 5) "font" ("ia_small_green")) ("I M P R O V E D   A I" "offset" (-167 5) "font" ("ia_dark_grey"))) "BITMAP" () "ROLLOVER" () "ACTIVATE" ("SOUND" ("menu_select")) "TOGGLE_LEFT" ("POSITION" (-2 4) "BITMAP" ("arrowleft") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowleft_r")) "ACTIVATE" ("SOUND" ("box_rollover") "BITMAP" ("arrowleft"))) "TOGGLE_RIGHT" ("POSITION" (76 4) "BITMAP" ("arrowright") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowright_r")) "ACTIVATE" ("SOUND" ("box_rollover") "BITMAP" ("arrowright")))) "ARTEMIS" ("POSITION" (232 154) "LABEL" (("" "offset" (11 5) "font" ("ia_small_green")) ("A R T E M I S   T R A C K I N G" "offset" (-167 5) "font" ("ia_dark_grey"))) "BITMAP" () "ROLLOVER" () "ACTIVATE" ("SOUND" ("menu_select")) "TOGGLE_LEFT" ("POSITION" (-2 4) "BITMAP" ("arrowleft") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowleft_r")) "ACTIVATE" ("SOUND" ("box_rollover") "BITMAP" ("arrowleft"))) "TOGGLE_RIGHT" ("POSITION" (76 4) "BITMAP" ("arrowright") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowright_r")) "ACTIVATE" ("SOUND" ("box_rollover") "BITMAP" ("arrowright")))) "SMALL_FIXES" ("POSITION" (232 184) "LABEL" (("" "offset" (11 5) "font" ("ia_small_green")) ("S M A L L   F I X E S" "offset" (-167 5) "font" ("ia_dark_grey"))) "BITMAP" () "ROLLOVER" () "ACTIVATE" ("SOUND" ("menu_select")) "TOGGLE_LEFT" ("POSITION" (-2 4) "BITMAP" ("arrowleft") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowleft_r")) "ACTIVATE" ("SOUND" ("box_rollover") "BITMAP" ("arrowleft"))) "TOGGLE_RIGHT" ("POSITION" (76 4) "BITMAP" ("arrowright") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowright_r")) "ACTIVATE" ("SOUND" ("box_rollover") "BITMAP" ("arrowright")))) "WIDE_MENUS" ("POSITION" (232 214) "LABEL" (("" "offset" (11 5) "font" ("ia_small_green")) ("W I D E   M E N U S" "offset" (-167 5) "font" ("ia_dark_grey"))) "BITMAP" () "ROLLOVER" () "ACTIVATE" ("SOUND" ("menu_select")) "TOGGLE_LEFT" ("POSITION" (-2 4) "BITMAP" ("arrowleft") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowleft_r")) "ACTIVATE" ("SOUND" ("box_rollover") "BITMAP" ("arrowleft"))) "TOGGLE_RIGHT" ("POSITION" (76 4) "BITMAP" ("arrowright") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowright_r")) "ACTIVATE" ("SOUND" ("box_rollover") "BITMAP" ("arrowright")))) "SMALL_LIST" ("BITMAP" () "POSITION" (336 52) "NORMAL_FONT" ("ia_dark_grey") "SELECTED_FONT" ("ia_dark_grey") "SPACING" (0 14) "OFFSET" (26 2) "SIZE" (14) "LABEL" ("S M A L L   F I X E S" "offset" (0 -18) "font" ("main_red")) "ACTIVATE" () "ROLLOVER" () "UP" ("POSITION" (-2 -2) "BITMAP" ("arrowupmp_normal") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowupmp_rollover")) "ACTIVATE" ("SOUND" ("box_rollover")) "DISABLE" ("BITMAP" ("arrowupmp_d"))) "DOWN" ("POSITION" (-2 200) "BITMAP" ("arrowdnmp_normal") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowdnmp_rollover")) "ACTIVATE" ("SOUND" ("box_rollover")) "DISABLE" ("BITMAP" ("arrowdnmp_d"))))) "CURSOR" ("BITMAP" ("cursor_circle_orange" "CENTER" (1) "CAPTURE" (1)))))"},
    // - MODS (Owen, 2026-11-08; docs/modding.md, the MODS screen): a main menu
    //   button after QUIT, TRAINING's column and OPTIONS' line, and its screen
    //   MODSDIALOG on the pilot screen's look: CAMPAIGNS in a copy of the
    //   games' box at the left (mods_menu.cpp, campaign_lines), MODS in the
    //   games' box, both the games' list, ON/OFF, UP and DOWN under MODS,
    //   ENTER and BACK in ACCEPT's and CANCEL's places, EDITOR in RENAME's,
    //   IMPORT (ADD MOD before), EXPORT, ADD FOLDER and MODS FOLDER beside it
    //   (mod_install.h, upgrades/mods/mod_export.h), REPORT and TEST under
    //   CAMPAIGNS' box (mod_report.h; modding.md, Quick testing).
    {"zbd/reader.zbd", "dialog.zrd", Insert, R"(MAINMENU/BUTTONS/QUIT)", R"("MODS")",
     R"(("POSITION" (92 395) "LABEL" ("M O D S" "offset" (47 35) "font" ("main_red")) "AUXBITMAP" ("main_line5" 59 47) "BITMAP" ("circlebtn_small") "ROLLOVER" ("SOUND" ("menu_rollover" 1.0)) "ACTIVATE" ("SOUND" ("menu_select" 1.0))))"},
    {"zbd/reader.zbd", "dialog.zrd", Insert, R"(PILOTDIALOG)", R"("MODSDIALOG")",
     R"(("IMAGE_PATH" ("..\data\common\images\dialogs\pilotreg;..\data\common\images\dialogs\shared") "PRIMITIVES" ("BACKGROUND" ("BITMAP" ("pilot_background") "POSITION" (0 0)) "LINES" ("BITMAP" ("pilot_lines") "POSITION" (443 326)) "LINES2" ("BITMAP" ("pilot_bg_lines") "POSITION" (0 0))) "BUTTONS" ("CAMPAIGNLIST" ("BITMAP" () "POSITION" (30 119) "NORMAL_FONT" ("main_darkgreen") "SELECTED_FONT" ("main_green") "SPACING" (0 14) "OFFSET" (26 2) "SIZE" (14) "LABEL" ("C A M P A I G N S" "offset" (0 -18) "font" ("main_red")) "ACTIVATE" ("SOUND" ("menu_select")) "ROLLOVER" () "UP" ("POSITION" (-2 -2) "BITMAP" ("arrowupmp_normal") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowupmp_rollover")) "ACTIVATE" ("SOUND" ("box_rollover")) "DISABLE" ("BITMAP" ("arrowupmp_d"))) "DOWN" ("POSITION" (-2 195) "BITMAP" ("arrowdnmp_normal") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowdnmp_rollover")) "ACTIVATE" ("SOUND" ("box_rollover")) "DISABLE" ("BITMAP" ("arrowdnmp_d")))) "MODLIST" ("BITMAP" () "POSITION" (384 39) "NORMAL_FONT" ("main_darkgreen") "SELECTED_FONT" ("main_green") "SPACING" (0 14) "OFFSET" (26 2) "SIZE" (14) "LABEL" ("M O D S" "offset" (0 -18) "font" ("main_red")) "ACTIVATE" ("SOUND" ("menu_select")) "ROLLOVER" () "UP" ("POSITION" (-2 -2) "BITMAP" ("arrowupmp_normal") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowupmp_rollover")) "ACTIVATE" ("SOUND" ("box_rollover")) "DISABLE" ("BITMAP" ("arrowupmp_d"))) "DOWN" ("POSITION" (-2 195) "BITMAP" ("arrowdnmp_normal") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowdnmp_rollover")) "ACTIVATE" ("SOUND" ("box_rollover")) "DISABLE" ("BITMAP" ("arrowdnmp_d")))) "ENTER" ("POSITION" (384 279) "BITMAP" ("circlebtn_small") "LABEL" ("P L A Y" "offset" (47 35) "font" ("main_red")) "ROLLOVER" ("SOUND" ("menu_rollover" 1.0)) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "BACK" ("POSITION" (452 366) "BITMAP" ("circlebtn_small") "LABEL" ("MSG_BACK_BTN" "offset" (47 35) "font" ("main_red")) "ROLLOVER" ("SOUND" ("menu_rollover" 1.0)) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "TOGGLE" ("POSITION" (384 246) "BITMAP" ("pilot_tab") "LABEL" ("ON/OFF" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("ON/OFF" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("ON/OFF" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "MOVEUP" ("POSITION" (464 246) "BITMAP" ("pilot_tab") "LABEL" ("UP" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("UP" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("UP" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "MOVEDOWN" ("POSITION" (544 246) "BITMAP" ("pilot_tab") "LABEL" ("DOWN" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("DOWN" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("DOWN" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "EDITOR" ("POSITION" (68 438) "BITMAP" ("pilot_tab") "LABEL" ("EDITOR" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("EDITOR" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("EDITOR" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "TESTMOD" ("POSITION" (108 336) "BITMAP" ("pilot_tab") "LABEL" ("TEST" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("TEST" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("TEST" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "ADDMOD" ("POSITION" (148 438) "BITMAP" ("pilot_tab") "LABEL" ("IMPORT" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("IMPORT" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("IMPORT" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "EXPORTMOD" ("POSITION" (228 438) "BITMAP" ("pilot_tab") "LABEL" ("EXPORT" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("EXPORT" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("EXPORT" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "ADDFOLDER" ("POSITION" (308 438) "BITMAP" ("pilot_tab") "LABEL" ("ADD FOLDER" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("ADD FOLDER" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("ADD FOLDER" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "MODSFOLDER" ("POSITION" (388 438) "BITMAP" ("pilot_tab") "LABEL" ("MODS FOLDER" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("MODS FOLDER" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("MODS FOLDER" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "REPORTMOD" ("POSITION" (28 336) "BITMAP" ("pilot_tab") "LABEL" ("REPORT" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("REPORT" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("REPORT" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "DELETEROW" ("POSITION" (28 353) "BITMAP" ("pilot_tab") "LABEL" ("DELETE" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("DELETE" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("DELETE" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "RESETROW" ("POSITION" (28 353) "BITMAP" ("pilot_tab") "LABEL" ("RESET" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("RESET" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("RESET" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0)))) "CURSOR" ("BITMAP" ("cursor_circle_orange" "CENTER" (1) "CAPTURE" (1)))))"},
    // - Instant Action's MODS (Owen, 2026-11-10; docs/mods-campaigns-plan.md,
    //   Instant Action; upgrades/mods/ia_mods_menu.h): a MODS tab on Instant
    //   Action's screen under the scenario's description, and its screen
    //   IAMODSDIALOG, MODSDIALOG's records: the MODS list with ON/OFF, UP and
    //   DOWN, ALL OFF at the top left under the "MODS: 2 on" line, ACCEPT
    //   and BACK; the left box is not drawn (mods_screen_lines).
    {"zbd/reader.zbd", "dialog.zrd", Insert, R"(ACTIONDIALOG/BUTTONS/CANCEL)", R"("IAMODS")",
     R"(("POSITION" (420 172) "BITMAP" ("pilot_tab") "LABEL" ("MODS" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("MODS" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("MODS" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))))"},
    // - Multiplayer's MODS (Owen, 2026-11-11; upgrades/mods/ia_mods.h, kMpEntry): a MODS button on the
    //   multiplayer screen, right of HOST SETUP, FIND GAME's kind; its screen is IAMODSDIALOG.
    {"zbd/reader.zbd", "dialog.zrd", Insert, R"(MULTIPLAYER_DIALOG/BUTTONS/CANCEL_BUTTON)", R"("MPMODS")",
     R"(("POSITION" (522 228) "BITMAP" ("circlebtn_tiny") "LABEL" ("MODS" "offset" (22 8) "font" ("ia_small_green")) "ROLLOVER" ("BITMAP" ("circlebtn_tiny") "LABEL" ("MODS" "offset" (22 8) "font" ("ia_small_green"))) "ACTIVATE" ("BITMAP" ("circlebtn_tiny") "LABEL" ("MODS" "offset" (22 8) "font" ("ia_small_green")))))"},
    {"zbd/reader.zbd", "dialog.zrd", Insert, R"(MODSDIALOG)", R"("IAMODSDIALOG")",
     R"(("IMAGE_PATH" ("..\data\common\images\dialogs\pilotreg;..\data\common\images\dialogs\shared") "PRIMITIVES" ("BACKGROUND" ("BITMAP" ("pilot_background") "POSITION" (0 0)) "LINES" ("BITMAP" ("pilot_lines") "POSITION" (443 326)) "LINES2" ("BITMAP" ("pilot_bg_lines") "POSITION" (0 0))) "BUTTONS" ("MODLIST" ("BITMAP" () "POSITION" (384 39) "NORMAL_FONT" ("main_darkgreen") "SELECTED_FONT" ("main_green") "SPACING" (0 14) "OFFSET" (26 2) "SIZE" (14) "LABEL" ("M O D S" "offset" (0 -18) "font" ("main_red")) "ACTIVATE" ("SOUND" ("menu_select")) "ROLLOVER" () "UP" ("POSITION" (-2 -2) "BITMAP" ("arrowupmp_normal") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowupmp_rollover")) "ACTIVATE" ("SOUND" ("box_rollover")) "DISABLE" ("BITMAP" ("arrowupmp_d"))) "DOWN" ("POSITION" (-2 195) "BITMAP" ("arrowdnmp_normal") "ROLLOVER" ("SOUND" ("arrow_rollover") "BITMAP" ("arrowdnmp_rollover")) "ACTIVATE" ("SOUND" ("box_rollover")) "DISABLE" ("BITMAP" ("arrowdnmp_d")))) "ACCEPT" ("POSITION" (384 279) "BITMAP" ("circlebtn_small") "LABEL" ("MSG_ACCEPT" "offset" (47 35) "font" ("main_red")) "ROLLOVER" ("SOUND" ("menu_rollover" 1.0)) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "BACK" ("POSITION" (452 366) "BITMAP" ("circlebtn_small") "LABEL" ("MSG_BACK_BTN" "offset" (47 35) "font" ("main_red")) "ROLLOVER" ("SOUND" ("menu_rollover" 1.0)) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "TOGGLE" ("POSITION" (384 246) "BITMAP" ("pilot_tab") "LABEL" ("ON/OFF" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("ON/OFF" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("ON/OFF" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "MOVEUP" ("POSITION" (464 246) "BITMAP" ("pilot_tab") "LABEL" ("UP" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("UP" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("UP" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "MOVEDOWN" ("POSITION" (544 246) "BITMAP" ("pilot_tab") "LABEL" ("DOWN" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("DOWN" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("DOWN" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0))) "ALLOFF" ("POSITION" (80 55) "BITMAP" ("pilot_tab") "LABEL" ("ALL OFF" "offset" (9 3) "font" ("main_red_small")) "ROLLOVER" ("BITMAP" ("pilot_tab_h") "LABEL" ("ALL OFF" "offset" (9 3) "font" ("ia_small_green")) "SOUND" ("box_rollover")) "DISABLE" ("LABEL" ("ALL OFF" "offset" (9 3) "font" ("ia_small_grey"))) "ACTIVATE" ("SOUND" ("menu_select" 1.0)))) "CURSOR" ("BITMAP" ("cursor_circle_orange" "CENTER" (1) "CAPTURE" (1)))))"},
};

// A value as the table writes it: (lists), "strings", ints, floats with a '.'.
bool parse_at(const char*& p, Node* out) {
  while (*p == ' ') ++p;
  if (*p == '(') {
    ++p;
    std::vector<Node> items;
    for (;;) {
      while (*p == ' ') ++p;
      if (*p == ')') break;
      Node n{};
      if (!*p || !parse_at(p, &n)) {
        for (Node& i : items) zrdr::free_contents(&i);
        return false;
      }
      items.push_back(n);
    }
    ++p;
    *out = tree::list(std::move(items));
    return true;
  }
  if (*p == '"') {
    const char* end = std::strchr(p + 1, '"');
    if (!end) return false;
    *out = tree::str(std::string(p + 1, end).c_str());
    p = end + 1;
    return true;
  }
  const char* start = p;
  while (*p && *p != ' ' && *p != ')') ++p;
  const std::string t(start, p);
  if (t.empty()) return false;
  *out = t.find('.') != std::string::npos ? tree::real(std::strtof(t.c_str(), nullptr))
                                          : tree::num(static_cast<int32_t>(std::strtol(t.c_str(), nullptr, 10)));
  return true;
}

bool parse(const char* text, Node* out) { return parse_value(text, out); }

std::vector<std::string> split(const char* path) {
  std::vector<std::string> out(1);
  for (const char* c = path; *c; ++c) {
    if (*c == '/') out.emplace_back();
    else out.back() += *c;
  }
  return out;
}

// The slot of step `s` in list `l` (KEY's value, or item @i), or 0. For KEY
// steps `*key_at` gets the key's own slot.
int32_t slot(const Node* l, const std::string& s, int32_t* key_at = nullptr) {
  if (!l || l->type != zrdr::List) return 0;
  const int32_t count = l->list[0].i;
  if (s.size() > 1 && s[0] == '@') {
    const int32_t i = std::atoi(s.c_str() + 1) + 1;
    return i < count ? i : 0;
  }
  const size_t hash = s.find('#');
  const std::string key = s.substr(0, hash);
  int32_t want = hash == std::string::npos ? 1 : std::atoi(s.c_str() + hash + 1);
  for (int32_t i = 1; i + 1 < count; ++i) {
    const Node& n = l->list[i];
    if (n.type == zrdr::String && n.s && key == n.s && --want == 0) {
      if (key_at) *key_at = i;
      return i + 1;
    }
  }
  return 0;
}

Node* walk(Node* l, const std::vector<std::string>& steps, size_t n) {
  for (size_t k = 0; k < n && l; ++k) {
    const int32_t i = slot(l, steps[k]);
    l = i ? &l->list[i] : nullptr;
  }
  return l;
}

// One edit on `top`; false when the tree is not what the edit expects.
bool apply_one(Node* top, const Edit& e) {
  const std::vector<std::string> steps = split(e.path);
  Node* parent = walk(top, steps, steps.size() - 1);
  if (!parent || parent->type != zrdr::List) return false;
  const std::string& last = steps.back();
  Node old{}, now{};
  bool ok = false;
  switch (e.op) {
    case Set: {
      const int32_t i = slot(parent, last);
      if (i && parse(e.a, &old) && tree::equal(parent->list[i], old) && parse(e.b, &now))
        tree::replace(&parent->list[i], now), now = Node{}, ok = true;
      break;
    }
    case Erase: {
      int32_t key = 0;
      const int32_t i = slot(parent, last, &key);
      if (i && parse(e.a, &old) && tree::equal(parent->list[i], old)) {
        if (key) tree::erase(parent, key, 2);
        else tree::erase(parent, i, 1);
        ok = true;
      }
      break;
    }
    case Insert: {
      int32_t at = 1;
      if (!last.empty() && last[0] == '@') {
        at = std::atoi(last.c_str() + 1) + 1;  // before item i; the item count: the end
        if (at > parent->list[0].i) break;
      } else if (!last.empty()) {
        const int32_t i = slot(parent, last);
        if (!i) break;
        at = i + 1;
      }
      std::vector<Node> items;
      Node key{};
      if (*e.a && !parse(e.a, &key)) break;
      if (!parse(e.b, &now)) {
        zrdr::free_contents(&key);
        break;
      }
      if (*e.a) items.push_back(key);
      items.push_back(now), now = Node{};
      tree::insert(parent, at, std::move(items));
      ok = true;
      break;
    }
  }
  zrdr::free_contents(&old), zrdr::free_contents(&now);
  return ok;
}

}  // namespace

bool parse_value(const char* text, Node* out) {
  const char* p = text;
  return parse_at(p, out);
}

bool has_edits(const std::string& archive, const std::string& entry) {
  for (const Edit& e : kEdits)
    if (archive == e.archive && entry == e.entry) return true;
  return false;
}

bool apply_ours(const std::string& archive, const std::string& entry, Node* root) {
  if (!root || root->type != zrdr::List) return false;
  bool any = false;
  for (const Edit& e : kOurs) {
    if (archive != e.archive || entry != e.entry) continue;
    Node copy = tree::clone(*root);  // a record not as expected (a mod's) is left whole
    Node* top = &copy;
    while (top->list[0].i == 2 && top->list[1].type == zrdr::List) top = &top->list[1];
    // Our values' small font is the dialog's own (Pirate's Moon's is ia_small_amber).
    std::string value = e.b;
    const Node* font = walk(top, split("GAME_OPTION_DIALOG/BUTTONS/CONTROLS_CFG/LABEL/font/@0"), 6);
    if (font && font->type == zrdr::String && font->s)
      for (size_t at = 0; (at = value.find("ia_small_green", at)) != std::string::npos; at += std::strlen(font->s))
        value.replace(at, 14, font->s);
    Edit ours = e;
    ours.b = value.c_str();
    if (apply_one(top, ours)) tree::replace(root, copy), any = true;
    else zrdr::free_contents(&copy);
  }
  return any;
}

bool apply_edits(const std::string& archive, const std::string& entry, Node* root) {
  if (!root || root->type != zrdr::List || !has_edits(archive, entry)) return false;
  // On a copy, so an entry that is not 1.0's after all (an edit's old value
  // does not match) is left whole.
  Node copy = tree::clone(*root);
  Node* top = &copy;
  while (top->list[0].i == 2 && top->list[1].type == zrdr::List) top = &top->list[1];
  for (const Edit& e : kEdits) {
    if (archive != e.archive || entry != e.entry) continue;
    if (!apply_one(top, e)) {
      zrdr::free_contents(&copy);
      return false;
    }
  }
  tree::replace(root, copy);
  return true;
}

}  // namespace mech3::patch12
