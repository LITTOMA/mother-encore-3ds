# Checked standalone Items resource

`romfs/data/opening.encitems` is a separate resource, compiled by
`tools/native_items.py` from `content/native-items.json`. It does not require
recompiling the C++ engine when compatible item records, initial inventory,
geometry, animation values, colors, resource paths, or audio bindings change.
The source extractor and texture builder are `tools/items_assets.py`.

The C++ API is `ItemData`/`ItemView` in `include/encore/items_data.hpp`.
`ItemData::load` validates the entire candidate before copying its bytes and
replacing the owner. Failure leaves the previous owner and its borrowed views
unchanged. A successful reload invalidates old views. A view must not outlive
its owner. All getters return values, strings are bounded borrowed views, and
out-of-range accesses return empty/zero records. Optional indices use
`item_no_index` (`UINT32_MAX`). This is not an alias of a C++ class layout on
disk: all fields are decoded individually with checked little-endian reads.

## Header and directory

All integer and IEEE-754 binary32 fields are little endian. There is no native
alignment, pointer, C++ enum object representation, or implicit padding in a
record. Maximum resource size is 1 MiB.

| Byte offset | Format | Meaning |
|---:|---|---|
| 0 | 8 bytes | `ENCITM01` |
| 8 | u32 | Format version 1 |
| 12 | u32 | Exact total byte size |
| 16 | u32 | Standard CRC32 of the entire file with these four bytes zero |
| 20 | u32 | Section count 11 |
| 24 | u32 | Capability version 1 |
| 28 | u32 | Rule version 1 |
| 32 | 20 bytes | Reviewed upstream commit `7d9246600fffe518408f5830d4848635019005a3` |
| 52 | 12 bytes | Reserved; must be zero |
| 64 | 11 × 16 bytes | Directory in the exact section order below |

Each directory record is `u16 kind, u16 stride, u32 offset, u32 count,
u32 byte_length`. Nonempty sections start on four-byte boundaries, follow the
preceding section without overlap, and contain `count × stride` bytes. Any
inter-section padding must be zero. Empty sections have zero offset and size.
Directory/header mismatches, arithmetic overflow, noncanonical empty sections,
nonzero padding, and trailing bytes fail closed.

| Kind | Section | Record stride | Maximum/required count |
|---:|---|---:|---|
| 1 | Strings | 1 | 1–65,536 bytes |
| 2 | Metadata | 16 | Exactly 1 |
| 3 | Definitions | 48 | 1–256 |
| 4 | Instances | 16 | 0–64, also bounded by metadata capacity |
| 5 | Resources | 60 | 1–64 |
| 6 | Layouts | 84 | 1–128 |
| 7 | Parameters | 20 | Exactly 11 unique named slots |
| 8 | Clips | 24 | Exactly Open, Close, CursorIdle |
| 9 | Tracks | 20 | 1–256 |
| 10 | Keys | 24 | 1–2,048 |
| 11 | Sounds | 16 | Exactly one mapping per five event types |

Strings are a UTF-8, NUL-terminated pool beginning and ending with NUL.
References must address a string beginning. Every string, including an
unreferenced string, is limited to 4,096 UTF-8 bytes. Newlines in descriptions
are preserved. Invalid/overlong UTF-8, surrogate encodings, and partial strings
are rejected. Paths reject control characters, absolute paths, backslashes,
colons, empty components, `.` and `..`; sound source paths may explicitly use
the `res://` prefix followed by a safe relative path.

## Record schemas

- Metadata: four u32 fields `capacity, owner, flags, reserved`. Capacity is
  external (1–64); the engine does not supply a game inventory size. Owner is
  a positive stable session binding. Flags and reserved are zero.
- Definition: six u32 fields `id, source, name, description, icon,
  equipment_slot`; four i32 fields `heal_hp, heal_pp, max_hp_boost,
  max_pp_boost`; two u32 fields `flags, can_use`. The three text fields use
  string offsets. Icon is a texture resource index or no-index. Equipment
  flag 1 requires a reviewed slot index 0–3; a nonequipment definition uses
  no-index. Other flags are unsupported. `can_use` is 0 or 1. Stat values
  are bounded to ±65,535. These fields support the reviewed menu predicate;
  they do not imply that arbitrary item effects have been implemented.
- Initial instance: four u32 fields `id, definition, equipped, doses`.
  Definition is an index; equipped is boolean; doses are 1–65,535. Source
  constructor defaults are resolved by the source extractor, never by C++.
  Initial equipped instances must reference equipment, with one per slot.
