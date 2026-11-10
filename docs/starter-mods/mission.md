# A single mission: a one-mission campaign

`mw3 --make-starter mission` makes `mods\starter_mission` with three files:

- `mod.txt` names the mod and its campaign:

  ```
  campaign=one_mission.txt
  ```

- `one_mission.txt` is the campaign: its name, the stock you start with,
  one heading, and one mission:

  ```
  name=Starter: One Mission
  stock=default
  op=The Drop bases=3
  mission=c1 1
  ```

  `mission=c1 1` is the game's own first mission (world `c1`, mission 1), its
  briefing and movie as in the game. Add more `mission=` lines to make a
  longer campaign, in the order they are played; `op=` lines group them.
- `about_this_mod.txt` says what it is.

In the game: MODS, CAMPAIGNS, Starter: One Mission, ENTER. The campaign has
its own pilots, kept apart from your others. The campaign file's keys are
in docs/modding.md, Custom campaigns.
