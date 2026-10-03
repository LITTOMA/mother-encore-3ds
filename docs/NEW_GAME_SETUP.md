# Original Ninten naming, bounded first stage

This isolated feature adds the first original name-entry stage before the
existing playable opening. It is not the complete original New Game sequence.
The upstream remains Act2v0.4.1.0, commit
`7d9246600fffe518408f5830d4848635019005a3`.

## Source and supported behavior

`Data/NamingSequences/intro.yaml` requests, in order, Ninten, Ana, Lloyd,
Pippi, Teddy, favorite homemade food, settings, and confirmation. The first five
names allow seven characters; food allows thirteen. The later player's own
name is a separate `playername.yaml` scenario, not a startup field.

This slice presents only the real first Ninten field. It begins empty, retains
source English uppercase/lowercase keyboard positions, and provides Backspace,
OK, and the seven original Don't care choices: Ninten, Ken, Douglas, Jeremy,
Mark, Ryu, Colin. Source comparison is case-insensitive after trimming edges;
the stored value retains its actual spelling and spaces. Empty/space-only and
blacklisted names are refused. Duplicate checking uses the source pending
values. Those other startup fields are still empty at this stage; it does not
invent party records or ban the names Ana/Lloyd before they are chosen.

B erases one character. B on an empty name returns to the title after the
source half-second cancellation guard. A selects the current key. X maps the
source keyboard-panel toggle, Y jumps to the source command group, and R maps
source next-field acceptance. Navigation uses the existing 3DS distinct-tap /
held-repeat adapter, with the source subgrid links and a source cursor tween.
The title's A/touch event is not processed again by the new naming owner.

The source's accented/symbol cells remain in place, dimmed and unselectable:
the current gameplay/save text path supports printable ASCII only. Each panel
has 61 selectable cells and 27 disabled cells, including commands. The original
320×180 composition is centered in 400×240 at 1:1; no stretched glyphs or new
upper-screen instructions are added. The lower screen explains controls and
that the other names/settings/intro remain unfinished.

## Actual session behavior

An accepted field commits only after the fresh playable scene initializes.
It updates `SessionCharacter.nickname`, then the house runtime's nickname.
House dialogue, battle party plate, action text, EXP/level/skill text, Record
card metadata and subsequent LOAD use that value. Stable actor IDs remain
unchanged. Cancelling never mutates the current session or writes a save.

`ENCNAMES` carries 81 indexed text bindings for the existing Lamp/Doll/Pillow
round resources. Each record checks encounter ID, original text index,
translation key, source template and compiled default value before inserting
its nickname segment. It never searches and replaces a name in live text.
The optional presentation resolver leaves every existing default-text caller
unchanged when absent. Reward arithmetic, growth conditions, RNG and source
scheduling are unchanged.

There is no save-schema bump. Existing default/custom ASCII nickname saves
continue to decode with their previous bytes and are not rewritten during
scanning or loading. This slice creates no Ana/Lloyd/Pippi/Teddy roster state.
Record remains the normal explicit save action; starting a game does not save.

Naming textures are released after waiting for the GPU on accept/cancel/reset;
they do not remain resident alongside normal gameplay textures.

## Data and source checks

`tools/new_game_assets.py` produces `content/native-new-game.json`, the checked
`romfs/data/opening.encnewgame`, and two original-font keyboard texture layers.
It reuses the checked Plain box, cursor, Ninten and shadow textures. Content,
translations, source animation keys, limits, layout, blacklist, defaults and
battle bindings are external resources, not C++ game-content tables.

The original source container/label geometry is resolved by official Godot
3.6.2 in a minimal isolated layout probe; its retained report includes actual
positions of all 88 keys. The keyboard graph projects the source Cursor and
NamingScreen subgrid rules at settled cursor positions. Mid-tween rapid
cross-grid navigation has not been compared frame-by-frame against Godot.
Do not describe this as full original naming-screen visual/timing equivalence.

## Deliberately unimplemented

The remaining five naming fields, settings, final yes/restart confirmation,
original Introduction scene and title intro remain absent. First-field OK
continues the existing house opening instead of pretending those stages ran.
The naming entry/exit choreography, prompt blink, case-label/ButtonPrompt art,
Mother Earth Piano naming track and dedicated acceptance/error sounds are not
yet ported. Existing source cursor/select/back sounds are reused where already
available; audible output still requires DSP/hardware verification.

## Verification

- Focused host naming/source checks: 2/2 passed.
- Focused naming test under ASan/UBSan: passed, LeakSanitizer disabled for the
  known executor limitation.
- Real devkitARM compile and 3DSX packaging: passed; 152 native RomFS files.
- Tests cover custom typing, length/empty/blacklist/default cycling/cancel,
  default behavior without resolver, action text with resolver, all existing
  encounter text records, source CRC/schema/size rejection, Record codec and
  fresh scene LOAD with custom nickname, and unchanged old default save bytes.
- Actual production ARM GUI check in a fresh cloud-Azahar profile: title A
  opened an empty field without inherited input, mixed-case `Ab` was entered
  with the source keyboard/case toggle, and R accepted into the playable bedroom.
  The guest exited normally with START. No game save was created or read.
- No full regression, CIA installation, audible output or physical 3DS claim
  is made. Record/LOAD/custom battle text coverage is the focused shared-core
  harness; this GUI check did not replay those later sequences.

Integration must apply only the lane manifest/patch. If another feature changes
shared content dependencies, rerun the source extractor/review and the focused
check against that combined data; do not edit generated hashes by hand.
