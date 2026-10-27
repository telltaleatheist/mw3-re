"""zFMV's movie scripts (gamez/zfmv, docs/fmv.md) and zRender's blur against
the original x86 in Unicorn, the x87 at 24 bits as in play.

    cmake --build build && .venv/bin/python tests/differential/test_fmv.py [runs, 400]
    .venv/bin/python tests/differential/test_fmv.py --break   # the test catches errors

The script: fmv.zrd's shipped blocks (INTRO, SPLASH, UNLOAD, ATTRACT,
campaign, c1m1, the DEMO ones, every mission's) and random ones with every
verb (bad entries, unknown verbs and short lists among them) are loaded
(0x00598d80) on both sides from the same binary tree, then played (0x005995b0)
and polled (0x00599620) at random times until done, in Direct3D (mode 1),
software (0) and mode 2; some runs are skipped by a key, rewound (0x00599750)
and played again, or deleted mid-way (0x00598d00). Every call out is stubbed
in the emulator and recorded on both sides (calltrace.py): the image, fade,
wait, blur, sound and movie actions' calls in order, with their arguments
(rects as their words, the tint's alpha as its double). Compared too: each
call's result and the current action after it. PLAYAVI finds no file (the
port's stub until item 11), so both sides end it at once.

The pause screen's blur (0x00599f70 run by 0x00599a70) the same way, and the
three blur passes (0x0055a540: 0x00559ec0, 0x0055a1b0, 0x0055a3b0) bit for bit
on random screens, scratch buffers, pitches, rects and both 16-bit formats.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402
from original import Original  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'analysis'))
import zrd  # noqa: E402

CTOR, DTOR, CLEAR, LOAD_SCRIPT, PLAY, PLAYING, STOP = 0x598cc0, 0x598d00, 0x598d30, 0x598d80, 0x5995b0, 0x599620, 0x599750
BLUR_CTOR, BLUR_RUN, BLUR = 0x599f70, 0x599a70, 0x55a540
READ_NODE, ZRD_LOAD, FREE_NODE, DISC = 0x557180, 0x556740, 0x556820, 0x575d40
SCRATCH, SCREEN, WIDTH, HEIGHT, PITCH_PX = 0x72c1e0, 0x72c1e4, 0x72c1e8, 0x72c1ec, 0x72c1f4
MASKS = 0x80272c
IAT = {'malloc': 0x5bb664, 'calloc': 0x5bb60c, 'free': 0x5bb65c, 'strdup': 0x5bb670, 'strncpy': 0x5bb648,
       'readfile': 0x5bb0c4, 'sprintf': 0x5bb658, 'stat': 0x5bb588}
# Video for Windows and the critical sections, by IAT slot: (name, stack bytes popped).
VFW = {0x5bb01c: ('AVIFileInit', 0), 0x5bb018: ('AVIFileExit', 0), 0x5bb030: ('AVIStreamOpenFromFileA', 24),
       0x5bb038: ('AVIStreamReadFormat', 16), 0x5bb02c: ('AVIStreamLength', 4), 0x5bb028: ('AVIStreamInfoA', 12),
       0x5bb034: ('AVIStreamRead', 28), 0x5bb020: ('AVIStreamFindSample', 12), 0x5bb024: ('AVIStreamRelease', 4),
       0x5bb684: ('ICLocate', 20), 0x5bb680: ('ICSendMessage', 16), 0x5bb688: ('ICDecompress', 0),
       0x5bb67c: ('ICClose', 4)}
SECTIONS = {0x5bb154: 4, 0x5bb158: 4, 0x5bb160: 4, 0x5bb168: 4}
VIDS, AUDS = 0x73646976, 0x73647561
FILE = 0x1234
FORMATS = ((5, 6, 5), (5, 5, 5))
SHIPPED = ['INTRO', 'SPLASH', 'UNLOAD', 'DEMO_INTRO', 'DEMO_EXIT', 'DEMO_SPLASH', 'ATTRACT', 'campaign', 'c1m1',
           'c2m1', 'c3m3', 'c4win', 'c4m7', 'NOT_A_BLOCK']
SIMPLE = {
    0x576d80: '', 0x576d90: '', 0x576da0: '', 0x576db0: '', 0x576ca0: '', 0x576cd0: '', 0x576cb0: '',
    0x576cc0: '', 0x576de0: '', 0x576e70: '', 0x576d00: '', 0x576d60: '', 0x576ea0: 'c,d,s0,s1',
    0x579500: 'c', 0x579af0: '', 0x579b10: '', 0x558290: 'c,d,s0,s1', 0x558610: '', 0x5179d0: 'zc',
    0x517b10: 'c', 0x516f50: 'zc', 0x56f520: 'zc', 0x56eb10: '', 0x56f0a0: 'c', 0x56f6f0: 'zc',
    0x56e3d0: 'c,s0', 0x56e9f0: 'c', 0x51a300: '', 0x51a9b0: 'c',
}

probe = load_probe()
probe.probe_fmv_load.argtypes = [ctypes.c_char_p, ctypes.c_int32, ctypes.c_char_p]
probe.probe_fmv_load.restype = ctypes.c_int32
probe.probe_fmv_play.argtypes = [ctypes.c_double]
probe.probe_fmv_play.restype = ctypes.c_int32
probe.probe_fmv_playing.argtypes = [ctypes.c_double]
probe.probe_fmv_playing.restype = ctypes.c_int32
probe.probe_fmv_current.restype = ctypes.c_int32
probe.probe_fmv_count.restype = ctypes.c_int32
probe.probe_fmv_actions.restype = ctypes.c_int32
probe.probe_fmv_movie.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int32, ctypes.c_char_p, ctypes.c_char_p,
                                  ctypes.c_int32]
probe.probe_blur.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p,
                             ctypes.c_void_p, ctypes.c_int32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
probe.probe_blur.restype = None


def encode(n):
    """A node in the binary zrd form (docs/zreader.md)."""
    if isinstance(n, bool):
        n = int(n)
    if isinstance(n, int):
        return struct.pack('<ii', 1, n)
    if isinstance(n, float):
        return struct.pack('<if', 2, n)
    if isinstance(n, str):
        b = n.encode('latin1')
        return struct.pack('<iI', 3, len(b)) + b
    return struct.pack('<iI', 4, len(n) + 1) + b''.join(encode(x) for x in n)


def shipped_fmv():
    data = (ROOT / 'game' / 'mw3-1.2' / 'zbd' / 'reader.zbd').read_bytes()
    for name, start, length in zrd.entries(data):
        if name == 'fmv.zrd':
            return data[start:start + length]
    raise SystemExit('fmv.zrd not in reader.zbd')


def random_entry(r):
    names = ['mech3splash', 'demologo1', 'demoexit', 'missing', 'x' * 60, 'intro.avi', 'demo_intro']
    verb = r.choice(['SHOWIMAGE', 'BLITIMAGE', 'LOADIMAGE', 'WAIT', 'FADEIN', 'FADEOUT', 'PLAYAVI', 'BLUR',
                     'BLURH', 'BLURV', 'PLAYSOUND', 'UNKNOWN', 'showimage'])
    n = r.choice(names)
    num = lambda: r.choice([0, 1, 110, 260, -5, r.randrange(-50, 700)])  # noqa: E731
    sec = lambda: r.choice([0.0, 0.25, 0.5, 1.0, 2.0, 4.0, r.uniform(0.01, 3.0), -1.0])  # noqa: E731
    if verb in ('SHOWIMAGE', 'LOADIMAGE'):
        return r.choice(([verb, n, num(), num()], [verb, n], [verb, n], [verb, n, num()]))
    if verb == 'BLITIMAGE':
        return [verb, n, num(), num()]
    if verb == 'WAIT':
        return [verb, sec()]
    if verb in ('FADEIN', 'FADEOUT'):
        rgb = [r.choice([0, 255, r.randrange(256), 300]) for _ in range(3)]
        return [verb, rgb, sec(), r.choice([1.0, 0.5, 0.0, r.uniform(0, 1)])]
    if verb == 'PLAYAVI':
        return [verb, n, r.choice([0, 1])] if r.random() < 0.4 else [verb, n]
    if verb in ('BLUR', 'BLURH', 'BLURV'):
        return [verb, r.choice([1, 2, 3, 10])]
    return [verb, n]


MOVIE_CALLS = {}
MOVIE_BLOCKS = ['INTRO', 'ATTRACT', 'campaign', 'c1m1', 'c4win']


def movie_tree(r):
    block = []
    for _ in range(r.randrange(1, 5)):
        e = random_entry(r)
        if r.random() < 0.6:
            e = ['PLAYAVI', 'm.avi', r.choice([0, 1, 1])] if r.random() < 0.5 else ['PLAYAVI', 'm.avi']
        block.append(e)
    return encode([['FMV_PATH', ['.\\video'], 'IMAGE_PATH', ['..\\data\\common\\images'], 'BLOCK', block]])


def random_tree(r):
    block = [random_entry(r) for _ in range(r.randrange(0, 9))]
    if r.random() < 0.08 and block:
        block.insert(r.randrange(len(block)), r.choice(['WAIT', 3, 1.5]))  # not a list: the read stops
    return encode([['FMV_PATH', ['.\\video'], 'IMAGE_PATH', ['..\\data\\common\\images'], 'BLOCK', block]])


def rect(e, p):
    if not p:
        return [0, 0, 0, 0, 0]
    return [1] + [e.r32(p + 4 * i) for i in range(4)]


class Side:
    """The original: the emulator with every call out of zFMV stubbed."""

    def __init__(self, t, fmt):
        self.t = t
        e = self.e = Emu(t)
        e.heap_stubs()
        self.file, self.pos = b'', 0
        a = e.cpu.arg

        def imp(name, fn, pop=0, key=None):
            e.import_stub(IAT[name], key or name, '', pop, lambda em, k, aa, p: em._ret(fn() or 0, p))
            if key is None:
                t.silent.add(name)
        imp('malloc', lambda: e.alloc(max(a(0), 4)))
        imp('calloc', lambda: e.alloc(max(a(0) * a(1), 4)))
        imp('free', lambda: 0)
        imp('strdup', lambda: e.world_string(e.cstr(a(0))))
        imp('strncpy', self.strncpy)
        imp('readfile', self.read_file, 20)
        imp('sprintf', self.sprintf)
        e.import_stub(IAT['stat'], '_stat', 'zs0', 0)
        for addr, spec in SIMPLE.items():
            e.stub(addr, spec=spec)
        e.stub(0x55b1f0, fn=lambda em, k, aa, p: em._ret(t.handle(k, em.args('c,d,s0,s1') + rect(em, em.args('s2')[0])), p))
        e.stub(0x558540, fn=lambda em, k, aa, p: em._ret(t.handle(
            k, [em.args('c')[0] & 0xffff] + rect(em, em.args('d')[0]) + em.args('s0,s1')), p))
        e.stub(BLUR, fn=lambda em, k, aa, p: em._ret(t.handle(k, rect(em, em.args('c')[0]) + em.args('d')), p))
        e.stub(0x558080, fn=self.render_target)
        e.stub(ZRD_LOAD, key='zrdr_load', fn=lambda em, k, aa, p: em._ret(self.tree, p))
        e.stub(DISC, key='disc', fn=lambda em, k, aa, p: em._ret(0, p))
        t.silent |= {'zrdr_load', 'disc'}
        for slot, key, pop, two in ((0x72f358, '[0x0072f358]', 0, False), (0x72f35c, '[0x0072f35c]', 0, False),
                                    (0x72f360, '[0x0072f360]', 8, True)):
            if two:
                fn = (lambda kk: lambda em, k, aa, p: em._ret(t.handle(
                    k, em.args('c,d') + rect(em, em.args('s0')[0]) + rect(em, em.args('s1')[0])), p))(key)
            else:
                fn = lambda em, k, aa, p: em._ret(t.handle(k, rect(em, em.args('c')[0]) + rect(em, em.args('d')[0])), p)
            e.w32(slot, e.new_stub(key, '', pop, fn))
        e.w32(0x72f3ec, e.new_stub('[0x0072f3ec]', '', 0))
        self.movie = None
        self.streams = set()
        self.movie_stubs()
        self.mode(1)
        r_bits, g_bits, b_bits = fmt
        for addr, v in ((0x802738, r_bits + g_bits + b_bits - 8), (0x80273c, g_bits + b_bits - 8),
                        (0x802740, 8 - b_bits), (0x802744, ((1 << r_bits) - 1) << (8 - r_bits)),
                        (0x802748, ((1 << g_bits) - 1) << (8 - g_bits)),
                        (0x80274c, ((1 << b_bits) - 1) << (8 - b_bits))):
            e.w32(addr, v)
        self.tree = 0
        self.obj = 0
        e.cpu.fpu_control(0x007f)  # the game's 24-bit precision in play (kept between calls)

    def mode(self, m):
        self.e.w32(0x72f348, m)

    def movie_stubs(self):
        """The movie player's calls out (fmv_stream.cpp): Video for Windows on
        the run's movie, zSound's streaming buffer, the stream's allocation."""
        e, t, a = self.e, self.t, self.e.cpu.arg

        def vfw(name):
            def f(em, k, aa, pop):
                m = self.movie
                if name == 'AVIStreamOpenFromFileA':
                    hr = t.handle(k, [em.cstr(a(1)), a(2)])
                    if hr == 0:
                        em.w32(a(0), 0x100 if a(2) == VIDS else 0x200)
                elif name == 'AVIStreamReadFormat':
                    h, buf, psize = a(0), a(2), a(3)
                    hr = t.handle(k, [h, int(buf != 0), em.r32(psize) if buf else 0])  # the size asked for is unset
                    if hr == 0:
                        f = m['vfmt'] if h == 0x100 else m['afmt']
                        if not buf:
                            em.w32(psize, len(f))
                        else:
                            em.cpu.write(buf, f[:em.r32(psize)])
                elif name == 'AVIStreamInfoA':
                    hr = t.handle(k, [a(0)])
                    if hr == 0:
                        em.cpu.write(a(1), m['vinfo'] if a(0) == 0x100 else m['ainfo'])
                elif name == 'AVIStreamRead':
                    hr = t.handle(k, [a(0), a(1), a(2), a(4)])
                elif name in ('AVIStreamLength', 'AVIStreamRelease', 'ICClose'):
                    hr = t.handle(k, [a(0)])
                elif name == 'AVIStreamFindSample':
                    hr = t.handle(k, [a(0), a(1), a(2)])
                elif name == 'ICLocate':
                    out = a(3)
                    hr = t.handle(k, [a(0), a(1), a(4), em.r32(out), em.r32(out + 0xc) >> 16, em.r32(out + 0x10),
                                      em.r32(out + 0x14)])
                elif name == 'ICSendMessage':
                    hr = t.handle(k, [a(0), a(1), int(a(2) != 0), int(a(3) != 0)])
                elif name == 'ICDecompress':
                    hr = t.handle(k, [a(0), a(1)])
                else:
                    hr = t.handle(k, [])
                em._ret(hr, pop)
            return f
        for iat, (name, pop) in VFW.items():
            e.import_stub(iat, name, '', pop, vfw(name))
        for iat, pop in SECTIONS.items():
            e.import_stub(iat, f'section{iat:x}', '', pop)
            t.silent.add(f'section{iat:x}')
        e.stub(0x576c70, spec='')
        e.stub(0x5758c0, fn=lambda em, k, aa, p: em._ret(t.handle(k, em.args('s0')), p))
        self.lock_buf = e.alloc(0x10000)

        def lock(em, k, aa, p):
            c, d, nbytes, p1, p2, n1, n2 = em.args('c,d,s0,s1,s2,s3,s4')
            ok = t.handle(k, [c, d, nbytes])
            em.w32(p1, self.lock_buf), em.w32(p2, self.lock_buf + 0x8000)
            em.w32(n1, t.handle('@lock_n1', [])), em.w32(n2, t.handle('@lock_n2', []))
            em._ret(ok, p)
        e.stub(0x575550, fn=lock)
        e.stub(0x575600, fn=lambda em, k, aa, p: em._ret(t.handle(k, em.args('c,s1,s2')), p))
        e.stub(0x575690, spec='c')
        e.stub(0x575980, spec='c')

        def new(em, k, aa, p):
            n = em.args('s0')[0]
            ptr = em.alloc(max(n, 4))
            if n == 0x1e4:
                self.streams.add(ptr)
            em._ret(ptr, 0)
        e.stub(0x5abd26, 'new', 's0', 0, new)
        # The stretch names a stream's picture 1 (the probe's stream_image).
        e.w32(0x72f360, e.new_stub('[0x0072f360]', '', 8, lambda em, k, aa, p: em._ret(t.handle(
            k, [1 if em.args('c')[0] in self.streams else em.args('c')[0]] + em.args('d') +
            rect(em, em.args('s0')[0]) + rect(em, em.args('s1')[0])), p)))

    def strncpy(self):
        e, a = self.e, self.e.cpu.arg
        dst, src, n = a(0), a(1), a(2)
        s = e.cstr(src).encode('latin1')[:n]
        e.cpu.write(dst, s + b'\0' * (n - len(s)))
        return dst

    def sprintf(self):
        e, a = self.e, self.e.cpu.arg
        fmt, out, k = e.cstr(a(1)), [], 2
        i = 0
        while i < len(fmt):
            if fmt[i] == '%' and i + 1 < len(fmt):
                c = fmt[i + 1]
                out.append(e.cstr(a(k)) if c == 's' else str(a(k)))
                k += 1
                i += 2
            else:
                out.append(fmt[i])
                i += 1
        s = ''.join(out).encode('latin1')
        e.cpu.write(a(0), s + b'\0')
        return len(s)

    def read_file(self):
        a = self.e.cpu.arg
        buf, n, out = a(1), a(2), a(3)
        chunk = self.file[self.pos:self.pos + n]
        self.pos += len(chunk)
        self.e.cpu.write(buf, chunk)
        self.e.w32(out, len(chunk))
        return 1

    def render_target(self, em, k, aa, p):
        c, d = em.args('c,d')
        w = self.t.handle(k, [])
        h = self.t.handle(k + '.h', [])
        if c:
            em.w32(c, w)
        if d:
            em.w32(d, h)
        em._ret(0, p)

    def build(self, data):
        self.file, self.pos = data, 0
        root = self.e.alloc(8)
        self.e.call(READ_NODE, ecx=FILE, edx=root)
        return root

    def call(self, addr, ecx=0, edx=0, stack=()):
        return self.e.call(addr, ecx=ecx, edx=edx, stack=stack)

    def new_script(self):
        self.obj = self.e.alloc(0x20)
        self.call(CTOR, ecx=self.obj, stack=(0, 0, 0))

    def load(self, data, name):
        self.tree = self.build(data)
        return self.call(LOAD_SCRIPT, ecx=self.obj,
                         stack=(self.e.world_string('fmv.zrd'), self.e.world_string(name)))

    def current(self):
        cur = self.e.r32(self.obj + 0x18)
        if not cur:
            return -1
        a, i = self.e.r32(self.obj + 0x10), 0
        while a:
            if a == cur:
                return i
            a, i = self.e.r32(a + 4), i + 1
        return -2

    def actions(self):
        """Each action's vtable and the words the probe gives for it."""
        e, out = self.e, []
        a = e.r32(self.obj + 0x10)
        while a:
            vt = e.r32(a)
            b = lambda off: e.r8(a + off)  # noqa: E731
            if vt == 0x5cf850:
                out += [vt, b(0x10), b(0x11)] + [e.r32(a + 0x14 + 4 * i) for i in range(4)]
            elif vt == 0x5cf820:
                out += [vt, e.r32(a + 8)]
            elif vt == 0x5cf868:
                out += [vt, e.r32(a + 8), e.r32(a + 0xc) & 0xffff, e.r32(a + 0x10), e.r32(a + 0x24)]
            elif vt == 0x5cf880:
                out += [vt, b(0xc), int(e.r32(a + 8) != 0)]
            elif vt in (0x5cf898, 0x5cf808, 0x5cf7f0):
                out += [vt, e.r32(a + 8), e.r32(a + 0xc)]
            elif vt == 0x5cf7d8:
                out += [vt]
            else:
                out += [0]
            a = e.r32(a + 4)
        return out

    def count(self):
        a, n = self.e.r32(self.obj + 0x10), 0
        while a:
            a, n = self.e.r32(a + 4), n + 1
        return n


