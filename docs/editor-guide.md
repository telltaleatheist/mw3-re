# The game editor: a guide

<!-- Generated from src/upgrades/editor/help*.cpp (the editor's tooltips): build/mw3 --editor-guide docs/editor-guide.md. Do not edit by hand; the test editor_guide_is_current fails when it is stale. -->

Pirate's Moon, as MechWarrior 3, shipped with the developers' own editor still inside: a menu over the running mission for the AI's paths (nets) and the vehicles' AI. The rebuild opens it to players and modders, makes the parts the release left empty work, and saves what you change as a copy of the campaign, a mod of your own, never into the game's files or anyone else's mod. The menus are the original's; the dialogs are windows of the rebuild's own. Every field, button and row in the editor has a tip (hold the mouse over it); this guide gathers them. How it works inside: docs/editor.md.

Pirate's Moon's editor is MechWarrior 3's, and so is the rebuild's: every menu, window and save in this guide is in both games. What is Pirate's Moon's own: the vehicle editor's chassis list and Add veh to node offer its 25 mechs. The original Pirate's Moon kept no edit of a net (a node moved, added or deleted was dropped); the rebuild keeps and saves them, as in MechWarrior 3. What it lacks: a node's via, terminus and static_mfb settings, new in Pirate's Moon, have no fields in Node Params (a save writes them as the file had them), and a vehicle's target settings (the target tag in aiv.zrd) have no page in Mech AI.

## Quick start

1. Open the editor from the MODS screen: choose a campaign at the left, then MISSIONS, a mission, and PLAY. The game starts again inside that campaign in the mission. There the pause menu's EDIT MISSION, or Shift+F12, opens the editor, and PLAY MISSION or the same key goes back to playing. A campaign played from MODS (PLAY) has the same switch in every mission. Nothing started from the main menu has the editor.
2. The menu is on the left; point at a row and click (or press Space). The view is a free camera: fly it with the movement keys.
3. EDIT AI NETS: pick a net. Its nodes and links are drawn in the world. Click a node to pick it; picking a second one links the first to it. Move Node, Delete Node, Node Params and Add veh to node work on the first picked node.
4. EDIT VEHICLES: pick a vehicle, change its pages, then Apply (the running mission takes it at once) and Save (it is written for the next start).
5. The editor only saves as. The first save (Save Changes in the net menu, the vehicle editor's Save, Add veh to node, New Weapon, Teleport's OK) asks for a name and makes a copy of the campaign under it, a mod of your own: mods\<the name>. That save and every later one go into the copy, and each says where. The campaign you opened, the game's own files and a mod you imported are never changed. A copy of yours opened in the editor is saved into itself, nothing asked.
6. EXIT GAME EDIT (or Shift+F12, or PLAY MISSION on the pause menu) closes the editor and you play the mission as it now is; the same key or EDIT MISSION opens the editor again. QUIT on the pause menu goes back to MODS, your copy chosen at the left when a save made one. There PLAY plays it, MISSIONS plays or edits one of its missions at once, INFO says what it changes and there EXPORT packs it into a zip to give away, and DELETE CAMPAIGN removes it with its pilots and saved games.
7. To undo a save, the file it replaced in your copy is kept beside it as <file>.bak (the save before, one kept): put the .bak back in its place. To undo everything, DELETE the copy on MODS: the campaign it was made from is as it was.
8. Every field, button and row has a tip: hold the mouse over it. The full guide is docs/editor-guide.md.

## From a campaign to your own changed copy

1. Choose what to start from. MODS lists the campaigns at the left: the game's own first, each campaign a mod brought, each copy you made. The mods ticked for it at the right are mounted with it. To start from someone else's work, IMPORT it (a zip, a mod's installer, or a file inside its folder or inside a game folder an installer was run over): the line says how many warnings its files gave, and INFO lists them in three groups, the worst first (what stops part of the mod working, what works here but may crash the original game, slips that do no harm), and writes the same to mod-reports\<the mod's folder>.txt in your settings folder. Missions, mech variants and animations made in other tools need no editor here: a mod holding them as the game's own files, or as mech3ax's JSON files beside them, loads as it is (docs/modding.md, Readable files read directly), and a mech loadout (.m3c) picked with IMPORT goes to the mech lab's Ctrl+I.
2. Open a mission: with the campaign chosen, MISSIONS lists its missions and PLAY starts the one picked. The game starts again inside the campaign, in the mission, with a stand-in pilot; EDIT MISSION on the pause menu (or Shift+F12) opens the editor.
3. Change the AI's paths: EDIT AI NETS, pick a net (net_01 is usually the main one). Pick a node and move it (Move Node, then press the mouse, drag, let go), link nodes (pick one, then the next), delete what is not wanted, set a node's speed or wait (Node Params). Save Changes.
4. Name your copy: the first save asks for a name (offered: "<campaign>, my edit") and makes mods\<the name>, a copy of the campaign that holds only the files you change (a mission's archive whole, zbd\c1\readerm1.zbd and the like) and names the campaign it was saved from (base= in its mod.txt). A name already taken is refused, never written over. Cancel saves nothing. Later saves go into the copy, nothing asked. A mod folder you made by hand is treated as any other: copied, never written.
5. Add a vehicle: pick a node, Add veh to node, choose its type and side, OK. It is made at once and written into the mission's vehicle list (aiv.zrd) in your copy.
6. Change a vehicle's AI: EDIT VEHICLES, pick it, set its pages (when it wakes, how far it follows, how it attacks and retreats, what valve it sets), Apply to try it in the running mission, Save to keep it.
7. Add a weapon: EDIT WEAPONS, pick the weapon to copy, set its name and numbers, OK. It is written into your copy (mod.txt and zbd\reader.zbd) and is part of that campaign; the mech lab offers it from the next start.
8. Set a mission's salvage: MISSION SETTINGS, type how many times in 100 a kill's salvage is offered in the mission that is open (empty for none of its own), OK. It is written into your copy (mod.txt) and counts from the next start.
9. Try it: EXIT GAME EDIT puts you in the mission at once. Or quit to MODS, your copy chosen: MISSIONS, a mission, PLAY plays that mission at once with a stand-in pilot; when it ends the list plays the others, and BACK leaves. PLAY plays the campaign as a player would: its pilot screen lists the copy's own pilots and your main-game pilots, and picking one of yours copies it in (the main game's pilot is never changed).
10. Go on: choose the copy, MISSIONS, a mission, PLAY, then EDIT MISSION. Its saves go into the copy itself.
11. Give it away: EXPORT packs the copy into one zip that IMPORT takes. A copy of a mod's campaign needs that mod on the other machine too; there it is an imported mod, which that player's editor copies and never writes.
12. Undo: every save keeps the file it replaced as <file>.bak; put it back to undo that save. DELETE on MODS removes a copy whole, with its pilots and saved games; RESET, on the game's own campaign, clears its ticks and its pilots there. Neither touches the game's files or your main-game pilots.

## The editor's menu

The menu over the mission when the editor is up (the original's, with HELP added in one of its blank rows). The view is a free camera, flown with the movement keys.

| Item | What it does |
|---|---|
| PICK A COMMAND | The menu's title. Point at a row and click, or press Space. |
| EDIT AI NETS | Edit the AI's paths: pick a net, then move, link, add and delete its nodes. Opens the net editor's menu. |
| EDIT VEHICLES | The Mech AI window: pick one of the mission's vehicles and change its AI settings. |
| EDIT WEAPONS | New Weapon: a weapon copied from one the game knows, its name and numbers set, written into your copy of the campaign (in a mission opened from MODS, MISSIONS only). Empty in the original game; ours. |
| HELP | This quick start, in a window. Ours: the original had none. |
| MISSION SETTINGS | Settings of the mission that is open, written into your copy of the campaign (in a mission opened from MODS, MISSIONS only). For now one: how often a kill's salvage is offered in this mission. Ours. |
| TELEPORT | Move your mech to a place: the mission's nav points, the world's saved places, or one you type in. The developers' own tool, which the original never let you reach; ours. |
| EXIT GAME EDIT | Close the editor: the view goes back to your mech and the mission goes on in play. Shift+F12 does the same, and opens the editor again; so does the pause menu's switch (EDIT MISSION, PLAY MISSION). Quitting the mission from the pause menu goes back to the MODS screen, your copy chosen when a save made one. |

## Teleport

The developers' teleport, which the original carries but never lets you reach. It lists your own place (Current Location), the mission's nav points and the world's saved places (each world ships some: mission starts, the Instant Action base). OK moves your mech there and saves the places, nav points included, into your copy of the campaign as zbd\<world>\location.zrd (the one before kept as .bak), as the original saved them. The first save asks the copy's name.

| Item | What it does |
|---|---|
| place | Pick a place to fill the numbers, or type a new name and leave the box to add one at the numbers below. |
| x | Where to go: east-west, in metres. |
| y | Height. |
| z | North-south. |
| yaw (deg.) | Which way to face, in degrees (-360 to 360). |
| OK | Go there, and save the places into your copy of the campaign (the first save asks its name). |
| Cancel | Close it; stay where you are. |
| Delete | Take the picked place off the list (asked first); saved with OK. |

## Save as

The editor never changes the campaign it opened. The first save of a session asks for a name and makes a copy of the campaign under it: mods\<the name>, a mod of your own that holds only the files you change and names the campaign it was saved from. The save then goes on into it, and so does every later one. The copy is listed among MODS's campaigns with the ticks of the campaign it came from. A copy of yours opened in the editor is saved into itself, and this box does not come up.

| Item | What it does |
|---|---|
| Name | The copy's name on MODS and its folder under mods. Offered: "<campaign>, my edit", then "..., my edit 2". A name whose folder exists is refused and another offered. |
| Save | Make the copy and save into it. |
| Cancel | Close the box: nothing is saved, and the next save asks again. |

## The net editor's menu

The net editor's own menu, shown once a net is picked. Click a node in the world to pick it; picking another links the first to it.

| Item | What it does |
|---|---|
| Pick CMD or Point | The menu's title: pick a row, or click a node in the world. |
| GOTO Net | Fly the view to the net's first node. |
| Change Net Params | The original's net window: Open shows a second net file beside this one, to compare. |
| Node Params | The first picked node's settings: place, speed, waits, flags, name, gate. Empty in the original; ours. |
| Add veh to node | Make a vehicle at the first picked node, on this net, and write it into the mission's vehicle list in your copy of the campaign. Did nothing in the original; ours. |
| Move Node | Move the first picked node: then press the mouse, drag it where it should go, let go. |
| Delete Node | Delete the picked node, or the picked line, after asking. |
| Delete Net | Empty in the original game: it does nothing. |
| Save Changes | Write the net into the mission's archive in your copy of the campaign (the first save asks the copy's name; one .bak kept of the file replaced). The line under the menu says where. |
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

A new vehicle at the first picked node, following this net. OK makes it at once and writes its record into the mission's aiv.zrd in your copy of the campaign, its AI settings copied from the first vehicle of that type in the mission.

| Item | What it does |
|---|---|
| vehicle type | The kind of vehicle: Pirate's Moon's 25 mechs, then the vehicles vehicle.zrd lists. |
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
| Default Chassis | The chassis the record names (p12): one of Pirate's Moon's 25, or "default" for none. |
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
| Save | Apply, then write every vehicle applied this mission into aiv.zrd in your copy of the campaign (the first save asks the copy's name; one .bak kept). From the next start. |

## The vehicle editor's message

When a box does not read, the original's message, and the page stops at that box.

| Item | What it does |
|---|---|
| OK | Close the message; correct the box and Apply again. |

## New Weapon (EDIT WEAPONS)

A new weapon for your copy of the campaign the editor was opened on from MODS, MISSIONS (the first save asks the copy's name; the campaign itself and an imported mod are never written). It is part of that campaign: on whenever the copy is played. It starts as a copy of a weapon the game knows, under a free name; set its row, its main numbers, its effects and sounds, then OK. Every other number is the copy's. An effect or sound is picked from those the game's weapons use, and shown once where the view points when picked. OK writes the weapon= line into the copy's mod.txt (the old file kept as mod.txt.bak) and the definition at the end of BALLISTICS in the copy's zbd\reader.zbd (weapons.zrd; the archive it replaced kept as .bak), or, when the copy has its own zbd\weapons.zrd text file, into that file (its own text kept, the old one as .bak). The mech lab offers it from the next start. The original game reads the file too and leaves the extra weapon alone.

| Item | What it does |
|---|---|
| copy of | The weapon it starts from: every number below, and all the rest, are this one's. |
| name | The weapon's name in weapons.zrd and mod.txt, up to 31 letters, digits, _ and -. A name any weapon already has is refused. |
| ammo | yes: the lab sells rounds for it and its bin empties as it fires. no: it never runs out. |
| tech | The techs the lab offers it in: Clan, Inner Sphere, any, or none (as TAG). |
| title | The name the lab shows; empty or the same as the name: the name. |
| damage | The damage of a hit (DAMAGE). |
| heat | The heat a shot adds to the mech that fires it (HEAT's first value). |
| range | How far it reaches, in metres (RANGE). |
| fire rate | Shots a second (FIRE_RATE): 0.25 is one every four seconds. |
| velocity | How fast its shot flies, metres a second (VELOCITY). A beam's own is kept: not written. |
| Clan tons | Its weight in a Clan mech's lab (MASS clan); written when it is offered to the Clans. |
| Clan slots | The critical slots it takes in a Clan mech (MASS clan). |
| Inner Sphere tons | Its weight in an Inner Sphere mech's lab (MASS sphere); written when offered there. |
| Inner Sphere slots | The critical slots it takes in an Inner Sphere mech (MASS sphere). |
| rounds a ton | With ammo: the rounds one ton of its ammo holds (MASS ammo). |
| muzzle | The animation at the firing end as it fires (FIRE ANIMATION). Shown where the view points. |
| firing sound | The sound of a shot (FIRE SOUND). Played where the view points. |
| projectile | The model that flies (FLYOUT MODEL): a shell, a missile, a beam's model. Not shown here. |
| projectile animation | The animation on the flying model (FLYOUT MODEL_ANIMATION). |
| trail | The trail behind the shot (FLYOUT ANIMATION). |
| flyby sound | The sound while it flies, or a beam's while it is on (FLYOUT SOUND). |
| impact | The animation where it hits (IMPACT default ANIMATION); the other soils' are the copy's. |
| impact sound | The sound where it hits (IMPACT default SOUND). |
| OK | Write it into your copy of the campaign (mod.txt and zbd\reader.zbd, or its own zbd\weapons.zrd), each file's earlier copy kept as .bak. |
| Cancel | Close it; nothing is written. |

## Mission settings (MISSION SETTINGS)

Settings of the mission that is open, for your copy of the campaign the editor was opened on from MODS, MISSIONS (the first save asks the copy's name; the campaign itself and an imported mod are never written). For now one: how often a kill's salvage is offered. The original game tosses a coin, 1 in 2, in two places: for a destroyed vehicle's weapons (all or none), and for a destroyed mech that lost a leg (the whole mech, or only its weapons and equipment). OK writes the line mission_salvage=<world> <mission> <number> into the copy's mod.txt (the old file kept as mod.txt.bak); it counts from the next start, whenever the copy is played. A mod ticked for the campaign can set one number for every mission (salvage_chance= in its mod.txt); a mission's own number wins over it. The main menu's campaign is never changed.

| Item | What it does |
|---|---|
| salvage chance | How many times in 100 a kill's salvage is offered in this mission: 0 never, 100 always, 50 the original's odds. Empty: no number of its own (a ticked mod's, else the original's). |
| OK | Write it into your copy of the campaign (mod.txt), the earlier mod.txt kept as mod.txt.bak. |
| Cancel | Close it; nothing is written. |
