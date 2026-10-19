# Texture libraries

Mech3.exe 1.2, 2026-10-19. The standalone texture files (`rimage.zbd`,
`rmechtex.zbd`, `rmechtex16.zbd`, `rmechtexs.zbd`) are read by the code between
`zimg_library.cpp` and `zimg_texture.cpp`. Ported to `src/gamez/zimage/
zimg_library.*`; `tests/differential/test_textures.py` runs the original open
and load on all 10,612 textures in the four files (and once with a 15-bit
display) and gets byte-identical results. The layout matches mech3ax
(`crates/image`).

## File

```
Header (0x18): zero, has_entries (must be 1), global palette count, texture count, zero, zero
Entry[count] (0x28): name[32], offset, global palette index (-1 for none)
global palettes: 256 x RGB565 each (0x200 bytes)
textures, at their offsets:
  info (0x10): flags u32, width u16, height u16, u32, palette count u16, stretch u16
  pixels: RGB565 per pixel, or one byte per pixel when palette count != 0
  alpha: one byte per pixel, when flags has 0x08
  palette: palette count x RGB565, when palettised and not using a global one
```

Flags: 0x01 two bytes per pixel, 0x02 alpha, 0x04 no alpha, 0x08 full alpha
plane, 0x10 global palette; 0x20/0x40/0x80 mark buffers the game allocated.
Only the low byte of the flags word is kept in memory.

## Loading

The game registers libraries by base name at mission load (0x48fcfe: `texture`,
then `mechtex` or `mechtexS` for low detail) with 0x5182a0. The file is found
on first use (0x518a30): with the `r` prefix it tries `r<name>.zbd`; failing
that, `r<name>8.zbd` down to `r<name>0.zbd`, then the plain name again. A
library given a variant number tries `r<name><n>.zbd` from n down to 0. The
first file that exists wins.

0x5187c0 opens the file, reads the header and entries, and appends the global
palettes to one table shared by all libraries (0x638510, count 0x63850c); each
library remembers where its palettes start. 0x518db0 finds a texture by name
(`_stricmp`), seeks to it and reads it (0x517f80). Textures that use a global
palette point at the shared copy.

On a 15-bit (RGB555) display every RGB565 colour, in pixels and palettes, is
converted as it loads: green drops its lowest bit. Palettised textures also get
32 shaded copies per light table for the software renderer (0x519740); with no
light tables (hardware rendering) nothing is added. The port reads the display
format from `zimg::State` and leaves the shading out.

## Original behaviour

- A texture whose pixel read fails is still returned (0x517f80 ignores the
  result), and `load` then writes its global palette through it unchecked.
- Asking `read_data` (0x517df0) for more bytes per pixel than the texture has
  succeeds without reading; asking for fewer fails.
- A library whose palettes fail to read keeps its slots in the shared count.
- The library name is used as a `sprintf` format (0x5182a0); harmless, since
  the names are constants without `%`.
