#!/usr/bin/env python3
"""Builds the release zip from this source: the stand-in, built with mingw and checked against the hash that was tested
on real games, under a name a paste can't put over the game's own DLL.

    python tools/weapon-patch/package/make_zip.py <version> [--out DIR]
"""
import argparse
import hashlib
import sys
import tempfile
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOLS = HERE.parent
sys.path.insert(0, str(TOOLS))
import proxy_install as pi  # noqa: E402

# The DLL every real run has used. A different hash means a new DLL, whose testing starts again.
DLL_SHA256 = '126ddb4eeca0fe1a0c42ef3b6a8d5cc987f238ea95a3b89abc9796cf5ab022d1'
DLL_SIZE = 10752


def crlf(data):
    return data.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')


def make(version, out_dir):
    with tempfile.TemporaryDirectory(dir=out_dir) as folder:
        dll_path = Path(folder) / 'IFORCE2.dll'
        pi.build(dll_path)
        dll = dll_path.read_bytes()
    digest = hashlib.sha256(dll).hexdigest()
    if digest != DLL_SHA256 or len(dll) != DLL_SIZE:
        raise SystemExit(f'the built DLL is {len(dll)} bytes, sha256 {digest}: not the tested one, {DLL_SHA256}')
    files = {'INSTALL.txt': crlf((HERE / 'INSTALL.txt').read_bytes()),
             'install.cmd': crlf((HERE / 'install.cmd').read_bytes()),
             'restore.cmd': crlf((HERE / 'restore.cmd').read_bytes()),
             'install-files/IFORCE2-standin.dll': dll,
             'install-files/weapon-rows.txt': crlf((TOOLS / 'rows-plain-ppc.txt').read_bytes()),
             'README.md': (TOOLS / 'README.md').read_bytes(),
             'WRITEUP.md': (TOOLS / 'WRITEUP.md').read_bytes(),
             'tools/proxy_install.py': (TOOLS / 'proxy_install.py').read_bytes(),
             'tools/patch_weapons.py': (TOOLS / 'patch_weapons.py').read_bytes(),
             'tools/rows-plain-ppc.txt': (TOOLS / 'rows-plain-ppc.txt').read_bytes(),
             'tools/proxy/iforce2_proxy.c': (TOOLS / 'proxy' / 'iforce2_proxy.c').read_bytes(),
             'tools/proxy/iforce2_proxy.def': (TOOLS / 'proxy' / 'iforce2_proxy.def').read_bytes()}
    path = out_dir / f'mw3-weapon-patch-{version}.zip'
    with zipfile.ZipFile(path, 'x', zipfile.ZIP_DEFLATED) as z:
        for name, data in files.items():
            info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))  # the same zip from the same source
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, data)
    return path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('version')
    parser.add_argument('--out', type=Path, default=Path('.'))
    args = parser.parse_args()
    path = make(args.version, args.out)
    print(path, hashlib.sha256(path.read_bytes()).hexdigest())


if __name__ == '__main__':
    main()
