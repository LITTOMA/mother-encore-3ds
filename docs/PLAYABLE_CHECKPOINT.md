# Combined playable development checkpoint (2026-10-02)

This is a historical 2026-10-02 checkpoint. Subsequent naming/settings work is
covered by STARTUP_SETTINGS_CHECKPOINT.md; current support is listed in STATUS.md.


This checkpoint combines the reviewed optional Pillow/Minnie branch, the exact
read-only rules6-to7 LOAD bridge, and the opt-in GPU span/certificate renderer.
It is a development candidate, not a stable release or a full-game port.

## Scope and configuration

Pillow is optional before the Doll melody. Its original Mom-room doors, seven
attack phrases, real non-boss battle/reward, Minnie leave/door scripts and
Yes/No/Cancel running tutorial are data-driven. Room rules/capabilities are 7;
Round schema is 5. Lamp 3 → Pillow 8 → Doll 16 applies growth once at threshold 9.
See PILLOW_MINNIE.md for the source execution and compatibility scope.

LOAD accepts the exact reviewed rules6 identity only after historical and
current domain checks. Every snapshot field is preserved. No user save was
read, rewritten or migrated on disk in development. Record over an occupied
old-version slot is still refused before writing; choose an empty slot to
create a rules7 save. See SESSION_MIGRATION.md.

Both EXPERIMENTAL_GPU_BACKGROUND and EXPERIMENTAL_GPU_CERTIFICATES default to 0
in source. The requested development artifact is built explicitly with both 1.
A separate 0:0 CPU-compatible artifact is retained for fallback/comparison.
The mode stamp includes both values and forces the platform main object to
recompile when either changes. These are separate build configurations.

The certificate path adds 3,594,240 bytes at 400×240 (cap 3,774,873). Allocation or
budget failure retains the exact unaccelerated-proof GPU span path; unavailable
GPU resources, unsupported TIME or span-capacity failure retains CPU rendering.
No frame-time allocation, resolution reduction or TIME quantization is used.
Extra preparation was approximately 802 ms in the isolated Doll diagnostic.

## Measured scope

The frozen certificate lane's standalone GPU readback compared 1,536,000 pixels
exactly. Its single real Doll diagnostic measured 15 steady 60-frame windows at
33.424 ms average (about 29.94 FPS), versus 48.744 ms for the earlier GPU path in its
recorded run. These are cloud-Azahar results for those diagnostic hashes, not a
claim that the final combined package or physical 3DS runs at 30 FPS. Additional
heap and whole-app peak/free-memory margin on Old3DS remain unverified.

The combined ARM Pillow diagnostic reaches the real Mom door, seven-phrase
attack script and battle with both GPU options compiled in, but Pillow's
background does not meet the fast-path conditions. It correctly falls back to
CPU. The observed text telemetry is approximately 5.4 FPS and 167 ms background
composition. This is a separate substantial performance limitation; the Doll
measurement must not be generalized to Pillow or the whole game.

## Final integration verification

Final affected host checks passed 5/5 in 37.64 seconds; the final complete
ASan/UBSan suite passed 77/77 in 130.33 seconds with LeakSanitizer disabled.
There was no second complete final host sweep.

The first host pass was 74/76. Its failures exposed older test fixtures that
assumed pre-Pillow schema/section scope. The evidence and source-backed fixture
corrections are retained. A preliminary build also rejected an upstream symlink
outside the candidate root; a full unchanged pinned source copy resolved that
without relaxing the provenance boundary.

The generated-origin ARM diagnostic completed the real Mom door, seven Pillow
phrases, two battle rounds and victory, EXP 3→8 / bank 5→10, Minnie leave at
(64,370), return door at (176,369), and hallway door event ending at (40,370).
There were no HOUSE/ROUND errors. Later QA navigation hit the real stair warp
to the living room, so tutorial choices and the subsequent Doll request are
verified only by the host complete-chain harness, not this ARM run.

## Remaining limits

No new screenshot, audible DSP output, CIA installation, full NewGame route,
actual player-save migration or Old/New3DS hardware validation is claimed.
The previous Doll-postwin Azahar crash remains a historical unresolved finding.
Podunk motion is still an independent unmerged lane. Full title opening/naming,
Settings/Copy/Delete, generic consumables and later game content are unfinished.
This checkpoint does not represent a stable release or full-game
completion percentage.
