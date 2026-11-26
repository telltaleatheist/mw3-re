"""SHA256SUMS over a release's files (docs/release.md, The combined package): every file in the folder but SHA256SUMS
itself, sorted by name, one "<64 hex>  <name>" line each, as sha256sum writes them and the launcher's updater reads
them. Run it on the folder that holds every platform's files before they are uploaded.

    python3 -I tools/package/sums.py <folder>
"""
import hashlib
import pathlib
import sys


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    folder = pathlib.Path(sys.argv[1])
    lines = []
    for f in sorted(p for p in folder.iterdir() if p.is_file() and p.name != 'SHA256SUMS'):
        h = hashlib.sha256()
        with f.open('rb') as fh:
            for chunk in iter(lambda: fh.read(1 << 20), b''):
                h.update(chunk)
        lines.append(f'{h.hexdigest()}  {f.name}\n')
    if not lines:
        sys.exit(f'{folder}: no files')
    (folder / 'SHA256SUMS').write_text(''.join(lines), newline='\n')
    print(f'{folder / "SHA256SUMS"}: {len(lines)} files')


if __name__ == '__main__':
    main()
