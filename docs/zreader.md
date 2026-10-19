# zReader: the game's data files

Mech3.exe 1.2, found with REA + Ghidra and the raw disassembly (2026-10-19).
Ported to `src/gamez/zreader/` and `src/gamez/zutil/zutl_zar.*`, checked against
the original in Unicorn on every file in `reader.zbd`
(`tests/differential/test_zreader.py`).

## Two layers

**The archive, `zutil/zutl_zar.cpp`.** A `.zbd` is a plain concatenation of files
with the table of contents at the end:

    file data ... | Entry[count] | uint32 version (1) | uint32 count

Each entry is 0x94 bytes: start, length, a 64-byte name, flags, a 64-byte comment
and a FILETIME. MW3 accepts only version 1 (`0x5767b0`); mech3ax also knows a
version 2 with a checksum, used by Zipper's later games. Names are matched with
`_stricmp`. `reader.zbd` holds 178 files (`briefing.zrd`, `mechs.zrd`,
`sounds.zrd`, ...).

**The reader, `zreader/zreader.cpp`.** Each file is one tree of 8-byte nodes,
`{int type; value}`: 1 int, 2 float, 3 string, 4 list. Stored as type then value;
a string is length then bytes; a list is a count then count - 1 nodes. In memory a
list keeps that count in its own first slot, `list[0] = {1, count}`, which is why
the stored count is one more than the items (mech3ax's reader notes the off-by-one
and guesses this; the exe confirms it).

Objects are flat lists of key/value pairs: `("CD_TRACKS" (("menu" 2) ...) ...)`.
`find` (0x5569e0) returns the node after the first string equal to the key.

The typed getters take that node: a string, a float (an int is converted), an
int, fixed lists of two or three floats or ints (0x556b70-0x556cf0; the integer
lists truncate floats with MSVCRT `_ftol`), and a boolean that is an int or one
of `true`/`on`/`false`/`off` in any case (0x556da0). Each has a keyed wrapper that
calls `find` first (0x5570b0-0x557150). A list item of the wrong type leaves that
output as it was.

## Loading

At startup (0x480000) the game points the reader at `..\data\common\zrdr` and
tries `detail.zrd`. Only if that fails does it open `zbd\reader.zbd` as the
current archive and try again, so the developers' loose text files override the
archive. `load` (0x556740) drops the directory and looks up name + extension in
each open archive, in the order they were opened. The sound module loads
`sounds.zrd` into 0x72c440 and passes its `CD_TRACKS` node to the CD code
(0x5706f0 -> 0x572450).

The loose text files (`.zrd` text, with `#if`/`#endif`) go through a tokenizer at
0x5563f0, 0x556510, 0x557550 and neighbours. None ship with the game, so it is not
ported yet. The archive writer (0x5765b0, 0x576890, 0x576990, 0x576720) is not
ported either; the game never builds archives at run time.

## Original behaviour worth knowing

- `find` is depth-first through every nested list and does not tell keys from
  values: a string *value* equal to the key matches too. Kept.
- When the matching string is the last item, `find` returns the slot one past the
  end, and an empty list `()` still has its item 1 read by the typed getters.
  Both read heap memory beyond the list. In `reader.zbd` 5,597 of the 13,646
  lookups the differential test makes land there. The port gives every list one
  zeroed spare slot, so those reads mean "no value" (FIX (A), `docs/fixes.md`).
- `get_float` copies a float's bits with `mov`; only ints are converted (`fild`).
  Ghidra's pseudocode shows a cast on both, which is wrong for floats.
- `reset` (0x5766d0) does nothing when the archive has no table, so a failed open
  leaves the path for `destroy` to free.
