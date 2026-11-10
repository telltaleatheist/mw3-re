# Starter mods

Four small example mods, one of each kind, to copy and change. The rebuild
makes each one from your own copy of the game, so nothing of the game's
comes with the rebuild or with this repository.

Make one (it goes into the `mods` folder of your settings folder, the same
place the MODS screen lists):

```
mw3 --make-starter mech
mw3 --make-starter weapon
mw3 --make-starter translation
mw3 --make-starter mission
```

For Pirate's Moon use `mw3pm` instead of `mw3`. Add `--install <your game
folder>` and `--write-dir <your settings folder>` if the rebuild has not been
told where they are yet.

Each makes a folder `starter_<kind>`. If that folder is there already, it is
left alone and nothing is made: rename or remove it yourself first. The
rebuild never writes over a mod.

Then: MODS, choose the mod, ON/OFF, restart, and TEST to play a mission with
it straight away (Quick testing in docs/modding.md). REPORT says in plain
words what the mod changes; each starter's report says nothing is refused.

- [A mech variant](mech.md): Thor B
- [A weapon](weapon.md): ER PPC B
- [A translation](translation.md): the main menu in German
- [A single mission](mission.md): a one-mission campaign
