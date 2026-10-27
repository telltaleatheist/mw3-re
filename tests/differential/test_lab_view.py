"""The Mech Lab's 3D view (mech3/lab_view, docs/mechlab.md section 9) and the
zClass world calls it brings (cls_world: 0x4f1bd0 create, 0x4f2500 origin,
0x4f2530 size, 0x4f2560 the partition grid) against the original x86 in
Unicorn, the FPU at 24 bits as in Direct3D.

    cmake --build build && .venv/bin/python -P tests/differential/test_lab_view.py [runs, 300]

On test_anim_run's c1 world (the node pool up, as when the lab opens from a
briefing), both sides:

- make a world, give it an origin and a size (in either order) and a grid of
  random cells (the lab's own first: -1024, 1024; 2048, -2048; 2048, -2048),
  and compare its data and every cell (the pointers left out);
- make a camera and an object3d at a random rotation and run the lab's frame
  step (0x450690: the camera's field of view, place and rotation, the
  object turned by the frame's time, the draw 0x4f73f0) with a random time
  and camera; the original's Lab State is a block holding the entry, the
  camera's wrapper and the camera, 0x44fd20 answers the object and the draw
  is recorded. Compared: the camera's data, the object's, the draw's
  camera, node, hardware flag and zone mode, and the zone mode after;
- the armour section's highlight (0x4504d0 with 0x450470, item 9b) on a tree
  of a random subset of the eight part names and some near misses under one
  root (0x44fd20 answers it), one section then another (as switching
  sections), every section and a few out of range:
  each part's object3d data (its blend flag, colour and weight).
Every word compared exactly (both sides are bit for bit).
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_anim_run import Compare, Emu, Port, f2u, f32, probe  # noqa: E402
from test_gamez import ZBD  # noqa: E402

probe.lv_world.argtypes = [ctypes.c_float] * 6 + [ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
probe.lv_highlight.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
probe.lv_frame.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_float, ctypes.c_void_p, ctypes.c_int]

WORLD_CREATE, ORIGIN, SIZE, GRID = 0x4f1bd0, 0x4f2500, 0x4f2530, 0x4f2560
CAMERA_CREATE, OBJECT_CREATE, SET_ROTATION = 0x4f32e0, 0x4fb740, 0x4fbd80
FRAME, MODEL_NODE, DRAW, HIGHLIGHT = 0x450690, 0x44fd20, 0x4f73f0, 0x4504d0
SET_NAME, ADD_CHILD = 0x4eb300, 0x4eb9b0
PARTS = ['torso', 'larm', 'rarm', 'lthigh', 'lfoot', 'rthigh', 'rfoot', 'head']
DECOYS = ['Torso', 'larm2', 'lfoo', 'rfoot_a', 'gun', 'o']
ZONE_MODE, FROZEN, DOUBLED = 0x5ea1d0, 0x605aa4, 0x605aa8
WORLD_POINTERS = (0x0c, 0x90, 0xa4, 0xa8, 0xb0, 0xb4)
CELL_POINTERS = (0x3c, 0x44)


class LabEmu(Emu):
    def __init__(self):
        super().__init__()
        self.obj = 0
        self.drawn = None
        self.cpu.stub_function(MODEL_NODE, lambda c: self.obj)
        self.cpu.stub_function(DRAW, self.draw, 4)

    def draw(self, c):
        self.drawn = [int(c.ecx != 0), int(c.edx != 0), c.arg(0), self.u(ZONE_MODE)]
        return 0

    def words(self, addr, n, zero=()):
        b = bytearray(self.cpu.read(addr, n))
        for o in zero:
            b[o:o + 4] = bytes(4)
        return list(struct.unpack(f'<{n // 4}I', bytes(b)))

    def world(self, ox, oz, sx, sz, cx, cz, size_first):
        cpu = self.cpu
        cpu.call(WORLD_CREATE)
        from unicorn.x86_const import UC_X86_REG_EAX
        w = cpu.uc.reg_read(UC_X86_REG_EAX)
        calls = [(ORIGIN, (f2u(ox), f2u(oz))), (SIZE, (f2u(sx), f2u(sz)))]
        for fn, a in (calls[::-1] if size_first else calls):
            cpu.call(fn, ecx=w, stack_args=a)
        cpu.call(GRID, ecx=w, stack_args=(f2u(cx), f2u(cz)))
        d = self.u(w + 0x38)
        out = self.words(d, 0xbc, WORLD_POINTERS)
        cols, rows, grid = self.u(d + 0x88), self.u(d + 0x8c), self.u(d + 0x90)
        for r in range(rows):
            row = self.u(grid + 4 * r)
            for c in range(cols):
                out += self.words(row + 0x48 * c, 0x48, CELL_POINTERS)
        return out

    def named(self, name):
        from unicorn.x86_const import UC_X86_REG_EAX
        self.cpu.call(OBJECT_CREATE)
        n = self.cpu.uc.reg_read(UC_X86_REG_EAX)
        p = self.alloc_fixed(len(name) + 1)
        self.cpu.write(p, name.encode() + b'\0')
        self.cpu.call(SET_NAME, ecx=n, edx=p)
        return n

    def highlight(self, names, first, section):
        root = self.named('root')
        parts = []
        for name in names:
            n = self.named(name)
            self.cpu.call(ADD_CHILD, ecx=root, edx=n)
            parts.append(n)
        self.obj = root
        state = self.alloc_fixed(0x80)
        self.cpu.u32(state + 0x2c, 1)  # an entry (0x44fd20 is stubbed: the root)
        for s in (first, section):  # the section shown before, then this one
            self.cpu.call(HIGHLIGHT, ecx=state, stack_args=(s & 0xffffffff,))
        out = []
        for n in parts:
            out += self.words(self.u(n + 0x38), 0x90)
        return out

    def frame(self, cam, rot, dt):
        cpu = self.cpu
        from unicorn.x86_const import UC_X86_REG_EAX
        cpu.call(CAMERA_CREATE)
        camera = cpu.uc.reg_read(UC_X86_REG_EAX)
        cpu.call(OBJECT_CREATE)
        self.obj = cpu.uc.reg_read(UC_X86_REG_EAX)
        cpu.call(SET_ROTATION, ecx=self.obj, stack_args=tuple(f2u(v) for v in rot))
        wrapper = self.alloc_fixed(0x18)
        cpu.u32(wrapper + 0x4, camera)
        cpu.u32(wrapper + 0x14, 1)
        state = self.alloc_fixed(0x80)
        cpu.u32(state + 0x2c, 1)  # an entry (0x44fd20 is stubbed)
        cpu.u32(state + 0x18, wrapper)
        cpu.write(state + 0x48, struct.pack('<6f', *cam))
        cpu.write(FROZEN, b'\0')
        cpu.write(DOUBLED, b'\0')
        cpu.u32(ZONE_MODE, 1)
        self.drawn = [0xffffffff] * 4
        cpu.call(FRAME, ecx=state, stack_args=(f2u(dt),))
        return (self.words(self.u(camera + 0x38), 0x1e8) + self.words(self.u(self.obj + 0x38), 0x90) +
                [v & 0xffffffff for v in self.drawn] + [self.u(ZONE_MODE)])


def port_words(fn, *args):
    buf = (ctypes.c_uint32 * (1 << 20))()
    n = fn(*args, buf, len(buf))
    assert 0 <= n <= len(buf)
    return list(buf[:n])


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    rng = random.Random(9)
    world, anim = ZBD / 'c1' / 'gamez.zbd', ZBD / 'c1' / 'anim.zbd'
    emu, port = LabEmu(), Port()
    emu.open(world, anim)
    port.open(world, anim)
    cmp = Compare(tol=0.0)  # bit for bit: integers would pass as tiny floats within a tolerance
    cells = turns = wraps = 0
    for k in range(runs):
        if k == 0:
            args = (-1024.0, 1024.0, 2048.0, -2048.0, 2048.0, -2048.0)  # the lab's
        else:
            ox, oz = f32(rng.uniform(-4000, 0)), f32(rng.uniform(0, 4000))
            sx, sz = f32(rng.uniform(100, 8000)), f32(-rng.uniform(100, 8000))
            args = (ox, oz, sx, sz, f32(rng.choice((sx, rng.uniform(64, 2048)))),
                    f32(-rng.choice((-sz, rng.uniform(64, 2048)))))
        size_first = k > 0 and rng.random() < 0.5  # the lab sets the origin first
        want = emu.world(*args, size_first)
        got = port_words(probe.lv_world, *args, int(size_first))
        cmp(f'world {k} {args}', want, got)
        cells += (len(want) - 0xbc // 4) // 18
        if k == 0:
            cam = (0.0, 5.0, -11.0, 0.02, 3.1415927, 0.0)  # the lab's
        else:
            cam = tuple(f32(rng.uniform(-20, 20)) for _ in range(3)) + tuple(
                f32(rng.uniform(-3.2, 3.2)) for _ in range(3))
        rot = tuple(f32(rng.uniform(-3.2, 3.2)) for _ in range(3))
        if rng.random() < 0.4:  # near pi, so the turn wraps
            rot = (rot[0], f32(rng.uniform(2.6, 3.1415)), rot[2])
        dt = f32(rng.choice((1 / 30, 1 / 60, 0.1, 0.5, 2.0, rng.uniform(0, 4))))
        want = emu.frame(cam, rot, dt)
        got = port_words(probe.lv_frame, struct.pack('<6f', *cam), struct.pack('<3f', *rot), dt)
        cmp(f'frame {k} cam {cam} rot {rot} dt {dt}', want, got)
        turns += 1
        wraps += rot[1] + 0.8 * dt > 3.1415927
    lit = 0
    for k in range(runs):  # the armour section's highlight (0x4504d0, item 9b)
        names = rng.sample(PARTS, rng.randrange(0, len(PARTS) + 1)) + rng.sample(DECOYS, rng.randrange(0, 3))
        rng.shuffle(names)
        sections = (-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -2)
        first, section = rng.choice(sections), rng.choice(sections)
        want = emu.highlight(names, first, section)
        got = port_words(probe.lv_highlight, '\n'.join(names).encode(), first, section)
        cmp(f'highlight {k} sections {first} then {section} parts {names}', want, got)
        lit += sum(1 for i in range(0, len(want), 0x90 // 4) if want[i] & 4)
    print(f'lab view highlight: {runs}/{runs} runs identical ({lit} parts blending)')
    print(f'lab view: {runs}/{runs} runs identical ({cells} grid cells, {turns} frame steps, {wraps} past pi); '
          f'{cmp.words} words, {cmp.approx} within tolerance (worst {cmp.worst:.2e})')


if __name__ == '__main__':
    main()
