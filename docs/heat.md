# Heat

Mech3.exe 1.2, found with REA + Ghidra (2026-10-18). Not ported yet: the update
reads mech components, sounds and the HUD that are still unmapped.

## Where

`0x004192f0`, a mech method called each frame with the frame step (`dt`,
0x0072c52c). It sits in the unnamed files between `mech_anim.cpp` and
`mech_data.cpp`. Found by following the `mech_heat_critical` sound: its handle is
stored in the mech sound table at 0x00605600 (entry 26 from 0x00605598), and
`0x0041e770` plays entry 26 from here.

## The model

Mech fields (offsets in the mech object):

| Offset | Meaning |
|---|---|
| `+0x554` | heat level, 0 to 2.0 |
| `+0x558` | heat added this frame (weapons and so on); cleared at the end of the update |
| `+0x55c` | dissipation rate this frame |
| `+0x69c` | flags: bits 0-2 warning-beep state, bits 3-5 shutdown state, bit 24/25 overheat flags |
| `+0xdd` | shutdown in progress |
| `+0xde` | boosted cooling (rate ×3), likely standing in water |

Each frame:

```
rate  = 1.0                                  0x005e1fd4
if (+0xde) rate *= 3
if (component0 flag +0xac):                  component0 = (mech+0x38c)[0] -> +0x78
    f = max(component0[+0xdc] / mech[+0x68], 1.0)
    rate *= min((3f + 1)^2, 8)               always 8: see below
rate *= 0.05                                 0x005e5d40
player only: rate *= (1.6 - |x|) when that is below 1; x from a vtable +0x1c call
             added += dt * 0.05 while +0x580 is set, and again while +0x574 != 0
added += 0.05 per frame while either of two components is in state 8
rate *= environment factor (0x0045fb90)
heat = clamp(heat - dt * rate, 0, 2.0)
heat = clamp(heat + added, 0, 2.0)
```

Original quirk, kept in the rebuild: `f` is the larger of the ratio and 1.0, so
`(3f + 1)^2` is at least 16 and the cap always applies; when the component flag
is set, dissipation is simply ×8. `min` would have given a 1-16 range, which may
have been the intent.

## Thresholds

| Heat | Effect |
|---|---|
| 0.7 | warning beep starts (sound 5); stops again below 0.7 (0x005e1fec, 0x005e1ff0) |
| 0.8 | player's screen effect, strength (heat - 0.8) × 5 (0x0043c1f0) |
| 0.95 | shutdown warning (sound 0x14), then shutdown voice (0x15) and power-down (0x0041cc30) |
| 0.9 | dropping below cancels a pending shutdown |
| 1.5 | overheated flag |
| 2.0 | critical: sound 26, forced shutdown path |
| 0.4 | below: recover from shutdown, clear flags |

The shutdown state machine draws `rand()` (MSVCRT; `crt::msvc_rand` in the
rebuild): at each check, a value under 0.8 of the range lets the mech ride it
out. When the byte at `+0x630` is set (likely a difficulty or realism option),
overheating instead deals damage (`0x0040fe70(2.0, 0x20c)`).

## To port

1. Name the mech fields above in the headless Ghidra project and re-decompile.
2. Identify component0 (`+0x38c` table), the vtable +0x1c value (throttle or
   speed), `0x0045fb90` (likely the map's temperature) and the two state-8
   components.
3. Port 0x004192f0 with its state machines; differential-test it in Unicorn by
   stubbing the calls it makes, as tests/differential/test_ztimer.py stubs
   GetTickCount.
