"""The release gate for the built-in games catalog (DEPLOY.md, A launcher release, step 1): the launcher's built-in
copy (src/upgrades/update/games.json) must be the live one (catalog/games.json, as published and signed), and every
badge it names must ship with the launcher (assets/launcher/<name>, at its sha256). A launcher built with an older
built-in shows an older list of games on any start that has no signed cache yet (2026-11-27: 1.8.0 shipped
revision 1, with no Revenge, while revision 4 was live).

    python3 -I tools/package/check_catalog.py [<checkout>]   exit 0 when it holds; else 1, saying what to do
"""
import hashlib
import json
import pathlib
import sys

ART = ('badge', 'heading_badge', 'card')


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else '.')
    built_in_path = root / 'src' / 'upgrades' / 'update' / 'games.json'
    live_path = root / 'catalog' / 'games.json'
    built_in = json.loads(built_in_path.read_text())
    live = json.loads(live_path.read_text())
    bad = []
    if built_in != live:
        bad.append(f'the built-in catalog (revision {built_in.get("revision")}) is not the live one (revision '
                   f'{live.get("revision")}): copy {live_path} over {built_in_path} (after publish_catalog.py has '
                   f'written its art sums) and commit it')
    carried = root / 'assets' / 'launcher'
    for g in built_in.get('games', []):
        for field in ART:
            name = g.get(field)
            if not name:
                continue
            f = carried / name
            if not f.is_file():
                bad.append(f'{g.get("key")}: its {field} {name} is not in {carried}: copy it from catalog/art')
                continue
            want = g.get(field + '_sha256')
            if want and hashlib.sha256(f.read_bytes()).hexdigest() != want:
                bad.append(f'{g.get("key")}: {carried / name} is not the catalog\'s {field}_sha256: copy it from catalog/art')
    if bad:
        for b in bad:
            print(f'check_catalog: {b}', file=sys.stderr)
        sys.exit(1)
    print(f'check_catalog: the built-in catalog is the live one (revision {built_in.get("revision")}), '
          f'its badges shipped')


if __name__ == '__main__':
    main()
