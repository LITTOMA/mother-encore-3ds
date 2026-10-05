# Shared battle residency and exact transition masks

Checkpoint: 2026-10-03. This removes synchronous presentation preparation from
encounter commits and eliminates discarded invisible first-frame raster work.
It is an **improved-loading checkpoint, not full seamless-transition acceptance**.
The exact background computation floor remains open.

## Ownership and execution

- Scene declarations enumerate checked battle/round dependency unions before
  interactive world admission. Preparation never chooses a random encounter,
  dispatches a script, ticks an actor, advances gameplay time or draws shared RNG.
- The current opening adapter inventories every RoomBattle declaration rather
  than special-casing Lamp, Pillow or Doll. Prediction only reorders this union.
- A private low-priority worker first probes bounded headers. Main-thread epoch
  tickets reserve pack/parser, decoded resources, exact kernel/proof working
  sets and stack before each allocation phase. File and CRC loops and all large
  kernel/mask loops are cancellation-aware. Worker publication uses release/
  acquire and join. Only the main thread creates or publishes Citro3D objects.
- GPU texture copies are bounded to 64 KiB per pump. Checked exact-path texture
  leases retain both texture and Tex3DS metadata. CPU indexed atlases and mask
  plans use immutable shared ownership. Last texture release stays at the
  owner's GPU-safe lifecycle boundary.
- Initial New Game / LOAD uses its existing responsive scene-preparation phase
  to admit all declared candidates. Naming/title textures are released first;
  the locale/font ownership fix is preserved. No battle loading scope exists.
- Encounter commit swaps ready owners only. The synchronous boot renderer is
  explicitly rebuilt into full indexed residency; a matching path alone is
  never a readiness certificate. Transition-frame reads are instrumented too.
- Cancellation is deliberately synchronous cooperative drain/join; no unsupported
  nonblocking replacement API is exposed. Tickets reject stale publication.
  Menus obscuring the same world do not retire its ready candidates.

## Bounds and future scenes

The current CTR opening profile allows 1,024 resource references, eight background
layers, bounded checked pack inputs, at most 2 MiB of indexed pixels per asset,
a 1 MiB transition-plan payload and a 300 KiB transition-batch LINEAR grant.
Larger future dependencies are explicitly rejected before interactive admission.

The LINEAR grant includes cold battle/round atlases after delayed boot loading.
The previous 6 MiB grant assumed these textures were already resident and rejects
the opening cold path. Actual checked RGBA8 backing shapes give a fully cold
Lamp upper bound of 11,273,472 bytes with all three GPU mechanisms enabled;
12 MiB remains additionally constrained by actual free platform LINEAR.
Introduction image/font owners retire at the final source house boundary, before
house and encounter allocation. Checked intro data and the final door overlay
remain alive; source clocks and masks continue without replaying scene images.
This arithmetic is a host allocation diagnostic, not a hardware measurement.
Manual cold-grant/shortage/diagnostic regression cases are retained, not run.
The background reservation is calculated from actual dimensions, palettes and
kernel allocation shapes, not a fixed 400x240 guess. A distinct 512x512/eight-
color two-layer stress case requires 21,781,920 bytes and is rejected by a
20 MiB ticket before allocation.

The opening encounter component ceiling is 28 MiB CPU and 12 MiB incremental
LINEAR, additionally limited by remaining platform heap minus 2 MiB outside
headroom and actual free LINEAR. The old unproven owner is charged until its
replacement is committed. Retained texture/batch allocations use actual padded
GPU sizes, including optional texture-strip resources when compiled.

The prior 24 MiB component ceiling correctly rejected the integrated three-
candidate working set. The conservative per-candidate retained CPU sum is
25,553,567 bytes; its maximum staged reservation is 26,043,226 bytes. Immutable
shared atlases/plans are physically shared but conservatively counted per
candidate in this component ledger. On the measured Azahar profile the whole
process used 26,678,288 bytes of a 95,010,816-byte heap at admission, with
9,119,872 bytes LINEAR free. These are **not physical Old/New 3DS guarantees**.

