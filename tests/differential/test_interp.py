"""The ported script interpreter (gamez/zinterp, mech3/mech3_interp;
docs/interp.md) against the original x86: interp.zbd's archive and its
file-time checks, the line reader and tokenizer, macros and variables, the
preprocessor, the dispatcher and every command's argument parsing.

    cmake --build build && .venv/bin/python tests/differential/test_interp.py

Each run is a short command list fed to both sides: mounts, preset macros,
settings, then `source` calls (or single lines, or the scroll list). The
original runs from its real MW3 interpreter object (ctor 0x410700, the
`gravity`/`ceiling` variables) in Unicorn, with the C runtime served from
Python (files from the same mounts as the port's zutl::open). Every engine
function the commands call is stubbed in both and recorded with its decoded
arguments (fake nodes, models and textures stand in; out-pointers get
deterministic values); log, printf and zerr text, each line's tokens, each
dispatch and each line's end (misses, error, quit, skip depth, current node)
are recorded too, and the state after is dumped: macros, variables, globals,
the model being built, the ScrollAlways and clutter lists, every fake node and
model. The traces must be identical.

Runs: every shipped .gs (as the mission load runs it, its size script first,
hardware on and off, and with GameZReadZBDFile failing), every .gw on its own,
the cockpit scripts with a current node, the archive rejected by a stale
loose copy and a newer loose copy, and crafted scripts covering every command
and the macro, preprocessor, tokenizer and error edge cases.
"""
import ctypes
import os
import re
import shutil
import struct
import sys
from pathlib import Path

os.environ.setdefault("MW3_X87", "24")  # the precision in play (CLAUDE.md)
sys.path.insert(0, str(Path(__file__).parent))
from original import STUBS, ROOT, Original  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_ESI  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_interp.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
GAME = ROOT / 'game' / 'mw3-1.2'
WORK = ROOT / 'build' / 'interp_test'

IAT = {'malloc': 0x5bb664, 'calloc': 0x5bb60c, 'realloc': 0x5bb610, 'free': 0x5bb65c, 'strdup': 0x5bb670,
       'strncmp': 0x5bb654, 'strchr': 0x5bb63c, 'strpbrk': 0x5bb548, 'strncat': 0x5bb600, 'strncpy': 0x5bb648,
       'stricmp': 0x5bb580, 'iswspace': 0x5bb544, 'atoi': 0x5bb5e0, 'atof': 0x5bb640, 'printf': 0x5bb5f0,
       'sprintf': 0x5bb658, 'mkdir': 0x5bb618, 'errno': 0x5bb550, 'fopen': 0x5bb628, 'fclose': 0x5bb630,
       'fread': 0x5bb594, 'fseek': 0x5bb59c, 'ftell': 0x5bb598, 'fgetc': 0x5bb54c, 'stat': 0x5bb588,
       'difftime': 0x5bb540, 'DeleteFileA': 0x5bb0d4}
NEW, DELETE, ZERR, SEARCH_NEW, SEARCH_OPEN = 0x5abd26, 0x5abd1a, 0x49bfd0, 0x576100, 0x5763b0
CTOR, VAR_REGISTER, MACRO_SET, SOURCE, EXEC_LINE = 0x410700, 0x595fa0, 0x595c60, 0x595990, 0x595510
SET_MECH, LIST_CTOR, SCROLL_CLEAR, SCROLL_TICK, ALWAYS_TICK = 0x4108a0, 0x5962a0, 0x5964c0, 0x596380, 0x5963a0
SCROLL, CLUTTER, LINEBUF = 0x731430, 0x731888, 0x730c20
USE_ARCHIVE, HARDWARE, PERSP, VERBOSE = 0x5f4244, 0x72f348, 0x74a3cc, 0x73f764
GRAVITY, INV_GRAVITY, CEILING, DAMAGE, BFE = 0x5e5d34, 0x5e5d38, 0x5e5d44, 0x5e1f68, 0x5ef73c
MODELS = (0x730c14, 0x730c18, 0x730c1c)
AT_LINE, AT_DISPATCH, AT_DONE = 0x595544, 0x591300, 0x5955c2
BIG = 0x48000000  # above test_gamez's heap, which the world runs keep
# The ported functions; the world runs (see main) run them for real.
REAL = {'find_node', 'find_in_tree', 'list_count', 'node_alloc', 'set_name', 'set_active', 'add_child',
        'camera_set_active', 'camera_set_clip', 'lod_add_child', 'display_set_clear_color', 'light_create',
        'object3d_create', 'object3d_add_child', 'world_set_fog_type', 'world_set_fog_color', 'world_get_fog_color',
        'world_set_fog_altitude', 'world_set_fog_density', 'world_set_fog_range', 'world_get_fog_range',
        'find_texture', 'pack_rgb', 'clouds_on', 'clouds_period', 'clouds_wavelength', 'clouds_level',
        'clouds_heading', 'anim_set_zbd_path', 'set_action', 'set_action_priority', 'write_prepare_a',
        'write_prepare_b', 'model_set_scroll', 'model_scroll_tick'}
NODE_BLOCKS, NODE_NBLOCKS, NODE_BLOCK_SIZE, NODE_LIVE, NODE_CAP = 0x60bf38, 0x60bf30, 0x5ea830, 0x60bf3c, 0x60bf34
MODEL_POOL, MODEL_SIZE, ANIM_PATH = 0x7402e4, 0x7402e0, 0x60c570

