# Work log: mw3 dev

The single worker session from 2026-10-25 (Owen), formerly "mw3 mac 4".
Its earlier log, the AI items and everything up to HUD plan item 1, is
`ai.md`; read its last sections for the state at the switch. The protocol
is in `README.md`.

## Queue at the switch (2026-10-25)

- Item 19, the weather: rain (c3, c4, t1) and snow (c4b m4 / m6), the
  manager's rect and tick (0x5a6790 / 0x5a67c0, renamed weather_rect /
  weather_tick), app_mission's create_rain / create_snow / weather_add.
  Rain first, snow second if it needs another sitting.
- Then `hud-plan.md` items 6-9: 0x40fac0 as one call, the mission maps
  (maps.zrd), the radar's map mode (Shift+R), the radar's polylines.
- Later: the front-end screens (campaign, Instant Action, the mech lab,
  briefing and debriefing, salvage, movies), planned the same way.
</content>
</invoke>
<invoke name="Bash">
<parameter name="command">cd /Volumes/Callisto/Projects/mw3-re; git add CLAUDE.md docs/worklogs/README.md docs/worklogs/dev.md && git diff --cached --stat && git -c user.email=telltaleatheist@users.noreply.github.com commit -q -m "Sessions renamed: mw3 lead and mw3 dev, one worker for every area; the dev's work log" && git push -q && git log --oneline -1