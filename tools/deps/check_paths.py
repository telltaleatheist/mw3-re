#!/usr/bin/env python3
# Fails when any file under a package folder carries a local path or the
# building user's name (release.md, Privacy): every byte of every file, as
# ASCII and as UTF-16, so a library's built-in strings count too (FFmpeg's
# configure line once carried the build folder). The public GitHub address
# (github.com/<user>, and the updater's api.github.com/repos/<user>) is the one
# allowed use of the name. A compiler may store that address in overlapping
# pieces (GCC's immediate stores of a short string: "...repos/tel" then
# "telltale..."), so the name may follow the address and a first part of
# itself (2026-11-21, 1.2.2's Linux mw3pm).
# Usage: check_paths.py <folder> [extra name ...]   (make_app.sh, make-appimage.sh)
import getpass, os, re, sys

root = sys.argv[1]
names = {n for n in [getpass.getuser(), os.environ.get('SUDO_USER', ''),
                     os.path.basename(os.path.expanduser('~'))] + sys.argv[2:] if len(n) >= 4}
words = [b'/Volumes/', b'/Users/', b'/home/', b'C:\\Users\\', b'C:/Users/'] + [n.encode() for n in sorted(names)]
pat = re.compile(b'|'.join(re.escape(w) for w in words))
pat16 = re.compile(b'|'.join(re.escape(w.decode().encode('utf-16-le')) for w in words))
# The backup's addresses too (2026-11-22, docs/launcher.md, Two homes): git.owenmorgan.com/<user> and the
# updater's .../api/v1/repos/<user>, and the name inside that host name itself. The games catalog's
# raw.githubusercontent.com/<user> too (2026-11-26, docs/launcher-split.md, b7).
allowed = re.compile(rb'(?:github\.com/(?:repos/)?|raw\.githubusercontent\.com/|git\.owenmorgan\.com/(?:api/v1/repos/)?)'
                     rb'([A-Za-z0-9-]{0,16})$', re.I)
BACKUP_HOST = b'git.owenmorgan.com'


def in_backup_host(data, m):
    lo = max(0, m.start() - len(BACKUP_HOST))
    at = data[lo:m.end() + len(BACKUP_HOST)].lower().find(BACKUP_HOST)
    return at != -1 and lo + at <= m.start() and m.end() <= lo + at + len(BACKUP_HOST)

bad = []
for d, _, files in os.walk(root):
    for f in files:
        p = os.path.join(d, f)
        if os.path.islink(p):
            continue
        data = open(p, 'rb').read()
        for m in pat.finditer(data):
            a = allowed.search(data[max(0, m.start() - 48):m.start()])
            if a and m.group().lower().startswith(a.group(1).lower()):
                continue
            if in_backup_host(data, m):
                continue
            ctx = data[max(0, m.start() - 30):m.end() + 50]
            bad.append(f"{os.path.relpath(p, root)}: {ctx.decode('latin-1')!r}")
            break
        else:
            if pat16.search(data):
                bad.append(f"{os.path.relpath(p, root)}: (UTF-16)")
for b in bad:
    print('local path or user name in the package:', b)
if bad:
    sys.exit(1)
print(f"check_paths: no local path or user name in {root}")
