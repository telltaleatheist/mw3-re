"""zTwoD's message box against the original x86 (docs/twod.md, The message box).

    cmake --build build && .venv/bin/python tests/differential/test_twod_msgbox.py [runs]

The original builds dlgMessage.zrd's MESSAGEBOX (0x005a6e60: its backdrop
grab, the eight MB_* buttons, TITLE and MESSAGE), runs its modal loop
(0x005a7280) with a style from MB_OK to MB_RETRYCANCEL, in software or
Direct3D mode, with the mouse wandering, pressing, holding and releasing
until it releases over one of the shown buttons, and is destroyed
(0x005a7150). Even runs use the shipped box (OK, Cancel and the user button
only, so the styles those answer); odd runs a synthetic one with all eight
buttons, every style. Every call out is stubbed (twod_world.py, plus the loop's
zVideo and zRender calls); the port (tests/probe_twod.cpp) runs the same
with the original's answers. Compared: the call traces and the answer.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, Trace, compare, load_probe  # noqa: E402
from original import ROOT  # noqa: E402
from twod_world import READER, TwodWorld, entries, find, parse_node, write_node  # noqa: E402
from test_twod import canonical, fbits  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_twod_msgbox.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32,
                                    ctypes.c_void_p, ctypes.c_int]
CTOR, RUN, DTOR, ZRDR_LOAD = 0x5a6e60, 0x5a7280, 0x5a7150, 0x556740
STYLE_BUTTONS = {0: ['MB_OK'], 1: ['MB_OK', 'MB_CANCEL'], 2: ['MB_ABORT', 'MB_RETRY', 'MB_IGNORE'],
                 3: ['MB_CANCEL', 'MB_YES', 'MB_NO'], 4: ['MB_YES', 'MB_NO'], 5: ['MB_RETRY', 'MB_CANCEL']}


def archive(files):
    """A .zbd archive (zutl_zar.h) of (name, bytes)."""
    blob, table = b'', b''
    for name, data in files:
        table += struct.pack('<II', len(blob), len(data)) + name.encode().ljust(64, b'\0') + bytes(0x94 - 72)
        blob += data
    return blob + table + struct.pack('<II', 1, len(files))


def synthetic_box(seed):
    """MESSAGEBOX with all eight MB_* buttons (the shipped one has OK, CANCEL, USERBTN)."""
    r = random.Random(f'box:{seed}')
    buttons = []
    for i, n in enumerate(['MB_OK', 'MB_CANCEL', 'MB_YES', 'MB_NO', 'MB_ABORT', 'MB_RETRY', 'MB_IGNORE',
                           'MB_USERBTN']):
        buttons += [n, ['POSITION', [100 + 60 * i, 300 + r.randrange(40)], 'BITMAP', ['circlebtn_tiny'],
                        'LABEL', ['MSG_' + n[3:], 'offset', [22, 8], 'font', ['ia_small_green']],
                        'ROLLOVER', ['SOUND', ['menu_rollover', 1.0]], 'ACTIVATE', ['SOUND', ['menu_select']]]]
    body = ['IMAGE_PATH', ['shared'],
            'FONTS', [[0, 'Lucida Console', 13, [178, 51, 30], 1], [1, 'Arial', 12, [255, 255, 255], 1, 700, 'CENTER']],
            'BACKGROUND_IMAGES', [['message_box', 212, 158]],
            'BUTTONS', buttons,
            'PRIMITIVES', ['TITLE', ['POSITION', [230, 170], 'FONT', [r.randrange(2)]],
                           'MESSAGE', ['POSITION', [230, 200], 'WORDWRAP', [200, 60], 'FONT', [0]]],
            'CURSOR', ['BITMAP', ['cursor_circle_orange'], 'CENTER', [1], 'CAPTURE', [r.randrange(2)]]]
    return [['MESSAGEBOX', body]]


def mouse_frames(r, positions, style):
    frames = []

    def frame(x, y, left, right=0):
        frames.append([x & 0xffffffff, y & 0xffffffff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, left, right, 0])
    for _ in range(r.randrange(0, 25)):
        frame(r.randrange(-10, 650), r.randrange(-10, 490), r.choice([0, 0, 1, 2, 4]), r.choice([0, 0, 4]))
    shown = [b for b in STYLE_BUTTONS.get(style & 0xf, []) if b in positions]
    if shown:
        x, y = positions[r.choice(shown)]
        frame(x + 3, y + 3, 1)
        for _ in range(r.randrange(0, 4)):
            frame(x + 3, y + 3, 2)
        frame(x + 3 + r.randrange(0, 2), y + 3, 4)
        for b in shown:  # in case the first release missed
            frame(positions[b][0] + 3, positions[b][1] + 3, 4)
    return frames


class Original:
    def __init__(self, seed, data, start, frames, mode, dt, no_draw, text, caption, style):
        self.t = Trace(seed=seed)
        w = self.w = TwodWorld(self.t, seed)
        e = self.e = w.emu
        t = self.t
        # every image there: the loop ends only on a button
        base_image = w.answers['0x005179d0']
        w.answers['0x005179d0'] = lambda rr, k, a: base_image(rr, k, a) or w.fake_texture(32, 16)
        w.answers.update({f'{a:#010x}': (lambda v: lambda rr, k, aa: v)(0x5e000000 + (a & 0xfff))
                          for a in (0x576db0, 0x576c70, 0x576d80, 0x576cd0, 0x576ca0)})
        w.answers['0x00579840'] = lambda rr, k, a: rr.randrange(2)
        self.tree = w.read_tree(data, start)
        e.stub(ZRDR_LOAD, '@zrdr_load', 'c,d', None, lambda emu, key, a, pop: emu._ret(self.tree, pop))
        e.w32(0x72f348, mode)
        e.wf(0x72c52c, dt)
        screen = e.world_alloc(16)
        for i, v in enumerate((0, 0, 640, 480)):
            e.world_w32(screen + 4 * i, v)
        w.answers['0x00579880'] = screen
        e.stub(0x579880, '0x00579880', '')

        def target(emu, key, a, pop):
            wp, hp, dp, pp = a
            base = t.handle(key, ())
            for p, v in ((wp, 640), (hp, 480), (dp, 16), (pp, 1280)):
                emu.w32(p, v)
            emu._ret(base, pop)
        w.answers['0x00558080'] = 0x7e000000
        e.stub(0x558080, '0x00558080', 'c,d,s0,s1', None, target)

        def set_target(emu, key, a, pop):
            base, rp, depth, pitch = a
            t.handle(key, (base, *[emu.r32(rp + 4 * i) for i in range(4)], depth, pitch))
            emu._ret(0, pop)
        e.stub(0x557f90, '0x00557f90', 'c,d,s0,s1', None, set_target)

        def flip(emu, key, a, pop):
            ra, rb, c, d = a
            t.handle(key, (*[emu.r32(ra + 4 * i) for i in range(4)], *[emu.r32(rb + 4 * i) for i in range(4)], c, d))
            emu._ret(0, pop)
        e.stub(0x576ea0, '0x00576ea0', 'c,d,s0,s1', None, flip)
        self.polls = 0

        def poll(emu, key, a, pop):
            t.handle(key, (a[0],))
            w.frame = self.polls
            self.polls += 1
            if self.polls > len(frames) + 5:
                raise RuntimeError('the box never answered')
            emu._ret(0, pop)
        e.stub(0x51b360, '0x0051b360', 'cl', None, poll)
        e.stub(0x575a00, '0x00575a00', '')
        for a in (0x576db0, 0x576c70, 0x576d80, 0x576cd0, 0x576ca0):
            e.stub(a, f'{a:#010x}', '')
        e.stub(0x579840, '0x00579840', 'c')
        e.w32(0x72f358, e.new_stub('[0x0072f358]', 'c,d', 0))
        w.mouse_frames = frames
        self.args = (no_draw, text, caption, style)

    def run(self):
        e, t = self.e, self.t
        self.snapshot = e.snapshot()
        no_draw, text, caption, style = self.args
        mb = e.alloc(0x12494)
        e.call(CTOR, ecx=mb, stack=(e.world_string('dlgMessage.zrd'), e.world_string('MESSAGEBOX'), no_draw))
        r = e.call(RUN, ecx=mb, stack=(e.world_string(text), e.world_string(caption), style, 0))
        t.handle('result', (r, e.r32(0x5f49b8), e.r32(mb + 0x11000)))
        e.call(DTOR, ecx=mb)
        return r


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 90
    data = READER.read_bytes()
    start = next(s for n, s, _ in entries(data) if n == 'dlgMessage.zrd')
    tree, _ = parse_node(data, start)
    body = tree[0][1]
    buttons = find(body, 'BUTTONS')
    positions = {buttons[i]: tuple(find(buttons[i + 1], 'POSITION')) for i in range(0, len(buttons) - 1, 2)}
    ok_runs = 0
    answers = {}
    calls = 0
    synth_path = ROOT / 'build' / 'twod_msgbox.zbd'
    shipped = (READER, data, start, positions)
    for seed in range(runs):
        r = random.Random(seed)
        if seed % 2:
            box = synthetic_box(seed)
            blob = write_node(box)
            synth_path.write_bytes(archive([('dlgMessage.zrd', blob)]))
            bb = find(box[0][1], 'BUTTONS')
            where = (synth_path, blob, 0, {bb[i]: tuple(find(bb[i + 1], 'POSITION')) for i in range(0, len(bb) - 1, 2)})
            style = r.randrange(6) | r.choice([0, 0, 0x10, 0x40])
        else:
            where = shipped
            style = r.choice([0, 1, 3, 5]) | r.choice([0, 0x30])  # the shipped box can answer these
        path, file_data, file_start, positions_here = where
        frames = mouse_frames(r, positions_here, style)
        mode = r.randrange(2)
        dt = r.choice([0.0, 1 / 60, 0.05, 0.3])
        no_draw = 1 if r.random() < 0.3 else 0
        text = r.choice(['Quit_MechWarrior_3?', 'Really?', 'A longer question that wraps', ''])
        caption = r.choice(['MechWarrior 3', 'Warning', ''])
        o = Original(seed, file_data, file_start, frames, mode, dt, no_draw, text, caption, style)
        res = o.run()
        rep = Replay(o.t, o.e, o.snapshot)
        probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
        flat = (ctypes.c_uint32 * (15 * len(frames)))(*[v for f in frames for v in f])
        got = probe.probe_twod_msgbox(str(path).encode(), no_draw, text.encode(), caption.encode(), style, flat,
                                      len(frames))
        a, _ = canonical(o.t.log, '', o.w.stub_textures)
        b, _ = canonical(rep.log, '', o.w.stub_textures)
        ok = compare(f'message box seed {seed}', a, b) and got == res
        if got != res:
            print(f'seed {seed}: answers {res} and {got}')
        ok_runs += ok
        answers[res] = answers.get(res, 0) + 1
        calls += len(a)
    print(f'zTwoD message box: {ok_runs}/{runs} runs identical (answers {dict(sorted(answers.items()))}, '
          f'{calls} calls)')
    return 0 if ok_runs == runs else 1


if __name__ == '__main__':
    sys.exit(main())