EncounterResidency exposes an explicit other-live grant for a future aggregate
SceneFactory transaction. Podunk world geometry, static/dynamic textures,
transforms, controllers, fonts/audio, outgoing scene and incoming staging are
not validated by the opening component pass. A content-hash unique-allocation
registry and a single feasible world+encounter transaction remain integration
requirements; same roles or source PNGs do not imply identical T3X backing.

## Source encounter manifest

`tools/encounter_dependencies.py` compiles the real source graph without sampling:
13 maps, 192 spawners plus four direct actors, 20 dialogue formations, 45 enemy
profiles and six weighted reinforcement tables. It preserves provenance, exact
weights and independent spawner appearance gates (source rate 80 admits residues
0..80, i.e. 81/100). Real three-enemy formation and reinforcement unions are
validated. The strict adapter verifies pinned source IR, bindings and assets.
Only the existing three checked battle/round pairs are compiled. Unsupported
combat execution and unsupported resource preparation are distinct gates; this
manifest does not claim an implemented arbitrary multi-enemy battle engine.

## Exact mask composition

The immutable run plan reproduces source mask equality, alpha and border
extension. A prepared GPU batch intersects complete exact background row spans
with mask runs and uses source-alpha blending over the existing world draw.
All-flat frames skip background generation. Unexpected composition/draw contract
failure reconstructs both background and mask through the exact CPU path.
The provably invisible first frame skips background work while preserving all
clocks, gameplay events and draw pixels.

The isolated ARM/Azahar readback passed 51 comparisons / 4,896,000 exact RGBA
pixels: all 25 real masks at 320x180 and 400x240, animated Pillow spans, a following
C2D sprite and 96,000-span/all-alpha stress. Host/cancellation tests passed
853 checks / 7,709,016 pixels; independent sanitizer audit passed 23,925 cases /
1,218,454 pixels. Successful mask arithmetic is unchanged by cancellation hooks.
Physical sampler qualification is not inferred from Azahar.

## Focused actual entry evidence

Normal title → isolated undefeated save LOAD → original Lamp doorway trigger
passed. The source save stayed byte-identical. LOAD admission took 5,699 ms
inside the existing LOAD phase (6,116 ms total); no additional encounter screen
was inserted. A separate generated post-Lamp diagnostic drove real Pillow combat,
reward/Minnie events and the real Doll door-ram trigger. It explicitly aligns the
player before that door ram; it is not a full New Game or complete tutorial test.

| Entry | commit | first submission | first mask submission | visible presentation upper bound | worst mask gap |
|---|---:|---:|---:|---:|---:|
| Lamp | 0.233 ms | 17.88 ms | 34.16 ms | 64.40 ms | 50.36 ms |
| Pillow | 0.262 ms | 16.26 ms | 32.66 ms | 62.78 ms | 67.73 ms |
| Doll | 0.271 ms | 16.02 ms | 32.44 ms | 62.56 ms | 49.86 ms |

All commits reported zero presentation load activity; all three actual entries
had zero instrumented indexed-render I/O and no late readiness miss. These
counters are not a universal claim about audio streaming or every filesystem
operation. This emulator lacks NDSP audio availability; audio-enabled timing
is still unqualified. Normal streaming retains its original source clocks.

The old first Pillow frame wasted 164.555 ms composing pixels that were never
drawn. The new first-frame composition is zero. Every sampled Pillow/Doll mask
frame used the exact GPU batch. Their remaining peak composition costs are
53.51 / 28.74 ms in exact background span generation. Lamp uses GPU flat masks,
then exact CPU fallback when the revealing frame requires its unsupported barrel
background; steady Lamp/Doll are approximately 30 FPS and Pillow approximately
15 FPS in this standard backend. Presentation timestamps are conservative
observations after queue completion and two VBlanks, not physical LCD scanout.

## Remaining GPU work

Qualify the optional generic row-linear texture-strip sampler before proposing
its default enablement. Steady Pillow strips alone do not fix entry: the current
mask path requests complete background spans. A bounded exact GPU mask clipping
seam must preserve the original mask/color/alpha operation over direct texture
backgrounds. Doll's certified region source and Lamp's barrel/radial source need
separate source-domain proof-backed GPU designs. No approximation, rounded time,
RNG advance or unqualified hardware claim is part of this checkpoint.

