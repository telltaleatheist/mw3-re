# Analysis tools

Scripts used to map `Mech3.exe` 1.2. They write into `analysis-output/`
(git-ignored) and are run from there. REA needs the Node 24 install noted in
CLAUDE.md; Ghidra headless needs `JAVA_HOME` as noted there.

| Script | What it does |
|---|---|
| `readaemon.py <log-prefix>` | Starts one `rea mcp` session on Mech3.exe (Ghidra, low priority) and serves tool calls on 127.0.0.1:47123, so each question takes under a second instead of an 80 s re-import. Run it in the background. |
| `q.py <tool> [json-args]` | One REA tool call through the daemon, e.g. `q.py procedure_callers '{"procedure":"0x5ac0b0"}'`. |
| `dumpdecomp.py procs.json decomp` | Decompiles every function through REA's `batch_decompile` into `decomp/<addr>.c` (about 3 minutes). `procs.json` comes from `q.py list_procedures`. |
| `vtscan.py` | Finds code pointers in the data sections (vtables, callback tables) whose targets Ghidra never made functions, into `undiscovered.json`. |
| `../ghidra/CreateAndDecompile.java` | Ghidra headless post-script: creates those functions and decompiles them into the same `decomp/`. REA cannot create functions, so this runs Ghidra directly. |
| `filemap.py` | Maps each original source-file path string (`C:\zipper\...`) to the functions that reference it (`filemap.json`). Needs `strings.json` from `q.py list_strings`. |
| `modmap.py` | Assigns every function to a source file by link-order ranges (`func2file.json`); its output is `docs/module-map.md`. |
| `calls.py <addr>` | The functions one function calls, labelled with their source file and strings. |
| `pe.py` | Reads 32-bit values and C strings from the exe by address (`pe.py vt <addr> <n> [other]` compares vtables). |
| `zrd.py <archive.zbd> [name.zrd]` | Lists a `.zbd` archive, or prints one reader file as nested lists. |
| `rdrkeys.py [filter]` | Every reader lookup in the exe: key, getter, where the value is stored, function and source file. |
| `fields.py <vtable>` | Recovers an object's field layout from how its methods use `this` (offsets, widths, float use, functions). |
| `msgtable.py [id ...]` | The message table in `Mech3Msg.dll`, the strings the game formats by id. |
| `typesfull.py`, `fpconsts.py` | Inputs for `../ghidra/ApplyTypes.java`: `docs/types.json` with placeholder fields, and the float constants in `.rdata` (`docs/structs.md`). |
| `../ghidra/ApplyTypes.java` | Ghidra headless post-script: applies the structs, names, methods and signatures, then writes typed decompilations to `decomp-typed/`. |
| `tex2png.py <library.zbd> <outdir> [filter]` | Exports textures from `rimage.zbd` / `rmechtex*.zbd` as PNG (`docs/textures.md`). |
| `keycheck.py` | Reader keys the exe looks up that no data file contains: optional, dead or misspelt (`docs/discoveries.md`). |
| `isols.py`, `isoget.py` | List or extract an ISO 9660 image (the disc), without mounting it. |
| `peimports.py <exe>` | Lists a PE's imported DLLs and functions. |

`objdump -d --x86-asm-syntax=intel Mech3.exe` (Apple's, from Xcode) gives the
whole disassembly in a few seconds; the analysis keeps it as `mech3.asm`.
