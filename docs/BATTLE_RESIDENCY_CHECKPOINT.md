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
The background reservation is calculated from actual dimensions, palettes and
kernel allocation shapes, not a fixed 400x240 guess. A distinct 512x512/eight-
color two-layer stress case requires 21,781,920 bytes and is rejected by a
20 MiB ticket before allocation.

The opening encounter component ceiling is 28 MiB CPU and 6 MiB incremental
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
