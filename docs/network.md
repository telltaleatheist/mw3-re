# Network (first look)

Mech3.exe 1.2, 2026-10-19. Not ported; this is enough to plan the LAN work.

## Layers

- **zNetwork** (`znet_session.cpp`, `znet_dplay.cpp`, about 270 functions from
  0x586c20 to 0x58fc70): a wrapper around DirectPlay 4 (`IDirectPlay4A`).
  Connection types TCP/IP, IPX and modem; lobby launch; error strings for every
  `DPERR_*`. Sends go through 0x5888a0 (`SendEx`, vtable +0xc4, asynchronous
  when the provider allows: "NOT Using Asynchronous Sends." otherwise).
  Receiving is 0x588930 (`Receive`, +0x64), the `poll_network` hook that
  `zGameApp`'s main loop calls each frame; 0x588a70 handles DirectPlay's own
  system messages (player joined, player data changed, ...).
- **MW3 messages**, built in the game code around 0x46f160-0x470380 (after
  `mech_reader.cpp`) and in `mission.cpp`: each is `uint16 type, uint16 size,
  payload`. Seen so far: 0x1f text (a chat line), 0x1a an event that copies a
  block of state, 0x18 built by an object's vtable +0x58, a score/team update
  (0x470170, `[0x60861c]` pilot data), and a 7-dword record with a position
  and a flag bit 0x80 (0x46fde0).

## Sync model

The messages carry state and events (positions, scores, chat), not per-frame
input, and the receive side polls once per frame without waiting for other
players. That points to state/event sync rather than lockstep, so small float
differences between machines should not desync a game. Not proven yet: the
game-side receive handlers are still to be read.

## For the rebuild

DirectPlay is gone (FIX (A)): the transport becomes UDP (for example ENet), with
a LAN broadcast for finding games, carrying the same MW3 message types.
Playing against the original exe would need DirectPlay's wire protocol and is
out of scope unless decided otherwise.
