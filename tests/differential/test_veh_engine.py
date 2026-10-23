"""A vehicle's engine sound against the original x86 (docs/vehicles.md, The
engine sound): the claim 0x4adce0 over 0x4adc60 and the release 0x4adcd0
over 0x4adc30, on two vehicles' EngineSounds sharing one entry, with
zSound's 3D update 0x571f30, play 0x56e7e0 and lock 0x56f3f0 stubbed; the
far point 0x4adb60 the original's own.

    cmake --build build && .venv/bin/python tests/differential/test_veh_engine.py [runs]

Each run: an entry claimed by neither, one or the other, a voice or none,
either EngineSound without an entry; then a dozen claims and releases in
random order, by either, a velocity or none, a doppler byte, the play
failing now and then. Compared: the traces (the place by value), each
claim's answer and the entry after each step.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402

U, P = ctypes.c_uint32, ctypes.c_void_p


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def run(probe, seed):
    r = rnd(seed, 'engine')
    t = Trace()
    script = t.script
    e = Emu(t)
    e.call(0x4adb60)  # the far point [0x0060b150]
    entry = e.world_alloc(0xc)
    eng = [e.world_alloc(0x8), e.world_alloc(0x8)]
    owner = r.choice([0, 0, 1, 2])
    sound = e.world_alloc(0x10)
    voice = r.choice([0, e.world_alloc(0x10)])
    has = [int(r.random() < 0.9), int(r.random() < 0.9)]
    e.w32(entry, [0, eng[0], eng[1]][owner])
    e.w32(entry + 4, sound)
    e.w32(entry + 8, voice)
    for i in range(2):
        e.w32(eng[i], 0x5c8b90)
        e.w32(eng[i] + 4, entry if has[i] else 0)

    def update(em, key, a, pop):
        em.t.handle(key, (a[0],) + tuple(em.r32(a[1] + 4 * j) for j in range(3)) + (int(a[2] != 0), a[3] & 0xff))
        v = em.t.script.pop(key + '.answer', 0)
        em.t.override(key, v)
        em._ret(v, pop)
    e.stub(0x571f30, spec='c,d,s0,s1', fn=update)

    def play(em, key, a, pop):
        em.t.handle(key, (a[0],) + tuple(em.r32(a[1] + 4 * j) for j in range(3)) + (a[2], int(a[3] != 0)))
        v = em.t.script.pop(key + '.answer', 0)
        em.t.override(key, v)
        em._ret(v, pop)
    e.stub(0x56e7e0, spec='c,d,s0,s1', fn=play)
    e.stub(0x56f3f0, spec='c')
    probe.probe_veng_set(owner, sound, voice, has[0], has[1])
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    name = f'engine seed {seed} (owner {owner}, voice {"set" if voice else "none"}, entries {has})'
    far = (ctypes.c_float * 3)()
    probe.probe_veng_far(far)
    want_far = [struct.unpack('<f', struct.pack('<I', e.r32(0x60b150 + 4 * j)))[0] for j in range(3)]
    if list(far) != want_far:
        print(f'{name}: far point original {want_far} rebuild {list(far)}')
        return False
    pos, vel = e.world_alloc(12), e.world_alloc(12)
    img = (U * 3)()
    for step in range(12):
        which = r.randrange(2)
        if r.random() < 0.4:
            e.call(0x4adcd0, ecx=eng[which])
            probe.probe_veng_release(which)
            got = want = None
        else:
            p = [f32(r.uniform(-3000, 3000)) for _ in range(3)]
            v = [f32(r.uniform(-30, 30)) for _ in range(3)]
            for j in range(3):
                e.w32(pos + 4 * j, struct.unpack('<I', struct.pack('<f', p[j]))[0])
                e.w32(vel + 4 * j, struct.unpack('<I', struct.pack('<f', v[j]))[0])
            use_vel = r.random() < 0.5
            dop = r.choice([0, 1, 0x101, 0xff])
            script['0x00571f30.answer'] = r.choice([0, 1, -1]) & 0xffffffff
            script['0x0056e7e0.answer'] = r.choice([0, e.world_alloc(0x10), e.world_alloc(0x10)])
            want = e.call(0x4adce0, ecx=eng[which], stack=(pos, vel if use_vel else 0, dop))
            script.pop('0x00571f30.answer', None)
            script.pop('0x0056e7e0.answer', None)
            got = probe.probe_veng_play(which, (ctypes.c_float * 3)(*p), (ctypes.c_float * 3)(*v) if use_vel else None,
                                        dop) & 0xffffffff
            if want != got:
                print(f'{name}: step {step} claim original {want:#x} rebuild {got:#x}')
                return False
        probe.probe_veng_image(img)
        o = e.r32(entry)
        wo = 1 if o == eng[0] else 2 if o == eng[1] else 0 if o == 0 else -1
        if [wo, e.r32(entry + 4), e.r32(entry + 8)] != list(img):
            print(f'{name}: step {step} entry original {[wo, e.r32(entry + 4), e.r32(entry + 8)]} rebuild {list(img)}')
            return False
    return compare(name, t.log, rep.log)


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_veng_set.argtypes = [ctypes.c_int32, U, U, ctypes.c_int32, ctypes.c_int32]
    probe.probe_veng_play.argtypes = [ctypes.c_int32, P, P, ctypes.c_int32]
    probe.probe_veng_play.restype = ctypes.c_int32
    probe.probe_veng_image.argtypes = [P]
    probe.probe_veng_far.argtypes = [P]
    probe.probe_vb_hooks(1)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 400
    for seed in range(runs):
        if not run(probe, seed):
            sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} engine sounds claimed and released as the original')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
