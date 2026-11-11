# A mod's installer program read as an archive (Clickteam Install Creator)

Ours (`src/upgrades/mods/installer_read.{h,cpp}`, `bz2_read.{h,cpp}`; modding.md,
IMPORT from an installer). Freako's mods and the other MechWarrior 3 and
Pirate's Moon mods we know of ship as installer programs made with Clickteam
Install Creator. IMPORT reads such a program as an archive: nothing of it is
run, and no part of it is written out as a program.

Where this description comes from: the installers themselves, read byte by
byte (the 15 in the catalogue of 2026-11-10: every file of every one unpacks
to its declared size with this reading), by dev 2 and dev 3 independently and
then reconciled. No third-party code or documentation was used. A public tool
(Bioruebe's cicdec) reads the same format; it was not read.

## The file

A Windows program (often UPX-packed: sections UPX0 / UPX1), then the
installer's data appended after the last section's bytes. The appended data
is found from the PE's section table (the largest raw offset + raw size),
never from the program's contents.

```
6 bytes     77 77 67 54 29 48  ("wwgT)H")
blocks      [u16 type][u16 flags][u32 size][size bytes of content] ...
```

A packed block's content is `[u32 unpacked size][u8 method][stream]`: method 1
is zlib, 2 is bzip2. Block types seen, in order:

| Type | What | Read? |
|---|---|---|
| 0x1435 | the wizard's fonts and layout | no |
| 0x1436 | the wizard's picture (a bitmap) | no |
| 0x1438, 0x143d, 0x143f | a publisher string, a font, 73,728 bytes (the oldest maker's only) | no |
| 0x143e | the texts: the title, the default folder (the first `X:\...` string), the screens' words | the title and the default folder |
| 0x1445 | 20 bytes | no |
| 0x143a | **the file list** | yes |
| 0x7f7f | **the files' data**, not packed as a whole | yes |

Unknown types are skipped by their size. The data block's size field leaves
out its first word, so the last file ends four bytes past the block's stated
end; the file's own end is the bound.

## The file list (0x143a, unpacked)

`[u32 count]`, then `count` records, each starting with its own length (u16).
Two layouts, by the maker's version:

| Field | 2,0,0,28 and 2,0,0,45 | 2,0,0,16 |
|---|---|---|
| u16 record length | +0x00 | +0x00 |
| u32 kind (0 a file; 2 the uninstaller's entry) | +0x04 | +0x04 |
| u32 flags | +0x08 | +0x08 |
| u32, a checksum by its look (not CRC-32 of the unpacked bytes; **not checked**) | +0x14 | +0x10 |
| u32 unpacked size | +0x18 | +0x14 |
| u32 offset in the data | +0x1c | +0x18 |
| u32 packed size (with the method byte) | +0x20 | +0x1c |
| three FILETIMEs | +0x28 | +0x20 |
| the name, backslashes, NUL-ended | +0x40 | +0x38 |

The reader takes the layout whose sizes and offsets hold for **every** file
(its data inside the installer and starting with a method byte 1 or 2), and
refuses a list neither fits. A kind other than 0 is not a file (its fields
mean something else); a name ending in a backslash is a folder. No file in
the 15 is stored unpacked, so any other method byte is refused.

Names are as the installer would place them under its chosen folder: under
`MechWarrior3\...` or `MechWarrior 3 Pirate's Moon\...` with the default
folder one level up (the newer ones), from the game folder itself
(`zbd\c1\readerm1.zbd`, the older ones; one has `ZBD\` in capitals), or from
deeper (one's default folder is `...\MechWarrior3\zbd` and its one file is
named bare).

## The data (0x7f7f)

A u32, then each file at its offset from the end of that word:
`[u8 method][stream]`. zlib and bzip2 both occur in one installer (PM 1.3: 54
bzip2, 12 zlib).

## What the reader checks

It reads a file from the internet. Every size and offset is checked against
the file (64-bit sums, so none wraps); at most 256 blocks, 20,000 files, a
16 MB file list, 2 GB a file. A name must stay inside the folder
(`mod_entry_path_ok`: no absolute path, drive, `..`, control characters or a
name Windows cannot hold), and two names that differ only in case are
refused. Each file must unpack to exactly its declared size: zlib through
miniz's inflate into a buffer of that size; bzip2 through our own reader, which
stops at the declared size, checks every table, index and run against its
bounds, and checks each block's CRC and the stream's. Tests:
`installer_read_lists_and_extracts`, `installer_read_refusals` (every
truncation of a made-up installer, sizes and offsets past the end, paths that
leave the folder, a method byte that is none, a stream past its declared
size), `bz2_read_refuses_what_is_wrong` (every truncation and single-bit flip
of a small stream), `tests/differential/test_bz2_read.py` (3,000 inputs packed
by Python's bz2 at every level, several-block streams, about 9,000 hostile
streams); the unit tests and the reader over the 15 real installers were run
clean under AddressSanitizer (2026-11-10).

bzip2 (`bz2_read.cpp`): one stream (`BZh1`..`BZh9`), its blocks, the
run-length stages, the Huffman tables (up to 6, lengths up to 20), the
move-to-front and the inverse transform, written from the format. A
randomised block (an old option) is refused. Bytes after the stream's end are
not read; no installer entry in the 15 holds two streams.