These measurements belong to the scoped checkpoint described above. They do
not establish current whole-suite, complete-game or physical-hardware validation.


## Direct mapped texture sampling

The optional texture backend now admits two repeated barrel layers with the
existing separable X oscillation / Y compression contract. It is selected by
loaded capabilities, never an encounter name or a per-battle switch. Pillow's
existing row-linear backend and 320x180 reference fallback are unchanged.

A prepared eight-pixel secant plan encloses each source coordinate and cached
trigonometric basis. Frame preparation updates source phases/movement, checks
all interior pixel centers against that enclosure, and accepts a texture strip
only when its possible source samples have the same checked source index.
Unsafe strips subdivide; one-pixel leaves snap the original scalar sample to
its exact source texel center. Period seams split before GPU normalization;
non-power-of-two source images are sampled in their padded 256x256 textures.
No frame color buffer, palette blending loop, texture upload, frame allocation,
time quantization or RNG calls are part of this direct path.

The fixed palette-row adaptation is applied before GPU texture creation.
Weighted base palettes and two binary OR/AND corrections reproduce every
finite palette pair of the existing CPU blend, including the admitted second
layer opacity. The bounded solver rejects palettes it cannot decompose exactly;
it does not substitute a quantized hardware alpha. Three draws use identical
nearest-sampled geometry and restore Citro2D state afterwards.

The batch cap is 32,768 strips and six 256x256 RGBA8 source-derived textures:
2,883,584 LINEAR bytes. The optional CPU secant plan is 576,000 bytes at400x240;
it replaces the Doll region certificate table rather than retaining both. The
encounter LINEAR ceiling is now16 MiB, still constrained by actual free LINEAR
and atomic admission. Metadata-based reservations include the new allocations
only for eligible source shapes. Allocation, shape, phase or capacity rejection
retains the prior checked CPU/span path without a partially rendered frame.

The lower screen reports `GPU mapped textures`, strip count and scalar boundary
pixel count only when that frame actually selected the direct path. Transition
mask composition continues to use the existing exact mask/span backend.

CPU coordinate enclosures and finite palette reconstruction do not qualify
undocumented physical PICA raster/interpolation precision. The additional f24
allowance remains an experimental platform assumption, as with the existing
row-texture backend. Real console pixel equivalence, visual behavior and FPS
remain unverified. Manual mechanism positive/negative cases are retained; no
test suite or sanitizer has been run for this change.


## Mapped texture frame-cost follow-up

The reported Citra regression reaches single-digit game FPS even while the
mapped GPU path is active. The preceding implementation still ran scalar
cosines at every one-pixel leaf and repeated geometry for three draws.

The source shader first applies static radial distortion, then time-dependent
X oscillation, Y compression and scrolling before nearest sampling and blending.
The fixed distortion and trigonometric basis remain prepared once. A summed-area
source-index proof now accepts uniform swept rectangles in constant work; equal
adjacent constant source pairs merge into wide strips. Ambiguous leaves reuse
the existing guarded coordinate bounds and compute original cosines only for
unresolved axes. These proofs preserve sampled indices; they do not simplify
the source animation, quantize time or generate a frame color texture. The only
new prepared storage is four bytes per source palette index, charged to admission.

A generic finite-palette solver additionally admits an exact decomposition
`C(i,j) = A(i) + B(j) - K * row(i) * column(j)`. It derives every value from loaded
resources, rejects intermediate saturation, and verifies all palette pairs
against the original float blend. Two source textures carry RGB and binary
alpha masks; four TEV stages complete one geometry draw. Other admitted palettes
retain the exact three-draw backend. The two-texture path saves 1 MiB of actual
LINEAR allocation while retaining the conservative six-texture budget ceiling.
No encounter IDs or source colors are embedded in this mechanism.

Lower-screen diagnostics distinguish strip count, selected draw passes, CPU
coordinate preparation, CPU command submission and original cosine calls.
Submission includes the preceding Citro2D flush and does not measure GPU
completion. Manual pixel-parity/negative cases remain available; no automatic
tests or sanitizer runs are part of this follow-up. Current emulator FPS and
physical-console equivalence/performance remain unverified until the new
installable is actually exercised.
