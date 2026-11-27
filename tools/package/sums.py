"""SHA256SUMS over a release's files (docs/release.md, The combined package): every file in the folder but SHA256SUMS
itself, sorted by name, one "<64 hex>  <name>" line each, as sha256sum writes them and the launcher's updater reads
them. Run it on the folder that holds every platform's files before they are uploaded. With MW3_SIGNING_KEY set
(tools/package/sign.py), SHA256SUMS.sig is written beside it, which every home's copy is then trusted by
(docs/launcher.md, Signed data); upload it with the rest.

    MW3_SIGNING_KEY=<key file> python3 -I tools/package/sums.py <folder>
"""
import os
import hashlib
import pathlib
import sys


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    folder = pathlib.Path(sys.argv[1])
    lines = []
    for f in sorted(p for p in folder.iterdir() if p.is_file() and p.name not in ('SHA256SUMS', 'SHA256SUMS.sig')):
        h = hashlib.sha256()
        with f.open('rb') as fh:
            for chunk in iter(lambda: fh.read(1 << 20), b''):
                h.update(chunk)
        lines.append(f'{h.hexdigest()}  {f.name}\n')
    if not lines:
        sys.exit(f'{folder}: no files')
    (folder / 'SHA256SUMS').write_text(''.join(lines), newline='\n')
    print(f'{folder / "SHA256SUMS"}: {len(lines)} files')
    if os.environ.get('MW3_SIGNING_KEY'):
        sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
        import sign
        print(sign.sign_file(folder / 'SHA256SUMS', sign.keys()))
    else:
        print('not signed (MW3_SIGNING_KEY not set): only GitHub\'s copy of this release will install')


if __name__ == '__main__':
    main()
