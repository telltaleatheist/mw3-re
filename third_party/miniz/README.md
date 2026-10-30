# miniz (tinfl only)

The inflate half of Rich Geldreich's miniz, MIT licensed (`LICENSE`).
Unchanged copies of `miniz_tinfl.c`, `miniz_tinfl.h` and `miniz_common.h`
from https://github.com/richgel999/miniz at tag 3.0.2, sha256:

- `miniz_tinfl.c` 9b7edc1fab708166e10f5e90a618a91c95a91496c4692e38160fdae810c1149a
- `miniz_tinfl.h` 040aad2d9ca23c487652c150277d950be12e72c5453ae1f7c6713ed2efd16b4e
- `miniz_common.h` 8b60e76e726dfcf41a338c3a4c54c70038041a21e6314aa54fd3b7f729ea3412
- `LICENSE` 0115478d567121238cf6cc1c0c361926cf07a49d9e4c9e66da97fac6a01646b3

`miniz.h` and `miniz_export.h` are ours, two-line stand-ins for the full
library's header and the export header its CMake generates.

The rebuild inflates the official 1.2 patch with it: the InstallShield
cabinet's chunks and the ZIP the patch may come in (docs/assets.md, The 1.2
patch; `src/mech3/patch_import.cpp`).
