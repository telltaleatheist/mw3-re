# Campaign Missions: a design sketch for Owen (2026-11-10)

A modding dialog for a custom campaign's mission list (modding.md, Custom
campaigns), in the form of editor-mech-variant.md. For Owen to pick from;
no code until he does.

What it writes is the campaign file the game already reads: `campaign=` in
mod.txt naming a text file of lines (`name=`, `stock=`, `movie_start=`,
`movie_end=`, `op=<heading> [bases=N]`, `mission=<world> <n> [briefing]
[movie] [bases=N]`), played in order, each line checked at the mount.

## Where it opens

Two ways, Owen's pick:
- **The MODS screen**, a CAMPAIGN tab beside EDITOR and TEST, on the chosen
  mod or custom campaign: no mission needs to be running, as the file is
  only a list. (Proposed.)
- Or a row in the editor's menu, as EDIT WEAPONS.

## The form

```
 Campaign Missions                         mod: Lost Frontier
 ---------------------------------------------------------------
 campaign       [ Lost Frontier                   ]   name=
 file           [ lost.txt                        ]   its pilots' folder
 stock          [ default  v ]
 opening movie  [ campaign v ]      ending movie  [ c4win    v ]
 ---------------------------------------------------------------
     heading / mission        briefing        movie     bases
  1  Operation 1                                         3
  2    c1 mission 1           brief_c11       c1m1
  3    c1 mission 2           (as the game)   -
  4    mymod_w1 mission 1     (as the game)   -
  5  Operation 2                                         3
  6    c4 mission 4           (as the game)   c4m4
                [ add mission ] [ add heading ] [ remove ]
                [ earlier ] [ later ]
 world          [ c1       v ]   mission [ 2 v ]   (the world's missions)
 briefing       [ (as the game) v ]   movie [ -  v ]   bases [ - v ]
 ---------------------------------------------------------------
 [ Play first ]  [ OK ]  [ Cancel ]
 the line that would not read, if one did not
```

- **world** lists the game's worlds and every enabled mod's `world=`;
  **mission** lists that world's `readerm<N>.zbd` (the game's, the mod's),
  as TEST's list finds them.
- **briefing** lists briefing.zrd's blocks (`brief_c11` ...; "as the
  game" leaves the line's briefing out, so the game's own for that mission
  is used); **movie** lists fmv.zrd's names, or none.
- **bases** sets the field bases at that mission or heading (0 to 3), or
  carries on.
- **Play first** starts the first mission with the campaign entered, as the
  MODS screen's ENTER does, after writing.

OK checks every line as the mount does (a world neither the mod's nor the
game's, a mission number its world lacks, a name taken by another campaign
of the mod) and shows the first refusal on the last line; nothing is
written then.

## What OK writes

- The campaign file (`lost.txt`): the lines above in order, a comment line
  at the top saying the form wrote it, the old file kept as .bak. Lines the
  form does not know (comments the modder wrote) are kept above the first
  `op=` (proposed).
- **mod.txt**: a `campaign=<file>` line, added if the mod has none for that
  file; the old mod.txt kept as .bak.

The game reads campaigns at the mount, so it plays from the next start
(whether the MODS screen shows a changed file before then is to check).

## Names and briefing words

- The **campaign's name** and the **headings** (`op=`) are free text; a
  mission's name in the game is its heading and place ("Operation 2,
  Mission 1"), as the campaign game makes it.
- A mission's **briefing** is a briefing.zrd block: a map image, a spoken
  voice (`briefing_c1m1`, a recording in the sounds archives) and markers
  whose labels are message names (`MSG_BRF_START`). The form can pick any
  existing block. New words mean new message lines (strings.txt, by number)
  and a recording; a block of the modder's own is a copy of briefing.zrd's
  block with its labels changed, written as a loose briefing.zrd text, which
  is a later sitting.

## What it cannot do yet

- **New briefings** (above): picking existing ones only.
- **New missions' contents** (enemies, objectives, the map): those are the
  world's readerm files, made with the editor's AI net and vehicle forms or
  mech3ax; the list only orders missions that exist.
- **Stock**: `default` (the original's new-campaign stock) is the only one
  the game knows for now.
- **Pirate's Moon's campaign**: PM numbers its missions across its worlds;
  its own campaign format, where it differs, comes with the PM side (the
  list form is the same).

## For Owen to pick

1. Where it opens: a CAMPAIGN tab on the MODS screen (proposed) or an
   editor row.
2. Whether the first sitting edits an existing campaign file only, or also
   makes a new one (a new file and its campaign= line).
3. Briefings: pick existing blocks only (sketched), or a first copy-and-
   relabel of a block in the same sitting.
4. Play first: in the first sitting or later.
