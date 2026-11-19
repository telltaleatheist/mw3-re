#!/usr/bin/env python3
"""mw1art.py KEEP_DIR OUT_DIR: MW1's screen art for the repo (data-src/art/), from our generated art.

KEEP_DIR is the remake's keep folder (assets/mw3/*.png, ours from Crucible, nothing of MW1's or MW3's, and
planets.csv). Run by hand when the art changes; the build copies OUT_DIR's files into mw1art.zbd (tools/mw1imgpack.cpp).

Each picture is kept at its source's size (research/hires-2d.md B2), cut only to its use's shape (the middle kept), so
the 2D can draw it at the window's resolution; its canvas-size 565 copy (the 480-line canvas: mw3-re
docs/resolution.md, sections 9 and 12) is made from the same file when it loads (src/mw1_art.h). Scenes and portraits
are JPEG at quality 90, the tiles' pictures and the emblem PNG (hard edges).
- a scene (1344x768, or a 512 square used as one): cut to the canvas's 854:480, shown 854x480 (the middle 640 at 4:3);
- a service tile's picture (icon_*): 100x100 (framed at pack time, mw1_chrome.h);
- a portrait or a mech picture (768x1024): 240x320, and a small one, <name>_s, 96x128 (the crew grid's), one file;
- an emblem (story_darkwing_crest): 240x240.
- a results picture (FIXES.md item 31, Owen's picks from KEEP_DIR/assets: ENDINGS): kept whole (8:7), shown 192x168
  at 4:3, and <name>_l, 240x210, at 16:9 (item 38); one file.
art.csv lists each canvas image: its name, its file and its canvas size.
The intro's unused takes (intro_0_*), the mechs' pictures and the hub's first scenes (SKIP) are left out.

scenery.csv: each world's scenery (hub_<type>), by MW1's planet number: our classification only (MW1's descriptions,
which it was read from, stay out of the repo).
"""
import csv
import os
import sys

from PIL import Image

SCENE = (854, 480)
TILE = (100, 100)
PORTRAIT, PORTRAIT_SMALL = (240, 320), (96, 128)
EMBLEM = (240, 240)
ENDING, ENDING_LARGE = (192, 168), (240, 210)
# The results screen's pictures (FIXES.md item 31): Owen's picks (2026-11-18), from KEEP_DIR/assets.
ENDINGS = {"end_win": "end_win_22.png", "end_retreat": "end_retreat_22.png", "end_lost": "end_lost_11.png"}
SKIP_PREFIX = ("intro_0_", "mech_")  # the mechs show as PM's icons (FIXES.md items 23, 27)
# The hub's first scenes, by our world type: replaced by hub_<landscape>_<setting> (FIXES.md item 6).
SKIP = {"hub_" + t for t in ("agri", "capital", "culture", "frontier", "ice", "industrial", "jungle", "military", "mining",
                             "ocean", "pleasure", "temperate", "trade")}
EMBLEMS = {"story_darkwing_crest"}
# The menu-title choice (FIXES.md item 86, waiting on Owen): title_bg recut 1.25 times closer from its top left, the
# 'Mech's head moved right and down, clear of the title (mw1menu.zrd's MW1MENUDIALOG_LOW).
TITLE_LOW_ZOOM = 1.25
PORTRAIT_PREFIX = ("recruit_", "char_", "liaison_")
# FIXES.md items 50 and 56: the office's liaison, as wide as its buttons, from under LEAVE down to the button row's
# foot: <name>_office, its own file, the portrait's whole height and the middle of its width, never stretched.
OFFICE_PORTRAIT = (160, 278)
PORTRAITS = {"bartender_portrait", "office_liaison_portrait"}


def cut(im, size):
    """The largest middle of `im` in `size`'s shape (no resampling)."""
    w, h = im.size
    if w * size[1] > h * size[0]:
        cw, ch = round(h * size[0] / size[1]), h
    else:
        cw, ch = w, round(w * size[1] / size[0])
    x, y = (w - cw) // 2, (h - ch) // 2
    return im.crop((x, y, x + cw, y + ch))


def kind(name, size):
    if name in EMBLEMS:
        return "emblem"
    if name.startswith("icon_"):
        return "tile"
    if name.startswith(PORTRAIT_PREFIX) or name in PORTRAITS:
        return "portrait"
    return "scene"


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: mw1art.py KEEP_DIR OUT_DIR")
    keep, out = sys.argv[1], sys.argv[2]
    src = os.path.join(keep, "assets", "mw3")
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):  # the art is remade whole: a picture removed goes
        if f.endswith((".png", ".jpg")):
            os.remove(os.path.join(out, f))
    made, rows = [], []
    for f in sorted(os.listdir(src)):
        if not f.endswith(".png"):
            continue
        name = f[:-4]
        if name.startswith(SKIP_PREFIX) or name in SKIP:
            continue
        im = Image.open(os.path.join(src, f)).convert("RGB")
        k = kind(name, im.size)
        sizes = {"scene": [("", SCENE)], "tile": [("", TILE)], "emblem": [("", EMBLEM)],
                 "portrait": [("", PORTRAIT), ("_s", PORTRAIT_SMALL)]}[k]
        im = cut(im, sizes[0][1])
        if k in ("scene", "portrait"):
            file = name + ".jpg"
            im.save(os.path.join(out, file), quality=90, optimize=True)
        else:
            file = name + ".png"
            im.save(os.path.join(out, file), optimize=True)
        made.append(os.path.join(out, file))
        for suffix, size in sizes:
            rows.append([name + suffix, file, size[0], size[1]])
        if name == "title_bg":
            low = im.crop((0, 0, round(im.size[0] / TITLE_LOW_ZOOM), round(im.size[1] / TITLE_LOW_ZOOM)))
            lfile = "title_bg_low.jpg"
            low.save(os.path.join(out, lfile), quality=90, optimize=True)
            made.append(os.path.join(out, lfile))
            rows.append(["title_bg_low", lfile, SCENE[0], SCENE[1]])
        if name.startswith("liaison_"):
            office = cut(im, OFFICE_PORTRAIT)
            ofile = name + "_office.jpg"
            office.save(os.path.join(out, ofile), quality=90, optimize=True)
            made.append(os.path.join(out, ofile))
            rows.append([name + "_office", ofile, OFFICE_PORTRAIT[0], OFFICE_PORTRAIT[1]])
    for name, f in ENDINGS.items():
        im = cut(Image.open(os.path.join(keep, "assets", f)).convert("RGB"), ENDING)
        file = name + ".jpg"
        im.save(os.path.join(out, file), quality=90, optimize=True)
        made.append(os.path.join(out, file))
        rows.append([name, file, ENDING[0], ENDING[1]])
        rows.append([name + "_l", file, ENDING_LARGE[0], ENDING_LARGE[1]])
    rows.sort(key=lambda r: r[0])
    with open(os.path.join(out, "art.csv"), "w", newline="") as o:
        w = csv.writer(o, lineterminator="\n")
        w.writerow(["image", "file", "w", "h"])
        w.writerows(rows)
    # The worlds' scenery: number and type only.
    with open(os.path.join(keep, "planets.csv"), newline="") as f, open(os.path.join(out, "scenery.csv"), "w", newline="") as o:
        w = csv.writer(o, lineterminator="\n")
        w.writerow(["n", "scenery"])
        for row in csv.DictReader(f):
            w.writerow([row["n"], row["scenery"]])
    total = sum(os.path.getsize(p) for p in made)
    print(f"{len(made)} pictures, {total / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
