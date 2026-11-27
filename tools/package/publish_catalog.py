"""The games catalog signed and staged for the web (docs/launcher.md, Where the launcher looks; DEPLOY.md):
catalog/games.json signed (catalog/games.json.sig, committed beside it on the hub's main, so GitHub's raw copy and
the mirror's carry it), and <folder>/catalog.json, catalog.json.sig and art/ written for the site's /launcher/ path,
where the web server serves them as plain files. With MW3_CATALOG_DEST set (an rsync destination), the folder is
copied there too; no address is kept in the repo.

    MW3_SIGNING_KEY=<key file> python3 -I tools/package/publish_catalog.py <folder>
"""
import json
import os
import pathlib
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import sign  # noqa: E402


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    repo = HERE.parent.parent
    cat = repo / 'catalog' / 'games.json'
    text = cat.read_bytes()
    doc = json.loads(text)  # a broken catalog is never signed
    if doc.get('catalog') != 1 or not isinstance(doc.get('games'), list):
        sys.exit(f'{cat}: not a catalog')
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
    print(f'{cat}.sig written (commit it with games.json); {out}: catalog.json, catalog.json.sig, art/ (revision {doc.get("revision")})')
    dest = os.environ.get('MW3_CATALOG_DEST')
    if dest:
        subprocess.run(['rsync', '-a', str(out) + '/', dest], check=True)
        print(f'copied to MW3_CATALOG_DEST')


if __name__ == '__main__':
    main()