# Every engine function the commands call: name (the port's Engine member),
# address, register arguments (ECX, EDX), stack arguments, result.
# Kinds: n node, m model, t texture, s string, f float, i int, o float out,
# v float[3] (or a pointer into the model being built), B into the build,
# k a local. Results: i int, h u16, m new model, t texture, N found by name,
# W none, nC a new node of class C, '' void.
ENGINE = [
    ('find_node', 0x4f7fe0, 'is', '', 'N'), ('find_in_tree', 0x4f9d60, 'ns', '', 'N'),
    ('walk_prefix', 0x4f8b80, 'si', '', 'W'), ('list_count', 0x4f7f80, 'i', '', 'i'),
    ('list_print', 0x4f7fa0, 'i', '', ''), ('node_alloc', 0x4eacd0, '', '', 'n0'),
    ('set_active', 0x4eb170, 'ni', '', 'i'), ('add_child', 0x4eb9b0, 'nn', '', 'i'),
    ('delete_child', 0x4ebb40, 'nn', '', 'i'), ('delete_tree', 0x4f9050, 'n', '', 'i'),
    ('free_node', 0x4f3390, 'n', '', 'i'), ('free_world', 0x4f1c60, 'n', '', 'i'),
    ('set_model', 0x4eb370, 'nm', '', 'i'), ('set_can_modify', 0x4eb260, 'ni', '', 'i'),
    ('set_overwrite', 0x4eb960, 'ni', '', 'i'), ('set_action_priority', 0x4eb5b0, 'ni', '', 'i'),
    ('set_altitude_surface', 0x4eb620, 'ni', '', 'i'), ('set_intersect_surface', 0x4eb6d0, 'ni', '', 'i'),
    ('set_intersect_bbox', 0x4eb750, 'ni', '', 'i'), ('set_proximity', 0x4eb790, 'ni', '', 'i'),
    ('set_landmark', 0x4eb7d0, 'ni', '', 'i'), ('set_lighting', 0x4f9e50, 'ni', '', 'i'),
    ('pool_alive', 0x4f8f50, '', '', 'i'), ('read_zbd', 0x4ff520, 's', '', 'i'),
    ('write_prepare_a', 0x4f7f70, '', '', ''), ('write_prepare_b', 0x4f7bd0, '', '', ''),
    ('write_zbd', 0x4fe8e0, 's', '', 'i'), ('camera_create', 0x4f32e0, '', '', 'n1'),
    ('camera_set_rotate', 0x4f36b0, 'n', 'fff', 'i'), ('camera_get_translate', 0x4f39d0, 'no', 'oo', 'i'),
    ('camera_set_active', 0x4f3420, 'ni', '', 'i'), ('camera_set_dynamic_lod', 0x4f32a0, 'i', 'f', ''),
    ('camera_set_fov', 0x4f3d90, 'n', 'ff', 'i'), ('camera_set_horizon_xz', 0x4f4100, 'nn', '', 'i'),
    ('camera_set_horizon', 0x4f4090, 'nn', '', 'i'), ('camera_set_lod_multiplier', 0x4f3ff0, 'n', 'f', 'i'),
    ('camera_set_clip', 0x4f3a70, 'n', 'ff', 'i'), ('camera_get_clip', 0x4f3b00, 'no', 'o', 'i'),
    ('camera_set_object_hse_test', 0x4f3550, 'i', '', 'i'), ('camera_set_window', 0x4f3620, 'nn', '', 'i'),
    ('camera_set_world', 0x4f3560, 'nn', '', 'i'), ('camera_set_translate', 0x4f3870, 'n', 'fff', 'i'),
    ('display_create', 0x4fa4c0, '', '', 'n4'), ('display_origin', 0x4fa600, 'ni', 'i', 'i'),
    ('display_resolution', 0x4fa580, 'ni', 'i', 'i'), ('display_set_clear_color', 0x4fa680, 'n', 'fff', 'i'),
    ('window_create', 0x4f9e90, '', '', 'n3'), ('window_add_clear_vertex', 0x4fa2c0, 'nv', '', 'i'),
    ('window_buffer', 0x4fa1c0, 'ni', '', 'i'), ('window_close_clear_polygon', 0x4fa3e0, 'n', '', 'i'),
    ('window_origin', 0x4fa0b0, 'ni', 'i', 'i'), ('window_resolution', 0x4f9fa0, 'ni', 'i', 'i'),
    ('window_set_clear_polygon', 0x4fa230, 'ni', '', 'i'), ('light_create', 0x4fa720, '', '', 'n9'),
    ('light_set_ambient', 0x4fa940, 'n', 'f', 'i'), ('light_set_color', 0x4fb510, 'n', 'fff', 'i'),
    ('light_set_diffuse', 0x4fa9b0, 'n', 'f', 'i'), ('light_set_directed', 0x4faaa0, 'n', '', 'i'),
    ('light_set_directional', 0x4faa20, 'ni', '', 'i'), ('light_set_orientation', 0x4faff0, 'n', 'fff', 'i'),
    ('light_set_point', 0x4fab20, 'n', '', 'i'), ('light_set_ranges', 0x4fadf0, 'n', 'ff', 'i'),
    ('light_set_saturated', 0x4faba0, 'ni', '', 'i'), ('light_set_static', 0x4fad30, 'ni', '', 'i'),
    ('light_set_subdivide', 0x4fac70, 'ni', '', 'i'), ('light_set_translate', 0x4faf80, 'n', 'fff', 'i'),
    ('lod_create', 0x4fe030, '', '', 'n6'), ('lod_add_child', 0x4fe0a0, 'nn', '', 'i'),
    ('seq_create', 0x4fdc70, '', '', 'n7'), ('seq_add_child', 0x4fdcd0, 'nn', 'if', 'i'),
    ('seq_set_active', 0x4fde50, 'ni', '', 'i'), ('seq_set_loop', 0x4fded0, 'ni', '', 'i'),
    ('seq_set_pause', 0x4fdf10, 'ni', '', 'i'), ('seq_set_repeat', 0x4fde90, 'ni', '', 'i'),
    ('object3d_create', 0x4fb740, '', '', 'n5'), ('object3d_add_child', 0x4fb7a0, 'nn', '', 'i'),
    ('object3d_get_translate', 0x4fc020, 'no', 'oo', 'i'), ('object3d_rotate', 0x4fbd80, 'n', 'fff', 'i'),
    ('object3d_scale', 0x4fbc10, 'n', 'fff', 'i'), ('object3d_set_opacity_is_set', 0x4fbb90, 'ni', '', 'i'),
    ('object3d_set_opacity', 0x4fba20, 'n', 'f', 'i'), ('object3d_translate', 0x4fc0b0, 'n', 'fff', 'i'),
    ('world_create', 0x4f1bd0, '', '', 'n2'), ('world_add_light', 0x4f2d50, 'nn', '', 'i'),
    ('world_extents', 0x4f2530, 'n', 'ff', 'i'), ('world_origin', 0x4f2500, 'n', 'ff', 'i'),
    ('world_partition_tolerance', 0x4f2860, 'n', 'ff', 'i'), ('world_partition_max_dec', 0x4f2880, 'ni', '', 'i'),
    ('world_partition', 0x4f2560, 'n', 'ff', 'i'), ('world_set_fog_type', 0x4f23e0, 'ni', '', 'i'),
    ('world_set_fog_color', 0x4f23f0, 'n', 'fff', 'i'), ('world_get_fog_color', 0x4f24a0, 'no', 'oo', ''),
    ('world_set_fog_altitude', 0x4f2420, 'n', 'ff', 'i'), ('world_set_fog_density', 0x4f2460, 'n', 'f', 'i'),
    ('world_set_fog_range', 0x4f2440, 'n', 'ff', 'i'), ('world_get_fog_range', 0x4f24c0, 'no', 'o', ''),
    ('world_set_virtual_map_size', 0x4f1dc0, 'ni', 'i', 'i'),
    ('world_set_virtual_partition', 0x4f1d90, 'ni', '', 'i'),
    ('texture_add', 0x518c60, 's', '', 't'), ('find_texture', 0x518220, 's', '', 't'),
    ('texture_directory', 0x516f50, 's', '', 'i'), ('pack_rgb', 0x5793c0, 'v', '', 'h'),
    ('matl_new', 0x536a70, 'B', '', 'i'), ('material_array_size', 0x536a60, 'i', '', 'i'),
    ('model3d_array_size', 0x5269a0, 'i', '', 'i'), ('model_new', 0x538fe0, '', '', 'm'),
    ('model_set_facade', 0x539cc0, 'mi', '', 'i'), ('model_do_shadows', 0x526a70, 'i', '', 'i'),
    ('polygon_material', 0x536ad0, 'B', '', 'i'), ('polygon_prepare', 0x53cc40, 'B', '', 'i'),
    ('polygon_add', 0x53c6c0, 'mi', 'BiBiBBiiiBi', 'i'), ('cycle_set_looping', 0x53bcd0, 'mi', '', 'i'),
    ('cycle_set_map', 0x53bd10, 'mt', '', 'i'), ('cycle_set_on', 0x53bc30, 'mi', '', 'i'),
    ('cycle_set_speed', 0x53bcf0, 'm', 'f', 'i'), ('model_set_color', 0x53bd60, 'mi', '', 'i'),
    ('model_set_priority', 0x53bb20, 'mi', '', 'i'), ('model_set_show_backface', 0x53bb50, 'mi', '', 'i'),
    ('model_set_morph', 0x53b960, 'mk', 'fii', 'i'), ('model_set_scroll', 0x526ae0, 'mi', 'ff', 'i'),
    ('model_scroll_tick', 0x541ad0, 'm', '', ''), ('texture_world_base', 0x526ac0, '', 'ff', 'i'),
    ('texture_world_per_meter', 0x526aa0, '', 'ff', 'i'), ('palette_name', 0x5975e0, 's', '', 'i'),
    ('palette_shading', 0x5269e0, 'i', '', 'i'), ('vertex_shading', 0x526a00, 'i', '', 'i'),
    ('inverse_z_tolerance', 0x526df0, '', 'f', 'i'), ('small_polygon_reject', 0x535ed0, '', 'f', 'i'),
    ('bfe_tolerance', 0x53cef0, '', 'f', ''), ('coplanar_tolerance', 0x538070, '', 'f', ''),
    ('colinear_tolerance', 0x538080, '', 'f', ''), ('write_texture_type', 0x5426a0, 'i', '', 'i'),
    ('write_texture_map', 0x5426b0, 't', '', 'i'), ('perspective_delta_x', 0x5580d0, 'i', '', 'i'),
    ('perspective_adaptive', 0x558120, 'ii', 'f', 'i'), ('perspective_inverse_z', 0x558170, '', 'f', 'i'),
    ('perspective_far_z', 0x558140, '', 'f', 'i'), ('clear_screen_buffer', 0x57a3c0, 'i', '', 'i'),
    ('video_dither', 0x579240, 'i', '', 'i'), ('video_wireframe', 0x579230, 'i', '', 'i'),
    ('clouds_on', 0x543910, 'i', '', ''), ('clouds_period', 0x543920, '', 'f', ''),
    ('clouds_wavelength', 0x543940, '', 'f', ''), ('clouds_level', 0x543960, '', 'ff', ''),
    ('clouds_heading', 0x5439f0, '', 'f', ''), ('make_shadows', 0x5487d0, '', '', 'i'),
    ('wind_max_speed', 0x54b650, '', 'f', 'i'), ('wind_accel', 0x54b660, '', 'f', 'i'),
    ('wind_ang_vel', 0x54b670, '', 'f', 'i'), ('wind_static_velocity', 0x54b630, 'v', '', 'i'),
    ('lens_flare_texture', 0x5676d0, 'it', '', 'i'), ('load_soils', 0x537cb0, 's', '', 'i'),
    ('rdr_add_path', 0x5561e0, 's', '', 'i'), ('rdr_set_path', 0x5561a0, 's', '', 'i'),
    ('anim_set_zbd_path', 0x5090f0, 's', '', ''), ('anim_debug_frame', 0x507dd0, '', '', 'i'),
]
CLASS_OF = {'n0': 0, 'n1': 1, 'n2': 2, 'n3': 3, 'n4': 4, 'n5': 5, 'n6': 6, 'n7': 7, 'n9': 9}
SPECIAL_INT = {'list_count': 3, 'polygon_material': 42, 'pool_alive': 1}
ACTIONS = {0: 'none', SCROLL_TICK: 'tick', ALWAYS_TICK: 'always'}


