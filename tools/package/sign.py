"""Ed25519 signatures for what the launcher fetches (docs/launcher.md, Signed data): a file's detached <file>.sig,
one line of 128 hex digits per key, each a signature of the whole file. The launcher (and the setup kit) check them
against the public keys in assets/launcher/sources.json.

The private key never goes in a repo: MW3_SIGNING_KEY names its file (a PEM, as `--new-key` writes and
`openssl genpkey -algorithm ed25519` does). Several files, separated by the system's path separator (':' or ';'),
sign with each: the old key and the next while keys move.

    python3 -I tools/package/sign.py <file> [<file> ...]   write each <file>.sig
    python3 -I tools/package/sign.py --public              the keys' public halves, for sources.json's "keys"
    python3 -I tools/package/sign.py --new-key <path>      a new private key at <path> (refuses an existing file)
    python3 -I tools/package/sign.py --check <file> <public key hex> [...]   whether <file>.sig checks

Needs Python's `cryptography` package (pip install cryptography).
"""
import os
import pathlib
import sys

try:
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey
    from cryptography.exceptions import InvalidSignature
except ImportError:
    sys.exit('sign.py needs the cryptography package: pip install cryptography')


def keys():
    paths = [p for p in os.environ.get('MW3_SIGNING_KEY', '').split(os.pathsep) if p]
    if not paths:
        sys.exit('MW3_SIGNING_KEY names no key file (the private key, kept outside every repo)')
    out = []
    for p in paths:
        key = serialization.load_pem_private_key(pathlib.Path(p).read_bytes(), password=None)
        if not isinstance(key, Ed25519PrivateKey):
            sys.exit(f'{p}: not an Ed25519 key')
        out.append(key)
    return out


def public_hex(key):
    return key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw).hex()


def sign_file(path, ks):
    data = pathlib.Path(path).read_bytes()
    sig = ''.join(k.sign(data).hex() + '\n' for k in ks)
    pathlib.Path(str(path) + '.sig').write_text(sig, newline='\n')
    return str(path) + '.sig'


def check(path, publics):
    data = pathlib.Path(path).read_bytes()
    lines = [l.strip() for l in pathlib.Path(str(path) + '.sig').read_text().splitlines() if l.strip()]
    for pub in publics:
        k = Ed25519PublicKey.from_public_bytes(bytes.fromhex(pub))
        for l in lines:
            try:
                k.verify(bytes.fromhex(l), data)
                return pub
            except (InvalidSignature, ValueError):
                pass
    return None


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    if a[0] == '--public':
        for k in keys():
            print(public_hex(k))
    elif a[0] == '--new-key' and len(a) == 2:
        p = pathlib.Path(a[1])
        if p.exists():
            sys.exit(f'{p}: there already; not replaced')
        k = Ed25519PrivateKey.generate()
        pem = k.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
        fd = os.open(p, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, 'wb') as f:
            f.write(pem)
        print(f'{p}: a new private key (keep it and a backup outside every repo); its public key:')
        print(public_hex(k))
    elif a[0] == '--check' and len(a) >= 3:
        by = check(a[1], a[2:])
        print(f'{a[1]}: signed by {by}' if by else f'{a[1]}: not signed by any of those keys')
        sys.exit(0 if by else 1)
    else:
        ks = keys()
        for f in a:
            print(sign_file(f, ks))


if __name__ == '__main__':
    main()
