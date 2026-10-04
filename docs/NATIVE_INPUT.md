# Native 3DS Circle Pad and touch adapter

The adapter provides a dynamic lower-screen stick, tap-to-confirm and an eight-way raw Circle Pad policy. This is an intentional 3DS input
adaptation, not the original Godot analog policy. Original movement still receives
integer axes in {-1,0,1}; diagonal vectors are never normalized and movement,
running, collision and animation rules are unchanged. No touch run/B button was
invented. Existing lower-screen development information is retained.

## Checked external tuning

`content/native-input.json` compiles with `python3 tools/native_input.py compile`
to `romfs/data/native.encinput`. `verify` rejects a stale binary. The independent
88-byte little-endian ENCINP01 resource has version, exact length, payload length,
reserved-byte and CRC32 checks. Runtime and compiler both validate all fields,
finite floats, relationships, screen geometry and translucent overlay colors.
Unknown fields/versions, duplicate JSON keys and bad binary32 rounding fail closed.
There is no compiled tuning fallback or game-content table.

Current tuning: Circle Pad enter/release 24/18 raw counts, angular hysteresis 4°,
touch enter/release 8/6 pixels, tap travel below 8 pixels, tap duration below 350ms,
base/thumb radius 32/7 pixels. The two overlay colors and opacity are also data.
Changing these values requires recompiling the resource, not the executable.

The SDK exposes `circlePosition` as signed 16-bit dx/dy. libctru reads the HID
sample without specifying calibration or manufacturing the direction bits.
The devkitPro SDL port uses nominal ±156 and negates dy for down-positive axes:

- https://github.com/devkitPro/libctru/blob/v2.7.0/libctru/include/3ds/services/hid.h
- https://github.com/devkitPro/libctru/blob/v2.7.0/libctru/source/services/hid.c
- https://github.com/devkitPro/pacman-packages/blob/master/3ds/SDL/SDL-1.2.15.patch

Accordingly, 156 is a documented nominal reference, not a measured physical-device
calibration guarantee. The classifier uses raw radial counts, and does not divide
by, clamp to, or require reaching 156. Physical-device calibration/feel is unverified.

## Sampling and ownership contract

Load `NativeInputData`, configure `NativeInputAdapter`, then call `sample` once
per physical HID poll, outside fixed-step catch-up. Pass unscaled, unclamped wall
time, actual D-pad booleans, raw Circle Pad coordinates (+Y up), and touch pixels
(+Y down). The result contains final direction, source device, independent touch
direction, one tap-confirm pulse, and an optional gesture display.

Each of the eight sectors spans 45°. Radial Schmitt thresholds and angular
hysteresis suppress chatter. Large turns jump directly to the nearest sector.
Exact positive boundary ties select the next clockwise sector in screen axes.

For walking, any actual D-pad button owns the direction first, including opposed
bits that cancel to zero. A latched touch drag owns it next, even after returning
to its center; raw Circle Pad owns it otherwise. Devices are not added together.
This deterministic priority is another explicit platform adaptation. Physical
menu HID direction interpretation stays unchanged. Touch directions use the same
existing 350ms/100ms repeat gate, which emits at most one step per physical frame.
Physical A and the touch confirm pulse must be ORed once at the platform boundary.

The first touch anywhere in 320×240 fixes that gesture's logical origin. Dragging
past the activation radius latches a drag until lift. Returning to center merely
stops direction output; it can never turn that drag back into a tap. Lift ends
the gesture. A short release below the travel threshold emits one confirmation.
A stationary long hold, invalid coordinates or timing, reset, or context change
suppresses that confirmation. New contact starts its timer at the sampled press,
without inheriting elapsed time before contact. No sub-frame events are fabricated.

The platform APT suspend/restore/sleep/wakeup hook resets both input adapters
and catch-up debt on the next main-loop iteration; held gestures must release.
This uses the actual lifecycle event rather than treating a slow frame as sleep.

Context zero disables input. Initial sampling, a context change or explicit reset
quarantines currently held controls until each physical device reaches neutral
or the finger lifts. The caller changes context for input-owner transitions,
pause/suspension, reset, dialogue/menu changes and other blocking transitions.
If ownership changes after sampling in the same frame, the caller must not
dispatch that old frame's gesture/confirm into the new owner. Direction results
may be reused for catch-up only while the same owner remains active.

`draw_native_input` draws only two small translucent circles on the lower target
after the existing debug UI. The base keeps the actual contact origin even at
screen edges. Only the displayed thumb is radially limited and clamped on screen;
input deltas and origin are never clamped. The upper game view is untouched.

## Verification

`tests/native_input_tests.cpp` performs angular sweeps at four radii, clockwise
and counterclockwise hysteresis sweeps, raw signed extremes, radial thresholds,
every direction, device opposition/priority, taps, drags, return-to-center,
long holds, all screen corners, invalid samples, reset/context transitions and
low-frame-rate single pulses/repeats. It mutates the actual binary with fresh CRCs
to prove malformed tuning is rejected, and changes valid activation data to
prove the same executable honors external tuning.

`tests/test_native_input.py` tests compilation/roundtrip, missing/unknown/duplicate
fields, booleans/nonnumeric/nonfinite values, every truncation, checksum/header
corruption, binary32 precision collapse and checked RomFS staging.

Focused host checks and ASan/UBSan pass; the native runtime also compiles with
the installed devkitARM compiler. LeakSanitizer is disabled under the executor's
documented ptrace restriction. Full build/integration evidence is recorded by
the current integration task. No new screenshot, GUI, emulator interaction or
physical 3DS touch/feel validation was performed for this adapter.
