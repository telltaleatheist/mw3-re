# Damage decals (bullet holes)

Each hit on a mech paints a bullet hole into the texture the shot hit:
a hole-shaped piece of the matching damage texture is stamped where the
shot landed. Ported 2026-10-29 (`mech3/mech_skin.cpp` `SkinSet::damage_decal`,
`gamez/zmodel/gmod_hit.cpp` `zmod::cookie`, `gamez/zimage/zimg_texture.cpp`
the image cache); checked by `tests/differential/test_decal.py` and
`mission_c1m2_damage_decals_are_painted_headless`.

## The call

Damage (0x0041af10, `damage.md` step 6): with the hit's collision record
(+0x20 its material), the mech's skin copy (+0x3e8) and the pilot's roster
entry, 0x00414370 gives the mech definition's skin set (+0x1c) and
0x00474d90 runs on it (ECX the set; stack the skin, the collision, the
damage, `ret 0xc`; the damage is not read). In the program it is
`DamageCalls::roster_x1c` and `damage_decal` (`app_mech.cpp`).

## 0x00474d90 (SkinSet::damage_decal)

1. The damage skin linked (0x00475520, once per set); none: done.
2. No holes loaded (the vector at 0x006061a8, first [0x006061ac], last
   [0x006061b0]): done.
3. The mech's skin entry for the hit material's texture (+0x10):
   0x00474cd0 compares each entry's texture with it, a cached entry's as
   null (a cached entry holds an image, not a table entry, so it never
   matches). It needs a damage partner (+0x10) with a texture.
4. A hole picked: `ftol(rand() * (n - 1) * 3.0518509e-05)` (1/32767), so
   the last of the eight is picked only when rand() is 32767: in effect
   holes 0..6.
5. The cookie (below) with the entry's table entry (null when cached),
   the partner's image and the hole. When it paints and the entry has a
   device texture (+4), the device copy is refreshed ([0x0072f374],
   0x0057baa0).

## 0x00542730 (the cookie, zModel\gmod_cookie.cpp)

- Needs the target, its image and pixels, the source's pixels and the
  hole's alpha; else reports 0x200 (line 0x54) and returns 0. The two images
  must be the same size and neither palettized (line 0x61).
- The UV ([0x0074a2c0], [0x0074a2c4]) is wrapped into -0.01..1.01 by
  steps of 1 and written back. It is the hit's: with HOLES a material
  whose skin entry has a damage partner is marked flag 2
  (`mechdef-skins.md`), and a flag-2 polygon is crossed with its texture
  coordinates (0x0054dd30), which are always stored
  (`movement-collision.md`). The capture switch [0x0074a2ac] gates only
  the scripts' texture marks (0x00542060, `weapon-effects.md`).
- The centre is `ftol(w * u), ftol(h * v)`, kept half a hole inside the
  image; the hole's rectangle is visited row by row, one mask byte a pixel.
- The 2D target's format [0x0074a738] picks the arithmetic: 6 is 565
  (mask over 3 blends), else 555 (over 7). From 0xfc the source texel is
  copied first, then blended with itself. 565's green keeps its sign in
  bit 5: a darkening step sets it (`gmod_hit.cpp`).

## The image cache (zimg_library.cpp, 0x005179d0 / 0x00517b10)

The holes and the damage skin's textures come from zImage's named image
cache: fetch by name (refcount 1, or one more), release by image (freed at
0, or at once when not cached; with no cache yet it reports "lazy
programmer alert!"). It had not been ported, so until now no hole and no
damage texture loaded: the skins' links reported "not loaded".

## HOLES and the install

HOLES ([0x006061cc]) is set by the mission load (0x0048faf0) on a 3D card
when it uses the RGB textures: InstallOptions bit 2 or 4 (the 4 MB or 8 MB
card set installed) and a card of 8 MB or more. With neither bit the mech
textures load reduced (variant 0x11: `rmechtex16.zbd`) and there are no
holes. The port's InstallOptions is now a full install (0x070707,
`settings.md`, `fixes.md`), so the mechs use `rmechtex.zbd` and get holes.

## Checks

- `test_decal.py`: the cookie on random images, UVs, sizes and both formats
  (3000 stamps), and the decal on random skins (entries cached or not,
  partners with and without textures, hole counts, seeds; 3000 hits), the
  exe and the port identical. Breaks caught: the 565 green sign bit, the
  555 threshold, the hole pick's n - 1, a cached entry's texture matched.
- `mission_c1m2_damage_decals_are_painted_headless`: c1m2 on the GPU path,
  the player taken to each enemy mech: 8 holes loaded, about 150 decals in
  3600 frames, on the Mad Cat's `mcat_d0x` textures, at the hits' UVs.
