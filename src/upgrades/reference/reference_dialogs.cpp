// The reference's registry for the dialogs (dialog.zrd, the dlg*.zrd files,
// briefing.zrd): zTwoD's dialog reader (gamez/ztwod: Dialog::load 0x0059f010,
// the objects, controls, lists and text) and each screen's own reads
// (mech3/menu_widgets.cpp, mainmenu.cpp, briefing.cpp, credits.cpp,
// lab_screen.cpp, action_menu.cpp; docs/twod.md, menus.md).
#include "upgrades/reference/reference.h"

namespace upgrades::reference {

File dialogs() {
  const char* D = "a dialog";
  const char* O = "a button or picture";
  const char* L = "a list";
  const char* B = "the briefing";
  return {"dialog", "The dialogs (dialog.zrd, dlg*.zrd, briefing.zrd)", "`zbd\\reader.zbd`",
          "zTwoD's dialog reader (`Dialog::load` 0x0059f010) and each screen's binds (docs/twod.md, menus.md)",
          "One record per dialog by the name its screen asks for (`MAINMENU`, `GAME_OPTION_DIALOG`, `MESSAGEBOX`, ...). "
          "`PRIMITIVES` and `BUTTONS` hold the dialog's pictures and controls by name; a screen binds the names it knows "
          "(an unknown name is never shown, a missing one is reported and left out). The keys below are read by every "
          "screen's binds; this list is what the menus the check opens read (every main menu screen, the options' pages, "
          "the briefing, the message box).",
          {
              {D, "IMAGE_PATH", "text: folders, `;` between", "none", "", "Where its pictures are found."},
              {D, "SHARED_IMAGE_PATH", "text", "none", "", "More folders, shared with other dialogs."},
              {D, "PRIMITIVES", "`NAME ( ... )` records", "none", "", "Its pictures (backgrounds, lines, boxes) by name."},
              {D, "BUTTONS", "`NAME ( ... )` records", "none", "", "Its controls by name: the screen's buttons, lists, sliders."},
              {D, "CURSOR", "a block", "none", "", "Its mouse cursor: `BITMAP`, `CENTER`, `CAPTURE`."},
              {"CURSOR", "CAPTURE", "int", "0", "", "Keeps the cursor inside the dialog."},
              {D, "FONTS", "a block", "none", "", "Fonts the dialog names."},
              {D, "BACKGROUND_IMAGES", "list", "none", "", "Pictures shown behind it."},
              {D, "BACKGROUND_SOUNDS", "list", "none", "", "Sounds played while it shows."},
              {D, "BACKGROUND_TEXT", "list", "none", "", "Text shown behind it."},
              {D, "BACKGROUND_VIDEOS", "list", "none", "", "Videos shown behind it."},
              {D, "MOUSETRAP", "list", "none", "", "A rectangle the mouse is held in."},
              {O, "POSITION", "list: x y", "0 0", "", "Where it sits (640 x 480)."},
              {O, "BITMAP", "a picture's name", "none", "", "Its picture."},
              {O, "AUXBITMAP", "a picture's name", "none", "the main menu's", "A second picture (the main menu's glow)."},
              {O, "CENTER", "flag", "0", "", "Its position is its centre."},
              {O, "SIZE", "list: w h", "the picture's", "", "Its size."},
              {O, "RECT", "list: x y w h", "none", "", "Its area."},
              {O, "LABEL", "text, or (text offset font) lists", "none", "", "Its words, their offset and font."},
              {O, "TEXT", "a message key", "none", "", "Its text."},
              {O, "FONT", "a font's name", "the dialog's", "", "Its font."},
              {O, "OFFSET", "list: x y", "0 0", "", "Its label's offset."},
              {O, "ROLLOVER", "a block", "none", "", "What changes under the mouse: picture, label, `SOUND`, `FLASH`."},
              {O, "ACTIVATE", "a block", "none", "", "What a press does: `SOUND`, picture."},
              {O, "DISABLE", "a block", "none", "", "How it looks when off."},
              {O, "DISABLE_SEL", "a block", "none", "", "How a check box looks when off and checked."},
              {O, "DISABLE_UNSEL", "a block", "none", "", "How a check box looks when off and not checked."},
              {O, "CHECKED", "a block", "none", "", "How a check box looks when checked."},
              {O, "FLASH", "list", "none", "", "A blinking label under the mouse."},
              {O, "SOUND", "a sound", "none", "", "Played (under the mouse, on a press)."},
              {O, "GLOW_OFFSET", "list: x y", "0 0", "the main menu's", "Where its glow sits."},
              {O, "HELP", "a message key", "none", "", "Its help line."},
              {O, "RADIUS", "int", "the default", "", "A round button's size."},
              {O, "MAXRADIUS", "int", "the default", "", "How far its glow grows."},
              {O, "FILLBITMAP", "a picture's name", "none", "a slider's", "A slider's filled part."},
              {O, "WORDWRAP", "list", "none", "", "Text wrapped at a width."},
              {O, "UP", "a block", "none", "a list's", "Its scroll-up arrow."},
              {O, "DOWN", "a block", "none", "a list's", "Its scroll-down arrow."},
              {L, "NORMAL_FONT", "a font's name", "the dialog's", "", "Its rows' font."},
              {L, "SELECTED_FONT", "a font's name", "NORMAL_FONT", "", "The chosen row's font."},
              {L, "STATIC_FONT", "a font's name", "none", "the mech lab's", "Rows that cannot be chosen."},
              {L, "LIST_FONT", "a font's name", "the dialog's", "", "A drop list's font."},
              {L, "SPACING", "list: x y", "0 0", "", "The space between rows."},
              {L, "LISTSIZE", "int", "the default", "", "Rows shown."},
              {L, "LISTWIDTH", "int", "the default", "", "Its width."},
              {L, "LIST_OFFSET", "list: x y", "0 0", "", "Where its rows start."},
              {L, "ACTIVE_RECT", "list: x y w h", "none", "", "The area that takes clicks."},
              {L, "ITEMROLLOVER", "a block", "none", "", "A row under the mouse."},
              {L, "BAR", "a block", "none", "", "Its bar."},
              {L, "ROLLOVER_SOUND", "a sound", "none", "", "Played over a row."},
              {L, "SELECT_SOUND", "a sound", "none", "", "Played on choosing a row."},
              {"the controls", "OPTIONTEXT", "a picture or text", "none", "", "The controls screen's description line."},
              {"the credits", "SCROLLING_TEXT", "a block", "none", "", "The credits' text."},
              {"the credits", "SCROLL_RATE", "float", "the default", "", "How fast the credits scroll."},
              {"the message box", "MB_YES", "a button", "not shown", "", "Its YES button."},
              {"the message box", "MB_NO", "a button", "not shown", "", "Its NO button."},
              {"the message box", "MB_ABORT", "a button", "not shown", "", "Its ABORT button."},
              {"the message box", "MB_RETRY", "a button", "not shown", "", "Its RETRY button."},
              {"the message box", "MB_IGNORE", "a button", "not shown", "", "Its IGNORE button."},
              {"the multiplayer screen", "CALL_BUTTON", "a button", "not shown", "", "Its CALL button."},
              {B, "MAP", "a block", "none", "", "The briefing's map."},
              {B, "MARKERS", "list", "none", "", "The map's markers, with the briefing's sounds."},
              {B, "at", "number (s)", "0", "", "When a briefing step starts."},
              {B, "image", "a picture's name", "none", "", "A briefing step's picture."},
              {B, "string", "a message key", "none", "", "A briefing step's text."},
              {B, "label", "a message key", "none", "", "A marker's label."},
              {B, "sound", "a sound", "none", "", "A briefing step's sound."},
              {B, "color", "list: r g b", "white", "", "A marker's colour."},
              {B, "radius", "list: two ints", "none", "", "A marker's circle."},
              {B, "points", "list of points", "none", "", "A marker's path."},
              {B, "offset", "list: x y", "0 0", "", "A label's offset."},
              {B, "font", "a font's name", "the dialog's", "", "A step's font."},
              {"the action menu", "inactive_font", "a font's name", "none", "", "A choice that is off."},
          },
          ";any:BUTTONS;any:PRIMITIVES"};
}

}  // namespace upgrades::reference
