# The program's handles (ours; `GameEngine::handle_of`, app_engine.cpp)

The ported game code is written against the exe's 32-bit pointers. Where the
program (app_*.cpp) hands one of its own objects to that code, it gives a
32-bit **handle** instead: `handle_of(p)` numbers a pointer the first time it
is asked for (`0x00100000 +` its place in `objects_`), `object_of(h)` gives
the pointer back, and `forget_object(p)` drops the pair so the handle gives
null from then on.

The table is keyed by the address. So an object freed **without** being
forgotten leaves its handle behind, and the next object the allocator makes
at that address answers to the old number: a stale handle then reaches the
new object, and a new object's handle number depends on what the heap reused
(runs differed by allocation luck: the entities' sort breaks ties on it). The
exe's raw pointers have the same hazard, without the numbering. Two ways keep
the table true:

- **forget at the free**: `forget_object` beside the `delete`;
- **forget at the next `new`**: an `on_new` hook on the code that allocates,
  which forgets whatever the new address still carried.

A handle held past its object's free then gives null (better than the exe's
pointer into freed memory) instead of another object.

## Each kind (checked 2026-11-10)

| Kind given a handle | At its free | At the next `new` there | State |
|---|---|---|---|
| Mechs | `mech_free`, the wreck's delete (app_mech.cpp) | the mech build's `on_new` | covered |
| A mech's weapon manager and its weapons | the mech's free calls' `forget`, before 0x004b01d0's delete (mech_build.cpp) | - | covered since 2026-11-10 |
| A vehicle's weapon manager and its weapons | `WiredVehicles::weapons_free` (app_vehicle.cpp) | - | covered since 2026-11-10 |
| A weapon manager's groups (Pirate's Moon: its AI's firing code asks for the selected group's handle, `zai_wire_firing_pm`) | with the manager's, at the same two places | - | covered since 2026-11-10 (pm) |
| Vehicles, the stealth object, field bases | `program_vehicle_free` | their hooks' `on_new` | covered |
| Turrets | `program_turret_free` | - | covered |
| The player's pocket marker | at the next one's making (app_pocket.cpp) | - | covered |
| AI controls and their brain's params | at the controls' delete (app_mission.cpp) | the brain's `on_new` | covered |
| The pilot's HUD | `hud_free` (app_mech.cpp) | - | covered since 2026-11-10 |
| HUD markers (`ztwod::Image`) | `marker_delete` | - | covered |
| Movie streams | app_fmv.cpp | - | covered |
| Reader nodes (`zrdr::load`) | `reader_free` (app_mission.cpp) | - | covered since 2026-11-10 |
| The four objective groups | the `objectives_free` hook, before the reset | - | covered since 2026-11-10 |
| The mission waynet, mech data | - | their `on_new` | covered |
| The mission map made at the launch (`new MissionMap`) | never freed (kept for the program's life) | - | no hazard |
| World nodes (zClass) | - | - | pool slots: a handle names the slot, as the exe's pointer does; the same as the exe |
| Playing sounds (`zsnd::play`) | - | - | the voices' pool, reused by design; the same as the exe |
| Sound objects (`zsnd::find`), anim defs (`zeff::anim_find`) | not forgotten | - | freed with the mission's bank and animations; handles to them are used at once or kept in a state's field the exe keeps as a pointer too: the same as the exe. Listed, not fixed |
| Mech definitions, their skins and loadouts (`mech_dfn`, configs) | not forgotten | - | freed by `free_dfns` (a remount, the program's end); the lab's screens hold their handles only while open. Listed, not fixed |
| The pilot's games (the campaign's and, on Pirate's Moon, the Pirate Missions' `StandaloneGame`) | not forgotten | - | freed with the pilot, at the pilot screen; the launch asks for the handle again at each use (`lance_game`, `standalone`) and keeps none past it. Listed, not fixed |
| A mech's parts and its legs' calves (the damage hooks; on Pirate's Moon the AI's firing code too) | not forgotten | - | freed with the mech; asked for again at each use. Listed, not fixed (main's table does not list them yet) |
| AI nets (`znet::find`) | not forgotten | - | rebuilt at each mission; handles are asked for again at each use. Listed, not fixed |

Tests: `mission_c1m1_freed_objects_handles_name_nothing_headless` (the
player's weapon manager and current weapon: after the mission's end their
handles give null, and the address asked again gets a new number).

A new kind of object given a handle needs one of the two ways above where it
is freed or made.
