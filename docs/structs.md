# Recovered structures

Mech3.exe 1.2, 2026-10-19. The layouts are in `types.json` (offset, type, name,
note), which `tools/ghidra/ApplyTypes.java` applies to the headless project:

```
python3 tools/analysis/typesfull.py      # types.json + placeholder fields
python3 tools/analysis/fpconsts.py       # float constants in .rdata
analyzeHeadless analysis-output/ghidra-project mech3 -process Mech3.exe -noanalysis \
  -scriptPath tools/ghidra -postScript ApplyTypes.java analysis-output/types.full.json \
  docs/symbols.csv analysis-output/decomp-typed analysis-output/fpconsts.txt
```

It takes about ten seconds. It creates the structs, names the functions in
`symbols.csv` (a `Class::` prefix puts them in that class), makes each class's
methods `__thiscall` so `this` is typed, applies the signatures in `types.json`,
and writes `analysis-output/decomp-typed/<addr>.c` for every method. Placeholder
fields (`f_554` float, `u_5cc` dword) come from `tools/analysis/fields.py`, which
reads how the methods use `this`; they only make Ghidra's output easier to read.

The exe has no RTTI and almost no class names in its strings, so class names are
ours, picked from what the code does and the source file around it. `Node` is
the GameZ name (`gwNode*`, `gClsReadNode` in error messages).

## The mech

Built by `operator new(0x6d8)` and the constructor at 0x4149f0 (from 0x4140ec).
Each constructor calls its base's, then stores its own vtable:

| Class | Constructor | Vtable | Size | Source area |
|---|---|---|---|---|
| ZEntity | 0x5a8310 | 0x5d0ef0 | 0x18 | between twod_wintext.cpp and zent_init.cpp (zEntity) |
| ZEntity3d | 0x5a7e10 | 0x5d0ea8 | 0x68 | same |
| AiTarget | 0x4d56e0 | 0x5cced8 | 0xd4 | between zaitarget.cpp and zrdrainet.cpp |
| Vehicle | 0x4aeb00 | 0x5c8d48 | 0xdc | between veh_basic.cpp and weapon.cpp |
| Mech | 0x4149f0 | 0x5bcef8 | 0x6d8 | mech.cpp and after |

ZEntity holds the world node (+4) and two more nodes; ZEntity3d adds position,
velocity and orientation; AiTarget adds the height (+0x68, the mech's from its
model's bounding box) and three bit masks; Vehicle the weapons (+0xd4).

Mech vtable entries that differ from Vehicle's are the mech's own methods
(`tools/analysis/pe.py vt 0x5bcef8 64 0x5c8d48`). Two used by heat: +0x5c is
0x40bae0 (returns 0), +0xa0 is `IsPoweredDown` (0x41cc10).

### Parts

Seven part pointers at +0x38c (`Mech::Part`, 0x41d510, maps part ids):

| Offset | Part | Class (constructor, size) |
|---|---|---|
| +0x38c, +0x390 | left, right leg | Leg (0x4635e0, 0xd4), with calf +0x74 and foot +0x78 (0x462590, 0xec) |
| +0x394, +0x398 | left, right arm | 0x466280, 0xd0; hand at +0x74 |
| +0x39c | torso (also ltorso, rtorso) | 0x464000, 0x1b4 |
| +0x3a0 | hip | 0x4620a0 |
| +0x3a4 | head | 0x4671a0, 0x74 |

Part ids (0x484cc0): 1 torso, 2 larm, 3 rarm, 4 lthigh, 5 rthigh, 6 head,
7 ltorso, 8 rtorso, 9 lhand, 10 rhand, 11 lcalf, 12 rcalf, 13 lfoot, 14 rfoot,
15 hip. All parts share a base (0x45fbe0) with the model node at +8.

The foot's ground update (0x462a10) asks the world for water under the foot
(0x475ea0: in water, surface height), sets the depth, and when the foot is on
the ground reads the soil from the ground polygon's material through the table
at 0x608c00 (filled at 0x47f400 from the soil names).

## The mech definition (`dfn_<mech>.zrd`)

`MechDfn`, 0x1a8 bytes, built at 0x46b7a0 and read at 0x46ba40 from the
`dfn_*.zrd` files in `reader.zbd`. The reader requires `version` 2. The keys and
where they land come from `tools/analysis/rdrkeys.py 0x46ba40`; angles are
converted to radians as they are read. `configurations`, `weapons`, `armor` and
`equipment` are walked into separate objects, not ported yet.

## The scene node

`Node`, 0xd0 bytes, the zClass node every model, camera, light and world is made
of. The layout is mech3ax's (`crates/nodes/src/mw/node.rs`, as read from the
`.zbd` files), and the exe agrees where checked: the foot constructor walks
`child_count` (+0x5c) and `children` (+0x60) comparing names (+0), and reads the
node's bounding box at +0x74.

## Not done yet

- Weapons: `weapons.zrd` is read in `zweapon/zwep_init.c` (45 keys,
  `rdrkeys.py zwep_init`); the weapon object and the ammo bins at Vehicle +0xd4.
- The pilot/HUD object at Mech +0x684 and the controls object at +0x37c.
- Most of the mech: about 170 offsets have only placeholder names.
