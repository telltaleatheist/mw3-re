"""The games catalog signed and staged for the web (docs/launcher.md, Where the launcher looks; DEPLOY.md):
every art a game names (badge, heading_badge, card) looked up in catalog/art, its SHA-256 written beside it in
catalog/games.json (badge_sha256 and so on; a stale one corrected, said) so the signature vouches for the art too,
a named art missing from catalog/art refused; then catalog/games.json signed (catalog/games.json.sig, committed beside it on the hub's main, so GitHub's raw copy and
the mirror's carry it), and <folder>/catalog.json, catalog.json.sig and art/ written for the site's /launcher/ path,
where the web server serves them as plain files. With MW3_CATALOG_DEST set (an rsync destination), the folder is
copied there too; no address is kept in the repo.

    MW3_SIGNING_KEY=<key file> python3 -I tools/package/publish_catalog.py <folder>
"""
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import sign  # noqa: E402


ART = ('badge', 'heading_badge', 'card')


def art_sums(cat, art_dir):
    """catalog/games.json's text with each art's sum filled in or corrected (in place, its layout kept)."""
    text = cat.read_text()
    games = json.loads(text).get('games', [])
    sums, changed = {}, []
    for g in games:
        for field in ART:
            name = g.get(field)
            if not name:
                continue
            f = art_dir / name
            if not f.is_file():
                sys.exit(f'{cat}: games ({g.get("key")}): its {field} {name} is not in {art_dir}: add it, or the launcher never fetches it')
            sums[(field, name)] = hashlib.sha256(f.read_bytes()).hexdigest()
    for (field, name), s in sums.items():
        pat = re.compile(r'(?<![A-Za-z_])("%s"\s*:\s*"%s")(\s*,\s*"%s_sha256"\s*:\s*"([^"]*)")?' % (field, re.escape(name), field))
        def put(m):
            if m.group(2) is None or m.group(3).lower() != s:
                changed.append(f'{field} {name}: ' + ('added' if m.group(2) is None else f'was {m.group(3)}'))
            return f'{m.group(1)}, "{field}_sha256": "{s}"'
        text = pat.sub(put, text)
    for g in json.loads(text).get('games', []):  # what will be signed says exactly that
        for field in ART:
            if g.get(field) and g.get(field + '_sha256') != sums[(field, g[field])]:
                sys.exit(f'{cat}: games ({g.get("key")}): {field}_sha256 could not be written')
    if changed:
        cat.write_text(text, newline='\n')
        for c in changed:
            print(f'{cat}: {c}')
    return text.encode()


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    repo = HERE.parent.parent
    cat = repo / 'catalog' / 'games.json'
    doc = json.loads(cat.read_bytes())  # a broken catalog is never signed
    if doc.get('catalog') != 1 or not isinstance(doc.get('games'), list):
        sys.exit(f'{cat}: not a catalog')
    text = art_sums(cat, repo / 'catalog' / 'art')
    ks = sign.keys()
    sign.sign_file(cat, ks)
    out = pathlib.Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    (out / 'catalog.json').write_bytes(text)
    shutil.copyfile(str(cat) + '.sig', out / 'catalog.json.sig')
    if (repo / 'catalog' / 'art').is_dir():
        shutil.copytree(repo / 'catalog' / 'art', out / 'art', dirs_exist_ok=True)
    for k in ks:
        if not sign.check(out / 'catalog.json', [sign.public_hex(k)]):
            sys.exit('the signature just written does not check')
    print(f'{cat}.sig written (commit it with games.json and its art sums); {out}: catalog.json, catalog.json.sig, art/ (revision {doc.get("revision")})')
    dest = os.environ.get('MW3_CATALOG_DEST')
    if dest:
        subprocess.run(['rsync', '-a', str(out) + '/', dest], check=True)
        print(f'copied to MW3_CATALOG_DEST')


if __name__ == '__main__':
    main()
