# The game's stand-in fonts

The original's text is drawn with the install's `arial.ttf`, `impact.ttf` and
`lucon.ttf`. For an install without them, every package carries these free
faces, unchanged, under the SIL Open Font License 1.1 (each one's licence is
beside it; "Liberation" and "Anton" are reserved names):

| the game's | stand-in | width against the game's | source |
| --- | --- | --- | --- |
| arial.ttf | LiberationSans-Regular.ttf | the same (metric-compatible) | liberationfonts/liberation-fonts 2.1.5 |
| impact.ttf | Anton-Regular.ttf | about 2% narrower | github.com/google/fonts `ofl/anton` |
| lucon.ttf | LiberationMono-Regular.ttf | about 0.5% narrower | liberationfonts/liberation-fonts 2.1.5 |

`SdlTwodHost::font_path` picks them; `open_ttf` sizes a stand-in by the game
face's own metrics, so the lines keep their heights (docs/twod.md section 6).