def dwords(t):
    lo, hi = struct.unpack('<II', struct.pack('<d', t))
    return lo, hi


def random_movie(r):
    """A movie for the AVI calls: its two AVISTREAMINFOA, the BITMAPINFOHEADER,
    the WAVEFORMAT and the key frames."""
    w, h = r.choice(((640, 480), (64, 48), (32, 16))), None
    w, h = w
    vlen = r.choice((1, 2, 5, 30, r.randrange(3, 90)))
    rate, scale = r.choice(((15, 1), (15, 1), (30, 2), (10, 1)))
    block = r.choice((1, 1, 2, 4))
    sr = r.choice((22050, 22050, 11025))
    half = r.choice((block * sr, block * sr + 4, block * 2205, block * 735, 4000))
    half -= half % block
    alen = r.choice((vlen * sr // (rate // scale), r.randrange(100, 400000)))
    every = r.choice((1, 15, 4, 1000))
    keys = [k for k in range(vlen) if k % every == 0] or [0]
    if r.random() < 0.2:
        keys = sorted(set(keys + [r.randrange(vlen) for _ in range(3)]))
    vsugg = r.choice((0, 3 * w * h, 1234))

    def info(typ, sc, rt, length, sugg, ss):
        b = struct.pack('<IIIIHHIIIIIIII4iII', typ, 0x30355649 if typ == VIDS else 0, 0, 0, 0, 0, sc, rt, 0, length,
                        0, sugg, 10000, ss, 0, 0, w, h, 0, 0)
        return b + bytes(0x8c - len(b))
    vfmt = struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0x30355649, 3 * w * h, 0, 0, 0, 0)
    if r.random() < 0.3:  # a format past 0x6c bytes (a header with extra data)
        extra = r.choice((0x50, 0x400))
        vfmt = struct.pack('<I', 40 + extra) + vfmt[4:] + bytes(r.randrange(256) for _ in range(extra))
    afmt = struct.pack('<HHIIHH', 1, 2 if block == 4 else 1, sr, sr * block, block, 16 if block >= 2 else 8)
    return {'vinfo': info(VIDS, scale, rate, vlen, vsugg, 0), 'ainfo': info(AUDS, 1, sr, alen, half, block),
            'vfmt': vfmt, 'afmt': afmt, 'keys': keys, 'vlen': vlen, 'alen': alen, 'half': half, 'block': block,
            'has_audio': r.random() < 0.9}


def answers(seed, mode, movie=None):
    r = rnd(seed, 'answers')
    skip = r.random() < 0.25
    skip_rate = r.choice([0.002, 0.02, 0.1])
    lock = {'bytes': 0}
    step = r.choice((0.05, 0.1, 0.3, 0.6, 1.1)) * (movie['half'] if movie else 1000)

    def answer(key):
        def f(n, a):
            rr = rnd(seed, f'{key}#{n}')
            m = movie
            if key == 'AVIStreamOpenFromFileA':
                if not m or (a[1] == AUDS and not m['has_audio']) or rr.random() < 0.02:
                    return 0x80044103  # AVIERR_FILEOPEN
                return 0
            if key in ('AVIStreamReadFormat', 'AVIStreamInfoA', 'AVIStreamRead'):
                return 0x80044066 if rr.random() < 0.01 else 0
            if key == 'AVIStreamLength':
                return m['vlen'] if a[0] == 0x100 else m['alen']
            if key == 'AVIStreamFindSample':
                pos, flags = a[1] - (1 << 32) if a[1] >= 1 << 31 else a[1], a[2]
                if flags & 4:
                    best = [k for k in m['keys'] if k <= pos]
                    return best[-1] if best else 0xffffffff
                nxt = [k for k in m['keys'] if k >= pos]
                return nxt[0] if nxt else 0xffffffff
            if key == 'ICLocate':
                return 0x777
            if key in ('ICSendMessage', 'AVIFileInit', 'AVIFileExit', 'AVIStreamRelease', 'ICClose'):
                return 0
            if key == 'ICDecompress':
                return 0xffffffff if rr.random() < 0.01 else 0
            if key == '0x00576c70':
                return 24 if rr.random() < 0.1 else 16
            if key == '0x005758c0':
                return 0 if rr.random() < 0.05 else 0x4400
            if key == '0x00575550':
                lock['bytes'] = a[2]
                return 0 if rr.random() < 0.03 else 1
            if key == '@lock_n1':
                b = lock['bytes']
                lock['n1'] = b if rr.random() < 0.9 else b // 2 - (b // 2) % max(1, m['block'])
                return lock['n1']
            if key == '@lock_n2':
                return lock['bytes'] - lock.get('n1', 0)
            if key == '0x00575690':
                if rr.random() < 0.05:
                    return 0
                if m and rr.random() < 0.05:
                    return rr.choice((m['half'], m['half'] - 1, m['half'] + 1, 2 * m['half'] - 1))
                size = 2 * m['half'] if m else 1
                return int(n * step + rr.randrange(0, 64)) % max(size, 1)
            if key == '0x00575980':
                return 0
            if key == '0x0051a9b0':
                return rr.choice((1, 2, 4)) if skip and rr.random() < skip_rate else 0
            if key in ('0x005179d0', '0x00579500', '0x0056f6f0'):
                return 0 if rr.random() < 0.12 else 0x10000 + 0x100 * n + rr.randrange(0x100)
            if key == '0x0056e3d0':
                return 0x20000 + n
            if key == '0x0056eb10':
                return 0x30000 + n
            if key == '0x00558080':
                return rr.choice((640, 800, 1024, 1280))
            if key == '0x00558080.h':
                return rr.choice((480, 600, 768, 960))
            if key == '_stat':
                return 0 if movie else 0xffffffff
            return rr.randrange(1, 0x10000)
        return f
    return answer


class Script(dict):
    def __init__(self, f):
        super().__init__()
        self.f = f

    def get(self, key, default=None):
        return self.f(key)

    def items(self):
        return []

    def __contains__(self, key):
        return True

    def __getitem__(self, key):
        return self.f(key)


def nan_alike(log):
    """The tint's alpha as a double: a NaN (a fade of 0 seconds at its start,
    0 / 0) is the x87's negative default NaN in the original and a positive
    one on ARM; the sign is not compared."""
    out = []
    for key, args in log:
        if key == '0x00558540' and (args[7] & 0x7ff00000) == 0x7ff00000 and ((args[7] & 0xfffff) or args[6]):
            args = args[:7] + (args[7] & 0x7fffffff,)
        out.append((key, args))
    return out


def run(seed, data, name, verbose=False, movie=None):
    r = rnd(seed, 'run')
    fmt = r.choice(FORMATS)
    mode = r.choice((1, 1, 1, 0, 2))
    t = Trace()
    t.script = Script(answers(seed, mode, movie))
    o = Side(t, fmt)
    o.mode(mode)
    o.movie = movie
    snap = o.e.snapshot()
    rep = Replay(t, o.e, snap)
    probe.probe_app_set_callback(rep.callback())
    probe.probe_fmv_reset(mode, *fmt)
    if movie:
        probe.probe_fmv_movie(movie['vinfo'], movie['vfmt'], len(movie['vfmt']), movie['ainfo'], movie['afmt'],
                              len(movie['afmt']))
    label = f'seed {seed} block {name} mode {mode} format {fmt}' + (' with a movie' if movie else '')
    results = []

    def both(what, want, got):
        results.append((what, want, got))
        return want == got

    o.new_script()
    n_e = o.load(data, name)
    n_p = probe.probe_fmv_load(data, len(data), name.encode())
    ok = both('load', n_e & 0xffffffff, n_p & 0xffffffff) and both('count', o.count(), probe.probe_fmv_count())
    buf = (ctypes.c_uint32 * 4096)()
    k = probe.probe_fmv_actions(buf, 4096)
    ok = ok and both('actions', o.actions(), list(buf[:k]))
    now = r.choice((0.0, 3.5, 1000.0, 86400.25, r.uniform(0, 5e4)))
    polls = 0
    for attempt in range(r.choice((1, 1, 2))):
        if not ok:
            break
        lo, hi = dwords(now)
        ok = both(f'play {attempt}', o.call(PLAY, ecx=o.obj, stack=(lo, hi)) & 0xff, probe.probe_fmv_play(now) & 0xff)
        while ok and polls < 600:
            now += r.choice((0.0, 0.016, 0.033, 0.1, 0.25, 0.7, 1.5))
            lo, hi = dwords(now)
            pe = o.call(PLAYING, ecx=o.obj, stack=(lo, hi)) & 0xff
            pp = probe.probe_fmv_playing(now) & 0xff
            polls += 1
            ok = both(f'poll {polls}', pe, pp) and both(f'current {polls}', o.current(), probe.probe_fmv_current())
            if not pe:
                break
            if r.random() < 0.01:  # left mid-way, as a state's leave does
                break
            if r.random() < 0.004:
                o.mode(mode := r.choice((0, 1, 2)))
                probe.probe_fmv_mode(mode)
        if ok:
            all_ = r.random() < 0.3
            o.call(STOP, ecx=o.obj, stack=(int(all_),))
            probe.probe_fmv_stop(int(all_))
            ok = both('after stop', o.current(), probe.probe_fmv_current()) and both(
                'count after stop', o.count(), probe.probe_fmv_count())
    if ok:
        o.call(DTOR, ecx=o.obj)
        probe.probe_fmv_delete()
    ok = ok and compare(label, nan_alike(t.log), nan_alike(rep.log))
    if movie:
        for key, args in t.log:
            k = key
            if key == 'AVIStreamFindSample':
                k = f'{key}({args[2]:#x})'
            elif key == 'ICDecompress':
                k = f'{key}({args[1]:#x})'
            MOVIE_CALLS[k] = MOVIE_CALLS.get(k, 0) + 1
    if not ok:
        bad = [x for x in results if x[1] != x[2]]
        if bad:
            print(f'{label}: {bad[0][0]}: original {bad[0][1]} rebuild {bad[0][2]}')
    if verbose:
        print(label, polls, 'polls', len(t.log), 'calls')
    return ok, len(t.log), polls


def run_pause_blur(seed):
    r = rnd(seed, 'pause')
    mode = r.choice((0, 1, 2))
    t = Trace()
    t.script = Script(answers(seed, mode))
    o = Side(t, FORMATS[0])
    o.mode(mode)
    rep = Replay(t, o.e, o.e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    probe.probe_fmv_reset(mode, *FORMATS[0])
    steps, passes = r.choice(((4, 1), (1, 1), (2, 3), (1, 0)))
    b = o.e.alloc(0x30)
    o.call(BLUR_CTOR, ecx=b, stack=(steps, passes))
    o.call(BLUR_RUN, ecx=b)
    probe.probe_fmv_blur_run(steps, passes)
    return compare(f'pause blur seed {seed} mode {mode} ({steps}, {passes})', t.log, rep.log), len(t.log)


# --- the blur passes ---------------------------------------------------------
def blur_case(r):
    """A screen, scratch and rect. The screen has `pad` random words either
    side: a narrow rect's walk reads above and below the target (the screen's
    other rows in the game). A pitch wider than the width only over the whole
    target, at most 2 * width - 2 (the rows then step back by the difference;
    past that the original walks off the scratch)."""
    w, h = r.randint(8, 90), r.randint(4, 60)
    kind = r.choice((1, 2, 3, 0))
    rc, pitch = None, w
    if r.random() < 0.5:
        x0, y0 = r.randint(-3, w // 2), r.randint(-3, h // 2)
        rc = (x0, y0, r.randint(max(x0, 0), w + 4), r.randint(max(y0, 0), h + 4))
    elif r.random() < 0.4:
        pitch = w + r.choice((3, 16))
        if pitch > 2 * w - 2:
            pitch = w
    fmt = r.choice(FORMATS)
    m = [((1 << b) - 1) << s for b, s in zip(fmt, (fmt[1] + fmt[2], fmt[2], 0))]
    pad = 2 * (w + pitch) + 16
    screen = [r.randrange(0x10000) for _ in range(pitch * h + 2 * pad)]
    scratch = [r.randrange(0x10000) for _ in range(w * h)]
    return w, h, pitch, kind, rc, m, screen, scratch, pad


def blur_original(o, c):
    w, h, pitch, kind, rc, m, screen, scratch, pad = c
    n = len(screen)
    scr = o.alloc(2 * n) + 2 * pad
    scx = o.alloc(2 * w * h)
    o.write(scr - 2 * pad, struct.pack(f'<{n}H', *screen))
    o.write(scx, struct.pack(f'<{w * h}H', *scratch))
    for addr, v in ((SCRATCH, scx), (SCREEN, scr), (WIDTH, w), (HEIGHT, h), (PITCH_PX, pitch)):
        o.u32(addr, v)
    for i, v in enumerate(m):
        o.u32(MASKS + 4 * i, v)
    cp = 0
    if rc:
        cp = o.alloc(16)
        o.write(cp, struct.pack('<4i', *rc))
    o.call(BLUR, ecx=cp, edx=kind)
    return o.read(scr - 2 * pad, 2 * n), o.read(scx, 2 * w * h)


def blur_port(c):
    w, h, pitch, kind, rc, m, screen, scratch, pad = c
    scr = (ctypes.c_uint16 * len(screen))(*screen)
    scx = (ctypes.c_uint16 * (w * h))(*scratch)
    cp = (ctypes.c_int32 * 4)(*rc) if rc else None
    probe.probe_blur(ctypes.addressof(scr) + 2 * pad, w, h, pitch, scx, cp, kind, *m)
    return bytes(scr), bytes(scx)


def blur_tests(n):
    o = Original()
    r = random.Random(0x55a540)
    done = changed = 0
    for k in range(n):
        c = blur_case(r)
        heap = o.heap_next
        want = blur_original(o, c)
        o.heap_next = heap
        got = blur_port(c)
        if want != got:
            w, h, pitch, kind, rc = c[:5]
            which = 'screen' if want[0] != got[0] else 'scratch'
            print(f'blur case {k}: {which} differs: {w}x{h} pitch {pitch} kind {kind} rect {rc} masks {c[5]}')
            return False, done, changed
        done += 1
        changed += want[0] != struct.pack(f'<{len(c[6])}H', *c[6])
    return True, done, changed


def main():
    probe.probe_fmv_reset.argtypes = [ctypes.c_int32] * 4
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 400
    shipped = shipped_fmv()
    cases = [(seed, shipped, SHIPPED[seed % len(SHIPPED)]) for seed in range(2 * len(SHIPPED))]
    cases += [(seed, random_tree(rnd(seed, 'tree')), 'BLOCK') for seed in range(1000, 1000 + runs)]
    # PLAYAVI found: the movie player on random movies, the shipped movie blocks and random ones.
    movies = [(seed, shipped, MOVIE_BLOCKS[seed % len(MOVIE_BLOCKS)], random_movie(rnd(seed, 'movie')))
              for seed in range(5000, 5000 + max(10, runs // 4))]
    movies += [(seed, movie_tree(rnd(seed, 'tree')), 'BLOCK', random_movie(rnd(seed, 'movie')))
               for seed in range(7000, 7000 + max(10, runs // 4))]
    broken = '--break' in sys.argv
    caught = 0
    calls = polls = 0
    for seed, data, name in cases:
        ok, n, p = run(seed, data, name, verbose='-v' in sys.argv)
        if not ok:
            if not broken:
                sys.stdout.flush(); os._exit(1)
            caught += 1
        calls += n
        polls += p
    for seed, data, name, movie in movies:
        ok, n, p = run(seed, data, name, verbose='-v' in sys.argv, movie=movie)
        if not ok:
            if not broken:
                sys.stdout.flush(); os._exit(1)
            caught += 1
        calls += n
        polls += p
    pause = 0
    for seed in range(40):
        ok, n = run_pause_blur(seed)
        if not ok:
            if not broken:
                sys.stdout.flush(); os._exit(1)
            caught += 1
        pause += n
    ok, done, changed = blur_tests(int(os.environ.get('BLUR_CASES', '300')))
    if not ok:
        if not broken:
            sys.stdout.flush(); os._exit(1)
        caught += 1
    if broken:
        print(f'{caught} runs caught the break')
        sys.stdout.flush(); os._exit(0 if caught else 1)
    want = ('ICDecompress(0x0)', 'ICDecompress(0x8000000)', 'ICDecompress(0x80000000)', 'ICDecompress(0x88000000)',
            'AVIStreamFindSample(0x11)', 'AVIStreamFindSample(0x14)', '0x00575550', '0x00575690', '0x005758c0',
            '0x0056e3d0', 'AVIStreamRelease', 'ICClose')
    print('movie calls: ' + ', '.join(f'{k} {MOVIE_CALLS.get(k, 0)}' for k in want))
    print(f'fmv: {len(cases) + len(movies)} scripts ({2 * len(SHIPPED)} shipped, {len(movies)} with movies), '
          f'{polls} polls, {calls} calls identical; '
          f'pause blur 40 runs, {pause} calls; blur passes {done} cases bit for bit ({changed} changed the screen)')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