def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def esc(b):
    if b is None:
        return 'null'
    out = '"'
    for c in b:
        out += chr(c) if 0x20 <= c < 0x7f and c not in b'\\"|' else '\\x%02x' % c
    return out + '"'


def msvc_atof(s):
    """MSVCRT's atof grammar (crt::msvc_atof)."""
    i, n = 0, len(s)
    while i < n and (s[i] == 0x20 or 9 <= s[i] <= 13):
        i += 1
    t = ''
    if i < n and s[i] in b'+-':
        t += chr(s[i]); i += 1
    digits = False
    while i < n and 0x30 <= s[i] <= 0x39:
        t += chr(s[i]); i += 1; digits = True
    if i < n and s[i] == 0x2e:
        t += '.'; i += 1
        while i < n and 0x30 <= s[i] <= 0x39:
            t += chr(s[i]); i += 1; digits = True
    if not digits:
        return 0.0
    if i < n and s[i] in b'eEdD':
        j, x = i + 1, 'e'
        if j < n and s[j] in b'+-':
            x += chr(s[j]); j += 1
        if j < n and 0x30 <= s[j] <= 0x39:
            while j < n and 0x30 <= s[j] <= 0x39:
                x += chr(s[j]); j += 1
            t += x
    if t in ('+.', '-.', '.'):
        return 0.0
    return float(t)


def msvc_atoi(s):
    i, n = 0, len(s)
    while i < n and (s[i] == 0x20 or 9 <= s[i] <= 13):
        i += 1
    neg = i < n and s[i] == 0x2d
    if i < n and s[i] in b'+-':
        i += 1
    v = 0
    while i < n and 0x30 <= s[i] <= 0x39:
        v = (v * 10 + s[i] - 0x30) & 0xffffffff
        i += 1
    if neg:
        v = (-v) & 0xffffffff
    return v


def stricmp(a, b):
    a, b = a.lower(), b.lower()
    return 0 if a == b else (-1 if a < b else 1)


FMT = re.compile(rb'%([-+ #0]*)(\*|\d+)?(?:\.(\*|\d+))?(hh|h|ll|l|I64)?([diouxXcsfeEgGp%])')


