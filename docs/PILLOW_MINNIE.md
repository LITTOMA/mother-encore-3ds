# Optional Pillow → Minnie branch

This branch extends the original house slice. It is an optional route before the
Doll melody; it is not a prerequisite for the main Doll/Carol/phone route.

## Content and execution

- The original `Upstair_Mom` / `Mom_Upstair` doors are connected. Their source
  transforms produce destinations `(464,169)` and `(176,369)`.
- `pillow_attack.yaml` runs all seven phrases, including the original fourteen
  position targets, looping Pillow motion, Minnie's continuous shake, camera,
  text, and the delayed battle boundary.
- Pillow is the real non-boss with its own exact sprite/background and checked
  `tackle` / `float` actions, EXP5 and bank/earned-cash5 reward. Live HP/PP,
  accumulated EXP, currency and inventory stay in the shared session.
- `minnie_leave.yaml` and `minnie_door.yaml` run in original handler order. The
  live teleport to `(64,370)` differs intentionally from the source reload-only
  position `(32,368)`. After the door cutscene, the live position is `(40,370)`.
- Minnie's original Yes/No/Cancel tutorial uses the existing dialogue choice UI.
  Source WAITs, hint color and gamepad wording are preserved. The externally
  declared native `ui_toggle` label is B. The open-door dialogue is also bound.
- Tutorial completion changes no flags. The existing running collision event
  opens Mimmie's door and reaches the original Doll encounter.
- Area6 requires `!minnie_leave`; Area7 requires `minnie_leave && !minnie_door`.
  Both inherit the parent `!doll_melody` condition. No optional progress is
  fabricated when the player chooses the main route.

## Compatibility and ownership

Room execution rules/capabilities are7. The existing binary section layout and
stable identity prefixes stay unchanged. New movement-path bits represent loop
and forced queue; a new typed StopActorLoop command and repeat-count flags are
validated before execution. Source loop-shake and repeated-jump delay values
live in external Rule records26 and27.

Rules4–6 remain readable with their original operand limits. House schema6,
snapshot schema1, and the source flag registry remain unchanged. Native save
compatibility is separately handled by the bounded rules6→7 migration work;
this feature does not rewrite any existing save or supply generic fallback.

Round schema5 supports a non-boss win-cutscene and conditional level2 growth.
The carried-session EXP path is Lamp3 → Pillow8 → Doll16. Growth is applied only
when the real threshold9 is crossed. Level2 entries do not learn or grow again.
The checked base-stat delta is added to live effective stats. Old round schemas
retain their prior contracts.

The shipping data build compiles Pillow's independent entry/round packs, and
RomFS discovery stages their referenced resources through the existing checked
Battle resource mechanism. `platform/ctr/main.cpp` needs no Pillow-specific
resource-selection code. The GPU renderer is unchanged in this branch.

## Verification and limits

- A generated post-Lamp harness walks
  through the actual Mom door, Pillow script, battle, reward, Minnie leave,
  hallway event, all three tutorial answers, real running door collision,
  sister-room warp and Doll request. It is not a full NewGame UI playthrough.
- An independent official Godot3.6.2 unchanged-source comparison covered
  8 cases/1,125 frames/15,791 checks. Shared loop state, stopped queued
  orbit, repeated-jump final wait and forced idle queue are covered.
- `reports/pillow-battle/`: checked source art/compilers,8 native first-round
  action/RNG comparisons, and44,765 final host schema/reward checks. Earlier
  sanitizer evidence is explicitly marked before the final additive-growth
  adjustment.
- A historical real ARM3DSX build staged 148 resources. This result does not
  validate later source/provenance regeneration or the current package.

No new screenshot, emulator interaction, audible DSP output, CIA installation or
Old/New3DS hardware validation is claimed. Minnie's later storage conversation,
other house routes and further content remain separate work.