- Resource: seven u32 fields `id, path, kind, width, height, columns, rows`,
  followed by 32 SHA-256 bytes. This matches the decoded `BattleResource`
  schema. Kinds are the existing resource kinds 1–3; texture dimensions must
  divide into the specified positive grid. SHA values must be nonzero and
  paths unique. Staging hashes every file and rejects missing/changed data.
- Layout: seven u32 fields `id, role, parent, kind, resource, frame, flags`;
  two floats `anchor.x, anchor.y`; four floats each for `rect` and `color`;
  four u32 patch margins in left/top/right/bottom order. Parent is no-index
  or an earlier layout index, making cycles impossible. Rect uses x/y/width/
  height; color is RGBA. Anchor coordinates are normalized. Flags are
  Visible=1, ClipChildren=2, Centered=4 (Sprite only), BehindParent=8
  (noncontainer only). Sprite and NinePatch frames index a valid texture
  grid. Only NinePatch may have nonzero patch margins. Text frame is a
  content-defined line index. Required bindings are one Grid, one InfoPanel,
  a sprite Cursor, and at least one Panel, ItemLabel, and Description.
- Parameter: u32 enum slot followed by four floats. Every declared slot
  occurs exactly once. There is no unknown-name or missing-value fallback.
- Clip: four u32 fields `id, role, first_track, track_count`, float duration,
  u32 loop. Duration is positive and at most 120 seconds; loop is boolean.
- Track: five u32 fields `target, property, interpolation, first_key,
  key_count`. Target is a layout index. Interpolation 0 is linear with the
  key's Godot ease; 1 is discrete. There are no duplicate target/property
  pairs within a clip. Frame tracks require discrete interpolation.
- Key: six floats `time, ease, value.x, value.y, value.z, value.w`, decoded
  as `BattleKey`. Times strictly increase within each track and lie in its
  clip. Ease is bounded to ±100. All vectors must be finite and bounded.
  Alpha, visibility, frame, scale, and rectangle properties have additional
  semantic ranges. Each track and each key belongs to exactly one owner;
  overlaps and orphan animation records are rejected.
- Sound: four u32 fields `event, path, audio_id, flags`. Path is the source
  sound path, audio_id is its positive stable audio-bank binding, and flags
  are zero. Events Open, Move, Close, Disabled, Confirm are each present
  exactly once. Shared paths/IDs across different event types are allowed.
  Cross-bank sound existence is checked at integration/build time.

Stable record IDs are positive and unique within Definitions, Instances,
Resources, Layouts, and Clips. References use record indices, not stable IDs.
Unknown enums, flags, versions, capability values, and rule versions fail
closed. Getters do not supply content defaults when a reference is invalid.

## Named parameter slots

Slots are the following 1-based sequence:

1. SourceViewport: width, height, 0, 0
2. PlatformViewport: width, height, 0, 0
3. GridShape: positive integral columns/visible rows, positive x/y pitch
4. LabelSize: width, height, 0, 0
5. CursorOffset: x/y offset, positive frame width/height
6. CursorMotion: positive move duration, repeat interval, animation fps, 0
7. InfoMotion: positive duration, nonnegative travel distance, 0, 0
8. DisabledColor: RGBA, each in [0,1]
9. NormalColor: RGBA, each in [0,1]
10. ScrollColor: RGBA, each in [0,1]
11. InputBinding: source button, source key, positive single-bit platform
    button mask, 0; all four values are integral

Role/kind/property enum values are defined by the header and compiler lists;
they are execution schema, not source game tables. Layout roles are
Container, Panel, Grid, ItemLabel, ItemIcon, Equipped, Cursor, InfoPanel,
Description, Scrollbar, ScrollBackground, ScrollThumb, Hint. Draw kinds are
Container, Sprite, Rectangle, NinePatch, Text. Properties are Position,
PositionX, PositionY, Scale, Alpha, Visible, Rect, Frame, Offset.

## Reproduction and validation scope

Run `python3 tools/items_assets.py verify`, then
`python3 tools/native_items.py compile` or `verify`. The former regenerates
the source IR semantics from reviewed pinned sources and checks native-layout
provenance. The latter verifies source-review/inventory/dependency hashes,
compiled asset hashes, exact field sets, and the bounded binary schema.
`native_items.stage_files(romfs)` returns only the checked pack and referenced
hashed resources; the existing audio bank stages sound data separately.

Tests in `tests/test_native_items.py` and `tests/items_data_tests.cpp` cover
all truncations, checksum-correct malformed metadata/directories/records,
UTF-8, references, parents, ownership, parameter semantics, required bindings,
missing events, unsafe flags, data-only edits, staging hashes, safe getters,
copied ownership, and transactional failure. Host and sanitizer results do
not establish 3DS rendering, audio, GUI, or hardware equivalence.