class Emulated:
    def __init__(self, mounts, world=None):
        self.world = world
        if world:
            # The mission's world, read by the original 0x4ff520 (test_gamez).
            from test_gamez import Emulated as WorldEmu
            w = WorldEmu()
            head = w.read_zbd(Path(world).read_bytes(), world.encode())
            assert struct.unpack_from('<i', head)[0] == 0, world
            self.cpu = cpu = w.cpu
        else:
            self.cpu = cpu = Original()
        self.mounts = mounts
        self.out = []
        self.calls = 0
        self.fail = set()
        self.mkdir_errno = 0
        self.files = {}
        self.ids = {}
        self.by_name = {}
        self.nodes = []
        self.models = []
        self.textures = 0
        cpu.uc.mem_map(BIG, 0x2000000)
        self.next = BIG
        self.sizes = {}
        self.errno_cell = self.alloc(4)
        self.scratch = self.alloc(8)
        for name, fn in (('malloc', lambda c: self.alloc(c.arg(0))),
                         ('calloc', lambda c: self.calloc(c.arg(0) * c.arg(1))), ('realloc', self.realloc),
                         ('free', lambda c: 0), ('strdup', self.strdup), ('strncmp', self.strncmp),
                         ('strchr', self.strchr), ('strpbrk', self.strpbrk), ('strncat', self.strncat),
                         ('strncpy', self.strncpy), ('stricmp', lambda c: stricmp(self.cstr(c.arg(0)), self.cstr(c.arg(1)))),
                         ('iswspace', lambda c: 1 if (c.arg(0) & 0xffff) in (9, 10, 11, 12, 13, 32) else 0),
                         ('atoi', lambda c: msvc_atoi(self.cstr(c.arg(0)))), ('printf', self.printf),
                         ('sprintf', self.sprintf), ('mkdir', self.mkdir), ('errno', lambda c: self.errno_cell),
                         ('fopen', lambda c: self.fopen(self.cstr(c.arg(0)), self.cstr(c.arg(1)))),
                         ('fclose', lambda c: 0), ('fread', self.fread), ('fseek', self.fseek),
                         ('ftell', lambda c: self.files[c.arg(0)][1]), ('fgetc', self.fgetc), ('stat', self.stat)):
            cpu.stub_import(IAT[name], fn)
        cpu.stub_import(IAT['DeleteFileA'], self.delete_file, pop_bytes=4)
        # atof returns a double in ST0, difftime too: small x86 wrappers.
        hook = self.slot(self.atof_hook)
        cpu.import_code(IAT['atof'], b'\xff\x74\x24\x04' + b'\xe8' + struct.pack('<i', hook - (cpu.code_next + 9)) +
                        b'\x83\xc4\x04' + b'\xdd\x05' + struct.pack('<I', self.scratch) + b'\xc3')
        cpu.import_code(IAT['difftime'], bytes.fromhex('db442404 da642408 c3'))
        self.patch(NEW, lambda c: self.alloc(c.arg(0)))
        self.patch(DELETE, lambda c: 0)
        self.patch(ZERR, self.zerr)
        cpu.stubs[ZERR] = (self.zerr, 0)  # test_gamez hooks it in place
        self.patch(SEARCH_NEW, lambda c: 1)
        self.patch(SEARCH_OPEN, self.search_open, 4)
        for name, addr, regs, stack, ret in ENGINE:
            if world and name in REAL:
                continue
            self.patch(addr, lambda c, n=name, r=regs, s=stack, t=ret: self.engine(c, n, r, s, t), 4 * len(stack))
        if not world:
            self.patch(0x4eb300, self.set_name)
        self.patch(0x4eb420, self.get_model)
        if not world:
            self.patch(0x4eb450, self.set_action)
        cpu.uc.ctl_flush_tb()  # the world read ran some of the patched code already
        cpu.u32(BFE, fbits(0.0125))
        cpu.uc.hook_add(UC_HOOK_CODE, self.on_bfe, begin=0x53cf00, end=0x53cf00)
        cpu.uc.hook_add(UC_HOOK_CODE, self.on_line, begin=AT_LINE, end=AT_LINE)
        cpu.uc.hook_add(UC_HOOK_CODE, lambda *a: self.out.append('D\n'), begin=AT_DISPATCH, end=AT_DISPATCH)
        cpu.uc.hook_add(UC_HOOK_CODE, self.on_done, begin=AT_DONE, end=AT_DONE)
        # The interpreter, as the app makes it, and the two static lists.
        cpu.call(LIST_CTOR, ecx=SCROLL)
        cpu.call(LIST_CTOR, ecx=CLUTTER)
        self.this = self.calloc(0xc0)
        cpu.call(CTOR, ecx=self.this)
        cpu.call(VAR_REGISTER, ecx=self.this, stack_args=(self.string(b'gravity'), 1, GRAVITY))
        cpu.call(VAR_REGISTER, ecx=self.this, stack_args=(self.string(b'ceiling'), 1, CEILING))
        cpu.u32(self.this + 0x70, self.slot(self.on_log))

    # memory
    def alloc(self, n):
        a = self.next
        self.next += (max(n, 1) + 0xf) & ~0xf
        self.sizes[a] = n
        return a

    def calloc(self, n):
        a = self.alloc(n)
        self.cpu.write(a, bytes(n))
        return a

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        a = self.alloc(n)
        if old:
            self.cpu.write(a, self.cpu.read(old, min(self.sizes.get(old, 0), n)))
        return a

    def string(self, b):
        a = self.alloc(len(b) + 1)
        self.cpu.write(a, b + b'\0')
        return a

    def u(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def i(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def cstr(self, p):
        if not p:
            return None
        out = bytearray()
        while True:
            chunk = self.cpu.read(p + len(out), 64)
            k = chunk.find(b'\0')
            if k >= 0:
                return bytes(out + chunk[:k])
            out += chunk

    def slot(self, fn, pop=0):
        addr = STUBS + 16 * len(self.cpu.stubs)
        self.cpu.stubs[addr] = (fn, pop)
        return addr

    def patch(self, addr, fn, pop=0):
        """Sends a function of the exe to a Python stand-in (jmp into a stub slot)."""
        s = self.slot(fn, pop)
        self.cpu.write(addr, b'\xe9' + struct.pack('<i', s - (addr + 5)))

    # C runtime
    def strdup(self, c):
        return self.string(self.cstr(c.arg(0)))

    def strncmp(self, c):
        a, b, n = self.cstr(c.arg(0)), self.cstr(c.arg(1)), c.arg(2)
        a, b = a[:n], b[:n]
        return 0 if a == b else (0xffffffff if a < b else 1)

    def strchr(self, c):
        p, ch = c.arg(0), c.arg(1) & 0xff
        s = self.cstr(p)
        if ch == 0:
            return p + len(s)
        k = s.find(bytes([ch]))
        return 0 if k < 0 else p + k

    def strpbrk(self, c):
        p, chars = c.arg(0), self.cstr(c.arg(1))
        s = self.cstr(p)
        for k, ch in enumerate(s):
            if ch in chars:
                return p + k
        return 0

    def strncat(self, c):
        d, s, n = c.arg(0), self.cstr(c.arg(1)), c.arg(2)
        dl = len(self.cstr(d))
        self.cpu.write(d + dl, s[:n] + b'\0')
        return d

    def strncpy(self, c):
        d, s, n = c.arg(0), self.cstr(c.arg(1)), c.arg(2)
        self.cpu.write(d, (s + bytes(n))[:n])
        return d

    def atof_hook(self, c):
        self.cpu.write(self.scratch, struct.pack('<d', msvc_atof(self.cstr(c.arg(0)))))
        return 0

    def cformat(self, fmt, word, dword):
        out, i = bytearray(), 0
        while i < len(fmt):
            m = FMT.match(fmt, i) if fmt[i] == 0x25 else None
            if not m:
                out.append(fmt[i]); i += 1
                continue
            i = m.end()
            flags, width, prec, _, conv = m.groups()
            conv = conv.decode()
            if conv == '%':
                out += b'%'
                continue
            spec = '%' + flags.decode()
            if width == b'*':
                spec += str(struct.unpack('<i', struct.pack('<I', word()))[0])
            elif width:
                spec += width.decode()
            if prec == b'*':
                spec += '.' + str(word())
            elif prec is not None:
                spec += '.' + prec.decode()
            if conv in 'fFeEgG':
                out += ((spec + conv) % dword()).encode()
            elif conv == 's':
                s = self.cstr(word())
                out += ((spec + 's') % ('(null)' if s is None else s.decode('latin1'))).encode('latin1')
            elif conv in 'di':
                out += ((spec + 'd') % struct.unpack('<i', struct.pack('<I', word()))[0]).encode()
            elif conv == 'u':
                out += ((spec + 'd') % word()).encode()
            elif conv == 'c':
                out += ((spec + 'c') % (word() & 0xff)).encode('latin1')
            else:
                out += ((spec + conv) % word()).encode()
        return bytes(out)

    def stack_reader(self, c, first):
        k = [first]

        def word():
            v = c.arg(k[0]); k[0] += 1
            return v

        def dword():
            lo, hi = c.arg(k[0]), c.arg(k[0] + 1); k[0] += 2
            return struct.unpack('<d', struct.pack('<II', lo, hi))[0]
        return word, dword

    def printf(self, c):
        text = self.cformat(self.cstr(c.arg(0)), *self.stack_reader(c, 1))
        self.out.append('P %s\n' % esc(text))
        return len(text)

    def sprintf(self, c):
        text = self.cformat(self.cstr(c.arg(1)), *self.stack_reader(c, 2))
        self.cpu.write(c.arg(0), text + b'\0')
        return len(text)

    def on_log(self, c):
        fmt, va = self.cstr(c.arg(0)), c.arg(1)
        if fmt == b'%s Failed: no current node':
            text = b'<stray> Failed: no current node'
        else:
            p = [va]

            def word():
                v = self.u(p[0]); p[0] += 4
                return v

            def dword():
                v = struct.unpack('<d', self.cpu.read(p[0], 8))[0]; p[0] += 8
                return v
            text = self.cformat(fmt, word, dword)
        self.out.append('G %s\n' % esc(text))
        return 0

    def zerr(self, c):
        level, line = c.arg(0), c.arg(2)
        text = self.cformat(self.cstr(c.arg(3)), *self.stack_reader(c, 4)).decode('latin1')

        def sub(m):
            v = int(m.group(1), 16)
            return '0x' + self.ids[v] if v in self.ids else m.group(0)
        text = re.sub(r'0x([0-9a-f]{8})', sub, text)
        self.out.append('Z %x %x %s\n' % (level, line, esc(text.encode('latin1'))))
        return 0

    def mkdir(self, c):
        self.out.append('C make_dir %s -> %d\n' % (esc(self.cstr(c.arg(0))), self.mkdir_errno))
        self.calls += 1
        self.cpu.u32(self.errno_cell, self.mkdir_errno)
        return 0 if self.mkdir_errno == 0 else 0xffffffff

    def delete_file(self, c):
        self.out.append('C delete_file %s -> %d\n' % (esc(self.cstr(c.arg(0))), 1 if 'delete_file' in self.fail else 0))
        self.calls += 1
        return 1 if 'delete_file' in self.fail else 0

    # files, through the same mounts as zutl::find
    def resolve(self, name):
        h = name.decode('latin1').replace('\\', '/')
        if not h:
            return None
        if h.startswith('/'):
            return h if os.path.isfile(h) else None
        for m in self.mounts:
            p = m + h if m.endswith('/') else m + '/' + h
            if os.path.isfile(p):
                return p
        return h if os.path.isfile(h) else None

    def fopen(self, name, mode):
        p = self.resolve(name)
        if not p:
            return 0
        f = self.calloc(0x20)
        self.files[f] = [Path(p).read_bytes(), 0, b'b' not in mode]
        return f

    def search_open(self, c):
        name, mode = self.cstr(c.edx), self.cstr(c.arg(0))
        base = re.split(rb'[\\/:]', name)[-1]
        for d in self.cstr(self.u(self.this + 0x74)).split(b';'):
            if d:
                p = d + (b'' if d.endswith(b'\\') else b'\\') + base
                if self.resolve(p):
                    return self.fopen(p, mode)
        return self.fopen(name, mode)

    def flag(self, f, bit):
        self.cpu.u32(f + 0xc, self.u(f + 0xc) | bit)

    def fread(self, c):
        buf, size, n, f = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        data, pos, _ = self.files[f]
        want = size * n
        chunk = data[pos:pos + want]
        self.files[f][1] = pos + len(chunk)
        c.write(buf, chunk)
        if len(chunk) < want:
            self.flag(f, 0x10)
        return len(chunk) // size if size else 0

    def fseek(self, c):
        f = c.arg(0)
        self.files[f][1] = c.arg(1)
        self.cpu.u32(f + 0xc, self.u(f + 0xc) & ~0x10)
        return 0

    def fgetc(self, c):
        f = c.arg(0)
        data, pos, text = self.files[f]
        if self.u(f + 0xc) & 0x10 or pos >= len(data):
            self.flag(f, 0x10)
            return 0xffffffff
        ch = data[pos]
        self.files[f][1] = pos + 1
        if text and ch == 0x1a:
            self.flag(f, 0x10)
            return 0xffffffff
        if text and ch == 0x0d and pos + 1 < len(data) and data[pos + 1] == 0x0a:
            self.files[f][1] = pos + 2
            return 0x0a
        return ch

    def stat(self, c):
        p = self.resolve(self.cstr(c.arg(0)))
        if not p:
            return 0xffffffff
        self.cpu.u32(c.arg(1) + 0x1c, int(os.stat(p).st_mtime) & 0xffffffff)
        return 0

    # fakes
    def new_model(self):
        m = self.calloc(0x60)
        self.ids[m] = 'M%d' % len(self.models)
        self.models.append(m)
        return m

    def new_node(self, name, cls):
        n = self.calloc(0xd4)
        if name:
            self.cpu.write(n, name[:0x23])
            if name.startswith(b'cls'):
                cls = msvc_atoi(name[3:])
        self.cpu.u32(n + 0x34, cls)
        if not name or b'nodata' not in name:
            self.cpu.u32(n + 0x38, self.calloc(0x200))
        self.ids[n] = 'N%d' % len(self.nodes)
        self.nodes.append(n)
        if not name or b'nomodel' not in name:
            self.cpu.u32(n + 0x3c, self.new_model())
        return n

    def find_fake(self, name):
        if name is None or b'missing' in name:
            return 0
        if name not in self.by_name:
            self.by_name[name] = self.new_node(name, 0)
        return self.by_name[name]

    def id(self, p):
        if p == 0:
            return '0'
        if p in self.ids:
            return self.ids[p]
        if self.world:
            bs = self.i(NODE_BLOCK_SIZE)
            for b in range(self.i(NODE_NBLOCKS)):
                base = self.u(self.u(NODE_BLOCKS) + 4 * b)
                if base <= p < base + bs * 0xd4:
                    return 'P%d' % (b * bs + (p - base) // 0xd4)
            pool = self.u(MODEL_POOL)
            if pool <= p < pool + 0x60 * self.i(MODEL_SIZE):
                return 'MP%d' % ((p - pool) // 0x60)
        return '?'

    def build_off(self, p):
        b = self.u(self.this + 0xa4)
        return 'B+%x' % (p - b) if b <= p < b + 0x1d8 else None

    def engine(self, c, name, regs, stack, ret):
        vals = [c.ecx, c.edx][:len(regs)] + [c.arg(k) for k in range(len(stack))]
        line, name_arg, outs = 'C ' + name, None, 0
        for kind, v in zip(regs + stack, vals):
            if kind in 'nmt':
                line += ' ' + self.id(v)
            elif kind == 's':
                s = self.cstr(v)
                line += ' ' + esc(s)
                if name_arg is None:
                    name_arg = s
            elif kind == 'f':
                line += ' f:%08x' % v
            elif kind == 'i':
                line += ' %d' % struct.unpack('<i', struct.pack('<I', v))[0]
            elif kind == 'o':
                if self.build_off(v):
                    line += ' ' + self.build_off(v)
                elif v == 0:
                    line += ' null'
                else:
                    outs += 1
                    self.cpu.write(v, struct.pack('<f', self.calls + 0.25 * outs))
                    line += ' o'
            elif kind == 'v':
                if self.build_off(v):
                    line += ' ' + self.build_off(v)
                else:
                    line += ' v:%08x,%08x,%08x' % struct.unpack('<III', self.cpu.read(v, 12))
            elif kind == 'B':
                line += ' ' + (self.build_off(v) or ('0' if v == 0 else 'local'))
            elif kind == 'k':
                line += ' ' + (self.build_off(v) or 'local')
        self.calls += 1
        r = 0
        if ret == 'i':
            r = SPECIAL_INT.get(name, 0)
            if name in self.fail:
                r = 1
            line += ' -> %d' % r
        elif ret == 'h':
            r = 0x1234
            line += ' -> %d' % r
        elif ret == 'm':
            r = self.new_model()
            line += ' -> ' + self.id(r)
        elif ret == 't':
            if name_arg is None or b'missing' not in name_arg:
                r = self.calloc(4)
                self.ids[r] = 'T%d' % self.textures
                self.textures += 1
            line += ' -> ' + self.id(r)
        elif ret == 'N':
            r = self.find_fake(name_arg)
            line += ' -> ' + self.id(r)
        elif ret == 'W':
            line += ' -> 0'
        elif ret in CLASS_OF:
            r = self.new_node(None, CLASS_OF[ret])
            line += ' -> ' + self.id(r)
        self.out.append(line + '\n')
        return r

    def set_name(self, c):
        n, s = c.ecx, self.cstr(c.edx)
        self.out.append('C set_name %s %s -> 0\n' % (self.id(n), esc(s)))
        self.calls += 1
        if n and s is not None:
            self.cpu.write(n, s[:34] + b'\0')
        return 0

    def get_model(self, c):
        n, out = c.ecx, c.edx
        r = 0
        if not n:
            r = 5
        elif out:
            self.cpu.u32(out, self.u(n + 0x3c))
        self.out.append('C get_model %s %s -> %d\n' % (self.id(n), 'O' if out else 'null', r))
        self.calls += 1
        return r

    def set_action(self, c):
        n, a = c.ecx, c.edx
        self.out.append('C set_action %s a:%s -> 0\n' % (self.id(n), ACTIONS.get(a, '?')))
        self.calls += 1
        if n:
            self.cpu.u32(n + 0x48, a)
        return 0

    def on_bfe(self, uc, address, size, user):
        self.out.append('C get_bfe_tolerance -> f:%08x\n' % fbits(0.0125))
        self.calls += 1

    def on_line(self, uc, address, size, user):
        t = self.this
        argc = self.u(t + 8)
        self.out.append('L %d' % argc + ''.join(' ' + esc(self.cstr(self.u(t + 0x20 + 4 * k))) for k in range(argc)) +
                        '\n')

    def on_done(self, uc, address, size, user):
        t = self.this
        r = self.cpu.read(t, 0xc0)
        go = 1 if r[0x18] == 0 else 0
        self.out.append('X %d m%d e%d q%d d%d %s\n' % (go, self.i(t + 0x14), r[0x10], r[0x18], self.u(t + 0xb4),
                                                     self.id(self.u(t + 0xb8))))

    # the run
    def call_al(self, addr, ecx, args):
        self.cpu.call(addr, ecx=ecx, stack_args=args, count=2_000_000_000)
        return self.cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff

    def run(self, config):
        t = self.this
        for raw in config.splitlines():
            w = raw.split()
            if not w:
                continue
            c = w[0]
            if c in ('mount', 'world'):
                pass  # given to the constructor
            elif c == 'archive':
                self.cpu.write(USE_ARCHIVE, bytes([int(w[1])]))
            elif c == 'hardware':
                self.cpu.u32(HARDWARE, int(w[1]))
            elif c == 'console':
                self.cpu.write(t + 4, bytes([int(w[1])]))
            elif c == 'macro':
                self.cpu.call(MACRO_SET, ecx=t, stack_args=(self.string(w[1].encode()),
                                                            self.string((w[2] if len(w) > 2 else '').encode())))
            elif c == 'fail':
                self.fail.add(w[1])
            elif c == 'mkdir_errno':
                self.mkdir_errno = int(w[1])
            elif c == 'current':
                self.cpu.u32(t + 0xb8, self.find_fake(w[1].encode()))
            elif c == 'mech':
                mech = self.calloc(8)
                self.cpu.u32(mech + 4, self.find_fake(w[1].encode()))
                self.out.append('R %d\n' % self.call_al(SET_MECH, t, (mech,)))
            elif c == 'source':
                self.out.append('R %d\n' % self.call_al(SOURCE, t, (self.string(w[1].encode()),)))
            elif c == 'exec':
                text = raw[raw.find('exec') + 5:].encode('latin1') + b'\n'
                self.cpu.write(LINEBUF, bytes(0x400))
                self.cpu.write(LINEBUF, text)
                self.out.append('R %d\n' % self.call_al(EXEC_LINE, t, (LINEBUF,)))
            elif c == 'touch':
                p = self.resolve(w[1].encode())
                if p:
                    os.utime(p, (int(w[2]), int(w[2])))
            elif c == 'scrollclear':
                self.cpu.call(SCROLL_CLEAR, ecx=SCROLL, stack_args=(int(w[1]),))
            elif c == 'scrolltick':
                for n in self.nodes:
                    if self.u(n + 0x48) == SCROLL_TICK:
                        self.cpu.call(SCROLL_TICK, ecx=n)
                holder = self.u(SCROLL + 0xc)
                if holder:
                    self.cpu.call(ALWAYS_TICK, ecx=holder)
        self.dump()
        return ''.join(self.out)

    def list_items(self, head):
        out, p = [], self.u(head)
        while p != head:
            out.append(self.u(p + 8))
            p = self.u(p)
        return out

    def dump(self):
        t, o = self.this, self.out
        r = self.cpu.read(t, 0xc0)
        mac, nm = self.u(t + 0x60), self.u(t + 0x64)
        o.append('S macros' + ''.join(' %s=%s' % (esc(self.cstr(self.u(mac + 8 * k))), esc(self.cstr(self.u(mac + 8 * k + 4))))
                                      for k in range(nm)) + '\n')
        var, nv = self.u(t + 0x68), self.u(t + 0x6c)
        o.append('S vars' + ''.join(' %s:%d:%08x' % (self.cstr(self.u(var + 12 * k)).decode(), self.u(var + 12 * k + 4),
                                                       self.u(self.u(var + 12 * k + 8))) for k in range(nv)) + '\n')
        o.append('S depth %d includes %d file %d binary %d loose %d current %s quit %d error %d\n' % (
            self.u(t + 0xb4), self.u(t + 0xa0), 1 if self.u(t + 0x98) else 0, r[0x94], self.i(t + 0xb0),
            self.id(self.u(t + 0xb8)), r[0x18], r[0x10]))
        o.append('S globals %s %s %s verbose %d persp %d\n' % (*(self.id(self.u(a)) for a in MODELS),
                                                              self.i(VERBOSE), self.i(PERSP)))
        o.append('S mw3 %08x %08x %08x ceiling %08x mech %s\n' % (self.u(GRAVITY), self.u(INV_GRAVITY), self.u(DAMAGE),
                                                                self.u(CEILING), 'set' if self.u(t + 0xbc) else '0'))
        b = self.u(t + 0xa4)
        raw = bytearray(self.cpu.read(b, 0x1d8))
        tex, model = self.u(b + 0x10), self.u(b + 0x1d4)
        raw[0x10:0x14] = raw[0x1d4:0x1d8] = bytes(4)
        o.append('S build %s %s' % (self.id(tex), self.id(model)) +
                 ''.join((' ' if k % 4 == 0 else '') + '%02x' % raw[k] for k in range(0x1d8)) + '\n')
        o.append('S scroll %s' % self.id(self.u(SCROLL + 0xc)) +
                 ''.join(' ' + self.id(n) for n in self.list_items(self.u(SCROLL + 4))) + '\n')
        o.append('S clutter' + ''.join(' ' + esc(self.cstr(s)) for s in self.list_items(self.u(CLUTTER + 4))) + '\n')
        for n in self.nodes:
            d = self.u(n + 0x38)
            data = ''.join((' ' if k % 4 == 0 else '') + '%02x' % x for k, x in enumerate(self.cpu.read(d, 0x50))) if d else ''
            o.append('S %s %s cls %d data%s model %s action %s\n' % (
                self.id(n), esc(self.cstr(n)), self.i(n + 0x34), data, self.id(self.u(n + 0x3c)),
                ACTIONS.get(self.u(n + 0x48), '?')))
        for m in self.models:
            o.append('S %s' % self.id(m) + ''.join((' ' if k % 4 == 0 else '') + '%02x' % x
                                                   for k, x in enumerate(self.cpu.read(m, 0x5c))) + '\n')
        if self.world:
            o.append('S world live %d capacity %d anim %s\n' % (self.i(NODE_LIVE), self.i(NODE_CAP),
                                                                esc(self.cstr(ANIM_PATH))))
            h = self.u(SCROLL + 0xc)
            if h:
                o.append('S holder %s %s cls %d flags %08x\n' % (self.id(h), esc(self.cstr(h)), self.i(h + 0x34),
                                                                 self.u(h + 0x24)))


def port(config):
    buf = ctypes.create_string_buffer(64 << 20)
    n = probe.probe_interp(config.encode('latin1'), buf, len(buf))
    return buf.raw[:n].decode('latin1')


def original(config):
    mounts = [w.split(None, 1)[1] for w in config.splitlines() if w.startswith('mount ')]
    world = next((w.split(None, 1)[1] for w in config.splitlines() if w.startswith('world ')), None)
    emu = Emulated(mounts, world)
    try:
        return emu.run(config)
    except Exception:
        print(''.join(emu.out[-8:]))
        raise


def compare(what, config):
    want, got = original(config), port(config)
    if want != got:
        a, b = want.splitlines(), got.splitlines()
        k = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
        ctx = '\n'.join('    ' + x for x in a[max(0, k - 6):k])
        raise SystemExit(f'{what}: traces differ at line {k + 1}\n{ctx}\n  original: {a[k] if k < len(a) else "<end>"}\n'
                         f'  port:     {b[k] if k < len(b) else "<end>"}')
    return want


def archive_names(path):
    d = path.read_bytes()
    n = struct.unpack_from('<I', d, 8)[0]
    return [d[12 + 0x80 * k:12 + 0x80 * k + 0x78].split(b'\0')[0].decode() for k in range(n)]


def main():
    zbd = GAME / 'zbd' / 'interp.zbd'
    names = archive_names(zbd)
    game = f'mount {GAME}\n'
    runs = lines = calls = 0
    seen = set()

    def check(what, config):
        nonlocal runs, lines, calls
        tr = compare(what, config)
        seen.update(re.findall(r'^C (\w+)', tr, re.M))
        runs += 1
        lines += tr.count('\nL ') + tr.startswith('L ')
        calls += tr.count('\nC ')
        return tr

    # Every mission script as the mission load runs it (0x48faf0).
    for gs in [n for n in names if n.endswith('.gs')]:
        m = gs[:-3]
        for hw in (1, 0):
            check(f'{gs} hardware {hw}', game + 'macro USEZBD true\n' + ('macro HARDWARE true\n' if hw else '') +
                  f'source support\\{m}size.gw\nsource {gs}\n')
        check(f'{gs} gamez fails', game + 'macro USEZBD true\nfail read_zbd\n' + f'source {gs}\n')
    # Every include on its own, with and without the mission macros.
    for gw in [n for n in names if n.endswith('.gw')]:
        check(gw, game + f'source {gw}\n')
        check(gw + ' (macros)', game + 'macro DB c1\nmacro USEZBD true\nmacro HARDWARE true\nmacro COMPILE true\n'
              'macro worldName world1\n' + f'source {gw}\n')
    for k in 'ABC':
        check(f'cpit_cycle{k}', game + f'current cpit_ani{k.lower()}\nsource support\\cpit_cycle{k}.gw\n')
    print(f'ok: {runs} runs of the shipped scripts identical ({lines} lines, {calls} engine calls)')
    # The missions again against their own worlds, the ported functions run
    # for real in both (lookups on the real node lists, the ScrollAlways
    # holder made in the pool, the anim path).
    before = runs
    for m in ('c1', 'c2', 'c3', 'c4', 'c4b', 't1'):
        check(f'{m}.gs on its world', f'world {GAME / "zbd" / m / "gamez.zbd"}\n' + game +
              f'macro USEZBD true\nmacro HARDWARE true\nsource support\\{m}size.gw\nsource {m}.gs\n')
        for k in 'ABC':
            check(f'cpit_cycle{k} on {m}', f'world {GAME / "zbd" / m / "gamez.zbd"}\n' + game +
                  f'current cpit_ani{k.lower()}\nsource support\\cpit_cycle{k}.gw\n')
    print(f'ok: {runs - before} runs on the six worlds identical, the ported functions run for real')
    crafted(check)
    names = [e[0] for e in ENGINE] + ['set_name', 'get_model', 'set_action', 'get_bfe_tolerance', 'make_dir',
                                      'delete_file']
    missing = [n for n in names if n not in seen]
    print(f'ok: {runs} runs in all identical ({lines} lines, {calls} engine calls; '
          f'{len(names) - len(missing)} of {len(names)} engine functions reached)' +
          (f', never reached: {" ".join(missing)}' if missing else ''))


def crafted(check):
    if WORK.exists():
        shutil.rmtree(WORK)
    WORK.mkdir(parents=True)
    zbd = GAME / 'zbd' / 'interp.zbd'
    loose = f'mount {WORK}\n'
    both = loose + f'mount {GAME}\n'
    for name, text in CRAFTED.items():
        p = WORK / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(text if isinstance(text, bytes) else text.encode('latin1'))
    for name in CRAFTED:
        if name.endswith('.gs'):
            check(name, loose + 'archive 0\nhardware 1\n' + f'source {name}\n')
    # The archive: a loose copy with the archived time keeps it, another time
    # rejects it (every script then loose), and a newer one wins alone.
    d = zbd.read_bytes()
    ents = {archive_names(zbd)[k]: struct.unpack_from('<II', d, 12 + 0x80 * k + 0x78) for k in range(len(archive_names(zbd)))}
    stale = WORK / 'Support' / 'tolerances.gw'
    stale.parent.mkdir(exist_ok=True)
    stale.write_bytes(b'SetBFETolerance 0.25\nSetCoplanarTolerance 0.5\n')
    ts = ents['Support\\tolerances.gw'][0]
    touch = f'touch Support\\tolerances.gw {{}}\n'
    check('loose copy, same time', both + touch.format(ts) + 'macro USEZBD true\nsource c1.gs\n')
    check('loose copy, older: archive rejected', both + touch.format(ts - 100) +
          'macro USEZBD true\nsource c1.gs\nsource c2.gs\n')
    check('loose copy, newer: archive rejected', both + touch.format(ts + 100) + 'macro USEZBD true\nsource c1.gs\n')
    # Newer than the archive's entry after the archive is open: that loose
    # copy wins alone (the live-edit path); an older one does not.
    check('archive open, newer loose copy', both + touch.format(ts) + 'macro USEZBD true\nsource support\\common.gw\n' +
          touch.format(ts + 100) + 'source support\\tolerances.gw\nsource c1.gs\n' + touch.format(ts - 100) +
          'source support\\tolerances.gw\n')
    os.remove(stale)
    # Damaged archives.
    for what, data in (('bad magic', b'\x18\x11\x97\x08' + d[4:]), ('bad version', d[:4] + b'\x08' + d[5:]),
                       ('short header', d[:6]), ('short directory', d[:12 + 0x80 * 3]),
                       ('empty', d[:8] + b'\0\0\0\0')):
        (WORK / 'interp.zbd').write_bytes(data)
        check(f'archive: {what}', both + 'macro USEZBD true\nsource support\\tolerances.gw\nsource c1.gs\n')
    os.remove(WORK / 'interp.zbd')
    # Single lines (the console's way in) and the scroll list.
    check('lines', loose + 'archive 0\n' + ''.join(f'exec {x}\n' for x in LINES))
    check('console', loose + 'archive 0\nconsole 1\n' + ''.join(f'exec {x}\n' for x in LINES[:12]))
    check('scroll list', loose + 'archive 0\nsource scroll.gs\nscrolltick\nscrollclear 0\nscrolltick\nscrollclear 1\n'
          'source scroll.gs\nscrolltick\n')
    check('mech', loose + 'archive 0\nmech player\nexec PrintTree\nexec NodeDump\nmech cls1_cam\nexec NodeDump\n')
    check('mkdir errors', loose + 'archive 0\nmkdir_errno 17\nexec mkdir a\nmkdir_errno 2\nexec mkdir b\n'
          'mkdir_errno 0\nexec mkdir c\n')
    check('fails', loose + 'archive 0\nfail free_node\nfail cycle_set_on\nfail delete_file\nfail free_world\n'
          'source commands.gs\n')


# Single lines through exec_line (as the debug console would send them).
LINES = [
    'set A 1', 'set B %A%2', 'Echo %A% %B% x', 'echo a', 'var DamageScale = 0.5', 'var gravity set 9.81',
    'var ceiling 4', 'var nosuch = 1', 'who', 'WorldSetGravity 12.5', 'WorldUnknown 1',
    'Unknown 1', 'Imaginary', 'SEQUnknown', 'settle x y', 'sourcery', 'Quit', 'FindNode world1',
    'GetBFETolerance', 'CountCameraNodes', 'CountUsedNodes', 'PrintUsedNodes', 'PrintNodeCount world',
    'PrintTree', 'NodeDump', 'CameraGetTranslate', 'Object3DGetTranslate', 'WorldGetFogColor', 'WorldGetFogRange',
    'FindNode cls2_world', 'WorldGetFogColor', 'WorldGetFogRange', 'WorldSetFogRangeNear 5',
    'WorldSetFogRangeFar 6', 'zap', '~x', '9lives', '',
]

CRAFTED = {
    # Every command once, with its arguments; failures and odd values too.
    'commands.gs': r'''# every command the dispatcher knows
AddClutterTemplates clutter1
AddClutterTemplates a b
AddEnhancerImage enh.tif
AnimSetZBDFile zbd\c1\anim.zbd
AnimSetDebugFrame
FindNode cls5_obj
AddChild child1
AddChild missing1
CameraSetRotate 1.5 -2.25 3
CameraSetRot 1 2 3
FindNode cls1_camera
CameraGetTranslate
CameraSetActive on
CameraSetActive OFF
CameraSetActive TRUE
CameraSetDynamicLOD 250
CameraSetFOV 60 45.5
CameraSetFOVwide 90 70
CameraSetFOV 33.3 47.7
CameraSetFOV 0.3 0.6
CameraSetHorizonXZ hor1
CameraSetHorizon hor2
CameraSetLODMultiplier 1.5
CameraSetNearFarClip 0.5 1000
CameraSetNearClip 2
CameraSetFarClip 3000
CameraSetNearClipX 2
CameraSetObjectHSETest on
CameraSetWindow window1
CameraSetWorld world1
CameraSetTranslate 1 2 3
CountCameraNodes
CountUsedNodes
FindNode cls5_cycler
CycleTextureSetOn 1
CycleTextureSetLooping on
CycleTextureSetMap tex01
CycleTextureSetMap missingtex
CycleTextureSetSpeed 0.25
FindNode cls5_nomodel_x
CycleTextureSetOn 2
ClearScreenBuffer on
FindNode cls5_parent
DeleteChild sub1
DeleteChild missingsub
DeleteFile junk.txt
DeleteTree tree1
DeleteTree missingtree
DirectedLightFluctuate on
DirectedLightFluctuate on extra
DirectedLightFluctuatePeriod 4
DirectedLightFluctuateWavelength 8.5
DirectedLightFluctuateBaseAndAmp 0.25 0.5
DirectedLightFluctuateBaseAndAmp 0.25
DirectedLightFluctuateHeading 45
DirectedLightFluctuateHeading 33.3
DirectedLightFluctuateHeading 0.9
FindNode cls4_display
DisplayOrigin 10 20
DisplayResolution 640 480
DisplaySetClearColor 0.1 0.2 0.3
FindSubNode sub2
FindSubNode missingsub
FindNode missingnode
FogLargeObjects on
FreeNode cls1_freecam
FreeNode cls2_freeworld
FreeNode cls3_freewin
FreeNode cls4_freedisp
FreeNode cls5_freeobj
FreeNode cls8_freeodd
GameZReadZBDFile zbd\c9\gamez.zbd
GameZWriteZBDFile out.zbd
GetBFETolerance
GlobalWindRandomMaxSpeed 12
GlobalWindRandomMaxSpeed 12 13
GlobalWindRandomAccel 1.5
GlobalWindRandomAngVel 30
GlobalWindRandomAngVel 33.3
GlobalWindRandomAngVel 1.3
GlobalWindStaticVelocity 1 2 3
GlobalWindStaticVelocity 1 2
LensFlareTexture 2 flare.tif
LightNew sun2
LightSetActive on
LightSetAmbient 0.4
LightSetColor 1 0.5 0.25
LightSetDiffuse 0.75
LightSetDirectedSource
LightSetDirectional on
LightSetOrientation -20.0 45. 0.
LightSetOrientation 33.3 -17.7 123.456
LightSetOrientation 0.3 0.6 0.9
LightSetPointSource
LightSetRanges 32 750
LightSetSaturated off
LightSetStatic on
LightSetSubdivide on
LightSetTranslate 1 2 3
LoadSoils soils.zrd
FindNode cls6_lod
LODAddChild lodkid
LODSetRange 10.5 20.25
MakeShadows
MatlNew
MatlFaceColor 0.5 0.25 1
MatlTexture tex02
MatlTexture missingtex
ModelNew model1 Facade
ModelNew model2 Plain
mModelDoShadows on
ModelDoShadows on
ModelDoShadows on extra
ModelPolygonBegin
ModelPolygonVertex 1 2 3
ModelPolygonVertex 4 5 6
ModelPolygonVertex 7 8 9
ModelPolygonUV 0.5 0.25
ModelPolygonUV 1 0
ModelPolygonEnd
NewCamera cam2
NewDisplay disp2
NewLOD lod2
NewNode node2
NewObject3D obj2
NewSEQ seq2
NewWindow win2
NewWorld world2
NodeDump
NodeSetActive on
NodeSetDescription described
NodeSetCanModify on
NodeSetLighting on
NodeSetOverwrite on
FindNode cls5_o3d
Object3DAddChild o3dkid
Object3DAddChild missingkid
Object3DGetTranslate
Object3DRegisterTexturesToWorld on
Object3DRegisterTexturesToWorld off
Object3DRotate 90 180 -45.5
Object3DRotate 10.1 20.2 30.3
Object3DRotate 1.2 1.3 1.8
Object3DScale 1 2 3
Object3DSetActionPriority 3
Object3DSetActive on
Object3DSetColor 7
Object3DSetFacade
Object3DSetOpacityIsSet 1
Object3DSetOpacity 0.5
Object3DSetPoints
Object3DSetPriority 5
Object3DSetShowBackFace on
Object3DSetTextureWorldBaseCoordinates 1 2
Object3DSetTextureWorldTexturesPerMeter 3 4
Object3DSetTextureWorldTexturesPerMet 5 6
Object3DSetMorphVertex 0.5
Object3DTranslate 1 2 3
PerspectiveTexture on
PrintNodeCount world
PrintTree
PrintUsedNodes
RdrAddPath a;b
RdrSetPath c;d
SEQNew seq3
SEQAddChild seqkid 4 0.5
SEQSetActive 1
SEQSetLoop 2
SEQSetPause 3
SEQSetRepeat 4
SEQSetPauseX 5
SetAltitudeSurface on
SetBFETolerance 0.001
SetBFETolerance
SetCoplanarTolerance 0.002
SetColinearTolerance 0.003
SetGameZNodeArraySize 100
SetMaterialArraySize 200
SetModel3DArraySize 300
SetIntersectBBOX on
SetIntersectSurface off
SetLandmark on
SetPaletteName pal
SetPaletteShading on
SetPerspectiveAdaptiveCorrection 1 2 3.5
SetPerspectiveTextureDeltaX 32
SetInverseZTolerance 0.01
SetPerspectiveInverseZTolerance 0.02
SetPerspectiveTextureFarZ 10000
SetProximity on
SetSmallPolygonRejectArea 1.5
SetTextureDirectory a;b
SetVertexShading on
TextureAdd tex03
Verbose on
VideoSetDither on
VideoSetWireFrame on
VideoSetDitherX on
FindNode cls3_window
WindowAddClearPolygonVertex 1 2 3
WindowAddClearPolygonVertex 1 2
WindowBuffer 2
WindowCloseClearPolygon
WindowOrigin 5 6
WindowResolution 320 200
WindowSetClearPolygon on
FindNode cls2_world
WindowBuffer 2
WorldAddLight light1
WorldExtents 4864.0 -5632.0
WorldOrigin -3840.0 2816.0
WorldPartitionInclusionTolerance 3.0 3.0
WorldPartitionMaxDECFeatureCount 9
WorldPartition 256 -256
WorldSetFogAltitude 10 20
WorldSetFogAltitude 10
WorldSetFogColor 0.5 0.6 0.7
WorldGetFogColor
WorldSetFogDensity 0.01
WorldSetFogRange 100 1000
WorldSetFogRangeNear 50
WorldSetFogRangeFar 2000
WorldGetFogRange
WorldSetFogState linear
WorldSetFogState exponential
WorldSetFogState off
WorldSetFogState foggy
WorldSetFogState
WorldSetVirtualPartitionMapSize 8 9
WorldSetVirtualPartition on
WriteTextureSetType 1
WriteTextureSetMap tex04
FindNode cls5_obj
WorldSetFogAltitude 1 2
FindNode missingagain
CycleTextureSetOn 1
LightSetSaturated on
Object3DSetScroll on 1 2
PrintTree
Object3DSetScrllAlways on 1 2
''',
    # Macros, the preprocessor and the tokenizer.
    'macros.gs': '# macros\r\nset A alpha\r\nset B %A%-%A%\nset C %A%%B%\nset D pre%nosuch%post\n'
                 'set E 100%\nset F %A\nset G a%A%b%B%c%\nset %A% indirect\nset %A% %B%\n'
                 'set H\nset\nTextureAdd %A%\nTextureAdd %%\nTextureAdd %alpha%\n'
                 'LensFlareTexture %G% %B%\nLightSetRanges %E% %F%\n'
                 'set N 7\nSEQAddChild %A% %N% %N%\nset true true\nifdef true\nTextureAdd yes1\nendif\n'
                 'ifdef A\nTextureAdd no1\nendif\nset T true\nset U true\nset V false\n'
                 'ifdef T\nTextureAdd yes2\nendif\nifndef T\nTextureAdd no2\nendif\n'
                 'ifdef V || T\nTextureAdd yes3\nendif\nifdef V && T\nTextureAdd no3\nendif\n'
                 'ifdef T || V && V\nTextureAdd no4\nendif\nifdef V && V || T\nTextureAdd yes4\nendif\n'
                 'ifdef T junk V\nTextureAdd yes5\nendif\nifdef V ||\nTextureAdd no5\nendif\nifdef\nTextureAdd no6\nendif\n'
                 'ifdef V\nifdef T\nTextureAdd no7\nendif\nTextureAdd yes6-quirk\nendif\nTextureAdd after\n'
                 'ifdefined T\nTextureAdd yes7\nendif\nendif\nendif\nendifx\nTextureAdd extra\n'
                 'ifndef nosuch\nTextureAdd yes8\nendif\n'
                 'mkdir somedir\nwho\nvar gravity = 20\nvar ceiling set 300.5\nvar DamageScale bad 1\n'
                 'var DamageScale == 0.25\nvar nosuch = 1\n'
                 'TextureAdd a,b c\t d  ,  e\nTextureAdd ,lead\nTextureAdd x , y\n'
                 'TextureAdd hash#comment\nTextureAdd \x80\xff\xa0hi\n  \t  \n#only a comment\n\n'
                 'TextureAdd 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15\n'
                 'LightSetRanges 1e2 2.5d1\nLightSetRanges -.5 +3.\nLightSetRanges 0x10 abc\nLightSetRanges 1e 1e+\n'
                 'SetPerspectiveTextureDeltaX -12abc\nSetPerspectiveTextureDeltaX 4294967297\n'
                 'Object3DSetActionPriority  +7\n'
                 'source sub.gw\nTextureAdd back\nsource missingfile.gw\nTextureAdd back2\n'
                 'source subquit.gw\nTextureAdd back3\nsource %SUBNAME%\nset SUBNAME sub.gw\nsource %SUBNAME%\n'
                 'Quit\nTextureAdd never\n',
    'sub.gw': 'TextureAdd insub\nset FROMSUB yes\nFindNode missingx\n',
    'subquit.gw': 'TextureAdd beforequit\nQuit\nTextureAdd afterquit\n',
    # Ctrl-Z ends a loose file; CR alone stays; a NUL ends a line.
    'ctrlz.gs': b'TextureAdd one\r\nTextureAdd two\rthree\r\nTextureAdd fo\x00ur\nTextureAdd five\x1aTextureAdd six\n',
    'noeol.gs': 'TextureAdd last-line-no-newline',
    'empty.gs': '',
    # GameZReadZBDFile failing as a sourced file's unterminated last line.
    'gzfail.gs': 'ifndef USEZBD\nTextureAdd before\nendif\nsource gzlast.gw\nifndef USEZBD\nTextureAdd fallback\nendif\n'
                 'set USEZBD true\nGameZReadZBDFile x.zbd\nifndef USEZBD\nTextureAdd fallback2\nendif\n',
    'gzlast.gw': 'set USEZBD true\nGameZReadZBDFile bad.zbd',
    'scroll.gs': 'FindNode obj1\nObject3DSetScroll on 0.5 0.25\nFindNode obj2\nObject3DSetScrollAlways on 1 2\n'
                 'FindNode obj3\nObject3DSetScrollAlways on 3 4\nFindNode cls5_nomodel\n'
                 'Object3DSetScrollAlways on 5 6\nObject3DSetScroll on 7 8\nFindNode missingobj\n'
                 'Object3DSetScroll on 9 10\nObject3DSetScrollAlways on 11 12\nObject3DSetScroll off 0 0\n',
}

if __name__ == '__main__':
    main()
