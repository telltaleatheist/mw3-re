# The game editor: a guide

<!-- Generated from src/upgrades/editor/help*.cpp (the editor's tooltips): build/mw3 --editor-guide docs/editor-guide.md. Do not edit by hand; the test editor_guide_is_current fails when it is stale. -->

MechWarrior 3 shipped with the developers' own editor still inside: a menu over the running mission for the AI's paths (nets) and the vehicles' AI. The rebuild opens it to players and modders, makes the parts the release left empty work, and saves what you change into a mod. The menus are the original's; the dialogs are windows of the rebuild's own. Every field, button and row in the editor has a tip (hold the mouse over it); this guide gathers them. How it works inside: docs/editor.md.

## Quick start

1. Open the editor from the MODS screen: pick a mod or custom campaign, then EDITOR and a mission, and it opens with the editor up. A custom campaign played from MODS (ENTER) has it in every mission too: EDITOR in the pause menu, or Shift+F12. Its saves go into that mod. The game's own campaign, Pirate Missions, Instant Action, training and multiplayer have no editor.
2. The menu is on the left; point at a row and click (or press Space). The view is a free camera: fly it with the movement keys.
3. EDIT AI NETS: pick a net. Its nodes and links are drawn in the world. Click a node to pick it; picking a second one links the first to it. Move Node, Delete Node, Node Params and Add veh to node work on the first picked node.
4. EDIT VEHICLES: pick a vehicle, change its pages, then Apply (the running mission takes it at once) and Save (it is written for the next start).
5. Save Changes (in the net menu) writes the nets. Each save says where: mods\<the mod>\zbd\... or mods\editor edits\zbd\...
6. To undo a save, the file it replaced is kept beside it as <file>.bak: put the .bak back in its place. For "editor edits", turning that mod off in MODS undoes them all.
7. Every field, button and row has a tip: hold the mouse over it. The full guide is docs/editor-guide.md.

## From a fresh mod to a changed mission

1. Make the mod: a folder under mods in your write folder with a mod.txt (name=My Mod) and campaign=my.txt, and my.txt listing the missions to change (name=My Campaign, then mission=c1 1, one line each; docs/modding.md).
2. Open it: on the MODS screen pick the campaign, EDITOR lists its missions, ENTER opens the one picked. The mission starts with the editor up and a stand-in pilot. Its saves go into the mod itself (zbd\c1\readerm1.zbd and the like), each keeping the file it replaced as .bak.
3. Change the AI's paths: EDIT AI NETS, pick a net (net_01 is usually the main one). Pick a node and move it (Move Node, then press the mouse, drag, let go), link nodes (pick one, then the next), delete what is not wanted, set a node's speed or wait (Node Params). Save Changes.
4. Add a vehicle: pick a node, Add veh to node, choose its type and side, OK. It is made at once and written into the mission's vehicle list (aiv.zrd).
5. Change a vehicle's AI: EDIT VEHICLES, pick it, set its pages (when it wakes, how far it follows, how it attacks and retreats, what valve it sets), Apply to try it in the running mission, Save to keep it.
6. Try it: EXIT GAME EDIT goes back to MODS. Play the campaign (ENTER on the MODS screen) to see the changes as a player would.
7. Undo: every save keeps the file it replaced as <file>.bak; put it back to undo that save.

## The editor's menu

The menu over the mission when the editor is up (the original's, with HELP added in one of its blank rows). The view is a free camera, flown with the movement keys.

| Item | What it does |
|---|---|
| PICK A COMMAND | The menu's title. Point at a row and click, or press Space. |
| EDIT AI NETS | Edit the AI's paths: pick a net, then move, link, add and delete its nodes. Opens the net editor's menu. |
| EDIT VEHICLES | The Mech AI window: pick one of the mission's vehicles and change its AI settings. |
| EDIT WEAPONS | Empty in the original game: it does nothing (yet). |
| HELP | This quick start, in a window. Ours: the original had none. |
| EXIT GAME EDIT | Close the editor. From MODS's EDITOR it goes back to the MODS screen; in a custom campaign's mission the view goes back to your mech and the mission goes on. |

## The net editor's menu

The net editor's own menu, shown once a net is picked. Click a node in the world to pick it; picking another links the first to it.

| Item | What it does |
|---|---|
| Pick CMD or Point | The menu's title: pick a row, or click a node in the world. |
| GOTO Net | Fly the view to the net's first node. |
| Change Net Params | The original's net window: Open shows a second net file beside this one, to compare. |
| Node Params | The first picked node's settings: place, speed, waits, flags, name, gate. Empty in the original; ours. |
| Add veh to node | Make a vehicle at the first picked node, on this net, and write it into the mission's vehicle list. Did nothing in the original; ours. |
| Move Node | Move the first picked node: then press the mouse, drag it where it should go, let go. |
| Delete Node | Delete the picked node, or the picked line, after asking. |
| Delete Net | Empty in the original game: it does nothing. |
| Save Changes | Write the net into the mission's archive in the mod (one .bak kept of the file replaced). The line under the menu says where. |
| Tree View | A window listing the world's objects by area. It only shows. |
| Help | The quick start, in a window. Ours. |
| Exit Net Editor | Close the net editor and its windows; back to the editor's menu. |

## Picking a net

EDIT AI NETS first asks which net to edit (the original's dialog).

| Item | What it does |
|---|---|
| the list | The mission's AI nets (paths), by file: net_01 is usually the main one. New Net makes an empty one at the view's pick. |
| OK | Edit the net picked. |
| Cancel | The same as OK, as in the original. |

## Node Params

The first picked node's record, as the net file holds it. A field whose box is unticked is left to the game's default. OK puts it on the node; Save Changes writes it.

| Item | What it does |
|---|---|
| placed (x y z given) | Whether the node has a place of its own (unticked: an edge node, which only adds settings to other nodes' links). |
| x | Its place east-west, in the world's units (metres). |
| y | Its height. The game puts the node on the ground under it when the mission loads. |
| z | Its place north-south. |
| speed | How fast a vehicle goes along the links out of this node (the file's speed). |
| wait interval | The interval of the wait at this node (the file's wait_interval). |
| wait time | How long a vehicle waits at this node (the file's wait_time), in seconds. |
| can leave, first | The file's canleave, first word: with it, a vehicle on these links may break off from the net (docs/ai.md). |
| can leave, second | The canleave pair's second word. |
| jump | A jump on the links out of this node (the file's jump): for mechs with jets. |
| arrange nodes, spacing | arrangenodes: the nodes this one links to are put on a circle this far round it. |
| hidden | The file's hidden flag: as the default, yes or no. |
| fixed | The file's fixed flag: as the default, yes or no. |
| autonext | The file's autonext flag: as the default, yes or no. |
| visible | The file's visible flag: as the default, yes or no. |
| backup | A backup node (the file's backup). |
| shunt | The file's shunt flag on the node's links. |
| name | The node's name (a message key or text); empty clears it. |
| gate | The gate the links out of this node pass (the file's gate); empty clears it. |
| OK | Put these on the node. Save Changes writes them. |
| Cancel | Leave the node as it was. |

## Add veh to node

A new vehicle at the first picked node, following this net. OK makes it at once and writes its record into the mission's aiv.zrd in the mod, its AI settings copied from the first vehicle of that type in the mission.

| Item | What it does |
|---|---|
| vehicle type | The kind of vehicle: the 19 mechs, then the vehicles vehicle.zrd lists. |
| x | Where it starts, east-west (the node's place to begin with). |
| y | Its height (the ground under it is used). |
| z | Where it starts, north-south. |
| yaw | Which way it faces, in degrees. |
| team id | Its team: 1 your side, 2 the enemy, 0 neither (Friend, Foe, Other set it). |
| side | Friend, Foe or Other: sets the team id to 1, 2 or 0. |
| OK | Make the vehicle now and write its record (said on the line under the menu). |
| Cancel | Close without making anything. |

## Deleting

Delete Node asks first, as the original did.

| Item | What it does |
|---|---|
| OK | Delete the picked node (and its links), or the picked line. |
| Cancel | Keep it. |

## Change Net Params

The original's net window: it has no parameters, only Open, which shows a second net beside the one edited.

| Item | What it does |
|---|---|
| OK | Close the window (the second net goes). |
| Cancel | Close the window. |
| Open | Choose a net file of the mission to show beside the edited net. |

## Open

The mission's net and region files, from the open archives.

| Item | What it does |
|---|---|
| the list | The files: net_NN.zrd are nets, reg_*.zrd regions. |
| Open | Show the file picked, drawn beside the edited net. |
| Cancel | Close the list. |

## Tree View

The world's objects, the world first, then by area (row and column). It only shows.

| Item | What it does |
|---|---|
| the tree | Open an item to see what it holds. |
| OK | Close it. |
| Cancel | Close it. |

## The Help window

The quick start (above), from the HELP rows.

| Item | What it does |
|---|---|
| Close | Close the window. |

## The vehicle editor (Mech AI)

The original's Mech AI dialog: one of the mission's vehicles, its AI settings on pages. A number box takes what the original's Windows boxes took: a bad number (or "+0", "-0") stops at that box with a message. Ranges are in the world's units (metres). Some settings sit on two pages (said below): the page left last wins. Apply puts the settings on the vehicle in the running mission; Save writes them for the next start.

| Item | What it does |
|---|---|
| the vehicle list | The mission's mechs and AI vehicles, by their record's name. Picking another applies the one shown first. |
| p1 | Activation: when the vehicle wakes, and how far it leaves its net to follow a target. |
| p2 | The vehicle: chassis, loadout, pilot, team, home net and where it starts. |
| Attack | How it attacks: aim, throttle and its strategy (with the strategy's own tab). |
| Retreat | When it breaks off and goes back to its net, and for how long. |
| p5 | Its scans (looking round for targets), their trigger and collision avoidance. |
| Fire | Fire control: the four numbers of its firing. |
| Valve | The valves it sets when a condition holds (doors, alarms, mission events). |
| Always active | On: the vehicle is awake from the start (aiv.zrd p8, item 10). |
| Disable | On: the vehicle starts disabled (p6, item 1). |
| Activation range | How near a target must come to wake it (params, item 1; kept squared in play). |
| Range to | What the activation range is measured to: the player or the nearest enemy (p8, item 11). |
| Enable Initial | On: it may leave its net to follow from the start (params, item 2). Saved. |
| Enable Now | The same, for the running vehicle only (not saved). Grey: as the vehicle has it now. |
| Leave Net Range to Target | It leaves its net to follow a target nearer than this (params, item 3). |
| Return to Net Range from Net | It goes back when this far from its net (params, item 4). Retreat shows it too. |
| Approach Range to Target | How near it closes on a target (params, item 5). Direct shows it too. |
| Minimum Range to Target | The nearest it comes to a target (params, item 6). Direct shows it too. |
| Configuration | The mech's loadout, from its stock ones (p10, item 2). Mechs only. |
| Default Chassis | The chassis the record names (p12): one of the 19, or "default" for none. |
| Skin | Its paint scheme, 0 to 100 (skin). |
| Pilot Name | The name shown for its pilot, at most 32 characters (pilot). Empty: none. |
| Objective Conditional | The record's objective-conditional flag (p12, item 2). |
| can configure | Whether its loadout may be changed (the pilot tag's can_configure). |
| Shutdown on start | On: it starts powered down (p12, item 3). |
| Home Net | The net it follows (the record's first item); the Net ID box shows its number. |
| Net Edit | Open the net window (Change Net Params) to look at nets beside this one. |
| Net ID | The home net's number (read only: pick it in Home Net). |
| Show | Draw its home net in the world, as the net editor does. Grey counts as on. |
| Show InOut | Draw the mission's boundary as walls: red outside, blue or yellow inside. |
| x | Where it starts, east-west. Send home puts it there now. |
| y | Its starting height. |
| z | Where it starts, north-south. |
| yaw | Which way it faces at the start, in degrees. |
| Send home | Put the vehicle at x, y, z and yaw now, on its home net (as last shown: Apply first). |
| Team | Friend, Foe or Other: its team becomes the first of your allies, your enemies, or neither. |
| Team number | Its team's number (read only). Leaving the box rounds it to a real team. |
| Lancemate | Whether it is one of your lancemates (lancemate: off, on, or grey, the file's third value). |
| Firing Accuracy | Its aim (p10, item 1), 0 to 1000: a number past that is kept but stops the page. |
| Firing Accuracy (second box) | The second aim number (p11). |
| Strategy | How it attacks: Direct, Zig Zag, Circle, Jump or Random Move (p2, item 1). |
| Throttle | Its throttle while attacking (p8, item 13). |
| Throttle (second box) | The second throttle number (p8, item 9). |
| Circle | The Circle strategy's setting. |
| Direct | The Direct strategy's settings (p1's approach and minimum ranges). |
| Jump | The Jump strategy's settings (p5's four scan numbers). |
| Zig Zag | The Zig Zag strategy's settings. |
| Random Move | Never made: it behaves like Circle (the original says so). |
| Radius | The circle's radius round its target (p2, item 5; Zig Zag's target distance too). |
| Approach | p1's Approach Range to Target, here too. |
| Minimum Range | p1's Minimum Range to Target, here too. |
| Delay | p8, item 1: with item 2, the bounds of a random wait (p5's first box too). |
| Delay 2 | p8, item 2 (p5 too). |
| Duration | p8, item 3 (p5 too). |
| Duration 2 | p8, item 4 (p5 too). |
| Length | The zig-zag's leg length (p2, item 6). |
| Target Distance | How far from the target it zig-zags (p2, item 5; Circle's radius too). |
| Radius (Random Move) | Circle's radius (Random Move is Circle). |
| Net Distance | On: it retreats when this far from its net (the box beside it, p1's return range). |
| Net Distance's number | p1's Return to Net Range from Net, here too. |
| Health | On: it retreats when its health falls below the box beside it (p2, item 4). |
| Health's number | The health it retreats below (p2, item 8). |
| Time | On: it retreats after the time beside it (p2, item 3). |
| Time's number | The time (p2, item 7; scaled by the game's intensity in play). |
| Health Change | On: it retreats when its health drops by the box beside it (p9, item 4). |
| Health Change's number | The drop in health (p9, item 5). |
| For timeout | On: after retreating it stays off its net for the time beside it (p6, item 3). |
| For timeout's number | That time (p6, item 2). |
| Forever | On: after retreating it never leaves its net again (p6, item 4). |
| Time (Initial) | Its scans start on a timer (p8, item 16). Saved. |
| Time (Set Value) | The same for the running vehicle only. Grey: as it has it. |
| Hit (Initial) | Its scans start when it is hit (p8, item 15). Saved. |
| Hit (Set Value) | The same for the running vehicle only. Grey: as it has it. |
| Aim (Initial) | Its scans start when it is aimed at (p8, item 14). Saved. |
| Aim (Set Value) | The same for the running vehicle only. Grey: as it has it. |
| Scan Parameters | Scan (look round) or Jump (jump about) when triggered (p8, item 17). |
| the scan's first box | p8, item 8. |
| the scan's second box | p8, item 1 (Jump's Delay too). |
| the scan's third box | p8, item 2 (Jump's too). |
| the scan's fourth box | p8, item 3 (Jump's too). |
| the scan's fifth box | p8, item 4 (Jump's too). |
| Quit scan on trigger pull | On: it stops scanning once it fires (p13). |
| Collision Avoidance: Enable | On: it steers round what is in its way (p8, item 5). |
| collision avoidance's first box | Its range (p8, item 6; kept squared in play). |
| collision avoidance's second box | p8, item 7. |
| Fire Control's first box | params, item 7. |
| Fire Control's second box | params, item 8. |
| Fire Control's third box | params, item 9. |
| Fire Control's fourth box | p8, item 12. |
| the valve list | Which valve this page sets: pick one, or type a new name and press Enter to make it ("0": none). Picking one gives the vehicle an order for it. |
| Active | On: this order is active. |
| Set | What the valve is set to when a condition holds: close (zero) or open (one). |
| Health | When its health falls below the box beside it. |
| Health's number | That health. |
| Ammo | When its ammunition falls below the box beside it. |
| Ammo's number | That amount. |
| Fired on | When it is fired on. |
| Target Distance | When its target is nearer than the box beside it. |
| Target Distance's number | That distance (kept squared in play). |
| Heat | When its heat rises past the box beside it. |
| Heat's number | That heat. |
| Enters | When it enters the region below. |
| Exits | When it leaves the region below. |
| Region | The region for Enters and Exits: pick one, or type a region file's name and press Enter. |
| Target LOS | When it sees its target, within the box beside it. |
| Target LOS's number | That distance (kept squared in play). |
| Pause | Pause the game while editing (the whole mission stops). |
| the save line | Where Save last wrote, or why it could not. |
| OK | Apply, then close the window. |
| Apply | Put the page's settings on the vehicle in the running mission. |
| Refresh | Read the mission's vehicles and nets again (after adding one); the shown one stays. |
| Save | Apply, then write every vehicle applied this mission into the mod's aiv.zrd (one .bak kept). From the next start. |

## The vehicle editor's message

When a box does not read, the original's message, and the page stops at that box.

| Item | What it does |
|---|---|
| OK | Close the message; correct the box and Apply again. |
