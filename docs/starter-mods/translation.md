# A translation: the main menu in German

`mw3 --make-starter translation` makes `mods\starter_translation` with three
files:

- `mod.txt` names the mod.
- `strings.txt` replaces the main menu's button labels:

  ```
  # MSG_CAMPAIGN_BTN
  266=K A M P A G N E
  ```

  Each line is a message's number, `=`, and the text to show instead. The
  line above each, starting with `#`, is a comment naming the message. The
  numbers come from your game's `Mech3Msg.dll` when the mod is made (the two
  games number their messages apart). The game's own labels are spaced
  capitals, so these are too.
- `about_this_mod.txt` says what it is.

To translate more, add lines: mech3ax lists every message with its number
and name, and the name itself works in place of the number while the game
runs. `\n` breaks a line. Letters the game's font lacks (outside
Windows-1252) leave that line out, said in the log.
