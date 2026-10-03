# Native original-session snapshot

`include/encore/session_save.hpp` and `runtime/session_save.cpp` implement a
standalone value codec and local file adapter for the original Phone/Record
integration. They do not use `Game`, M0 state, M0 RNG, or `runtime/save.cpp`.
No scene names, party/item/flag IDs, tuning, initial game values, encryption
passwords, or UI/audio calls are built into this module.

## API and caller responsibilities

All types are in `encore::upstream`. `SessionSnapshot` is an owned value containing
source string identities, current position/direction, run/shadow settings, party
ordering and character state, inventory/key items/storage, flags/object/seen
registries, region keys, rare-drop counters, encountered enemies, money, elapsed
playtime, names, settings, and caller-supplied version/date/debug metadata.
`SessionSaveCompatibility` carries independently caller-owned content family,
content revision and rules revision; the native save schema is separately versioned.

`validate_session_snapshot` validates structure. `encode_session_save` produces
bytes, and `decode_session_save` consumes bytes and an expected compatibility
identity. They leave the output untouched on rejection. `read_session_save` and
`write_session_save` add local synchronous file IO. An optional `SessionSaveFileOps`
implementation permits another storage adapter and deterministic IO failure tests.

The caller must supply all actual source-backed defaults from checked content.
Default-constructed values are initialization aids, not playable game defaults,
and fail validation until required identities/state are supplied. A successful
decode is a candidate snapshot, not authorization to apply unknown game content.
Before changing live state, the integration must validate every identity and
value against its loaded resources and exact supported scene/party scope. It
must reject unsupported nonempty inventories, modifiers, statuses, registries,
settings or other state; silently dropping fields is not supported.

Use stable source IDs/UTF-8 identities, never transient table indexes. Validate
positions against the target scene's allowed geometry and collision policy;
check the source's allowed direction set, text speeds, menu flavors, prompts,
levels/EXP, item capacities/equipment slots/doses and supported skills/statuses.
The generic codec cannot infer these rules. Restore world/session/inventory into
temporary candidate objects and publish them together after all checks succeed.

`PartyMember.to_dict()` saves level, EXP, HP, PP, permanent boosts, affinities,
skills, status and inventory. Source-derived max HP/PP and other calculated stats
are deliberately not independent saved fields. Recalculate them from verified
progression/content and saved modifiers/equipment; do not reset current HP/PP or
replay rewards during loading. Only the currently supported native character
type is actionable; this codec does not claim PartyNPC or whole-game loading.

An inventory item stores its source identity, equipped flag, positive resolved
dose count and unsigned 32-bit UID (zero is valid). UIDs must be globally unique
across all saved inventories. Preserve source/runtime-assigned identity instead
of generating replacement UIDs during each save.

## Source mapping

Evidence is pinned to `7d9246600fffe518408f5830d4848635019005a3`:

| Fields/behavior | Official source |
|---|---|
| Snapshot fields; save before live `saved=true`; restore | `Scripts/global/global.gd:481–529,575–629` |
| Slot card name/level/scene/time/party | `Scripts/UI/SaveSelection/saveFile.gd:37–105` |
| Character fields and stat recalculation | `Scripts/global/PartyMember.gd:182–209,512–529` |
| Item identity/equipped/doses/UID | `Scripts/global/Item.gd:16–36,45–46` |
| Inventory serialization | `Scripts/global/Inventory.gd:52–64` |
| Status identity and optional passive-healing turns | `Scripts/Main/Status.gd:21–25`; `Scripts/global/Character.gd:356–371` |
| Global registries and settings | `Scripts/global/globalData.gd:20–75` |
| Region key integer counters | `Scripts/global/uiManager.gd:183–189` |
| Enemy rare-drop integer counters | `Scripts/UI/Battle/BattleItemPool.gd:29–48` |
| Encountered enemy booleans | `Scripts/UI/Battle/BattleSystem.gd:358` |

The native caller must reproduce Record's flag timing: phrase 4 clears the live
`saved` flag; a write snapshots its current value; only a successful write sets
live `saved=true` and updates the respawn checkpoint. Thus the first written
snapshot contains false and a later write during the same menu visit can contain
true. The codec never sets flags, closes menus, resumes dialogue or emits success.
A failed write must remain a failure to the caller.

The original encrypted JSON does not include RNG state. Schema 1 explicitly
records `SessionRngPolicy::NotSerialized`. It imposes no blanket reseed or
preserve-on-load operation. Pinned load functions do not directly reseed, but
inventory reconstruction eagerly evaluates `Item.get_uid` defaults, which call
`randomize()` and `randi()` even when a saved UID is present. Future LOAD must
reproduce those source side effects and ordering; current SAVE does not implement
LOAD or claim random-stream restoration. See the Continue audit for exact paths.
Value1 was not shipped in a usable save feature before this neutral-policy
correction. Native item UID allocation remains separately documented.

Source date dictionaries are represented as caller-supplied UTF-8 `saved_at`
metadata, not interpreted by this module. This format is neither JSON nor
byte-compatible with source encrypted saves; it is not encrypted or authenticated.
CRC detects accidental corruption, not deliberate modification.

## Format and checked bounds

All integers are little-endian; doubles are IEEE-754 binary64. The 28-byte header
contains eight-byte `ENCSNAP1` magic, uint32 schema, content family, content
revision, rules revision, and payload byte count. The payload follows the member
order implemented by `write_snapshot`/`read_snapshot`; it ends with RNG policy.
A final uint32 IEEE CRC-32 covers every preceding byte, including the header.
No trailing data, unrecognized schema/RNG policy, or compatibility migration is
accepted. Header values must match the caller exactly; none default to a baseline.

Strings have uint32 byte lengths and valid UTF-8 (no NUL, overlong encoding,
surrogate or out-of-range code point). Lists have uint32 counts. Boolean bytes
must be zero or one. Unique keyed entries are enforced within each registry;
characters, party membership, skills, statuses and item UIDs are checked. Party
members must reference a saved character. Snapshot order is preserved exactly.

Structural safety limits are 1 MiB per file, 4,096 bytes per string, 4,096 entries
per table, and 32 characters/party members. Counters (cash/bank/earned money, EXP,
keys, rare drops and passive-healing turns) must be in uint32 range; HP/PP/level
and resolved doses fit nonnegative int32, with level/doses nonzero. Permanent
boost values fit signed int32. These are native
representation caps, not source gameplay limits. Negative modifier values are
representable; their gameplay validity belongs to the content adapter.

Position and multiplier values must be finite and fit a float. Direction
components must be finite within [-1,1], with a nonzero vector; this permits
source unnormalized diagonal directions. Playtime is nonnegative and at most
2^53−1 seconds; text speed must be positive, finite and float-representable.
The integration imposes the stricter actual source rules. Decoder bounds are
checked before allocating/copying each string/list; all parsing occurs in an
independent candidate, and the output moves only after full validation.

## File replacement and failures

The local adapter never creates a directory. Callers supply a trusted slot path
and serialize all writes to it. Read performs bounded complete-file IO and full
format validation; it never automatically substitutes a backup.

For a write:

1. Encode and validate in memory; inspect and validate an existing primary
2. Exclusively create `path.tmp`, write/flush/close, reread and compare every byte
3. If a primary exists, exclusively create `path.bak.tmp` from its exact bytes,
   write/flush/close, reread/compare, then rename it over `path.bak` and verify the
   resulting backup again
4. Rename the verified `path.tmp` over the primary as the sole commit point

Any reported failure before the final replacement leaves the primary byte-identical.
The backup may already have been refreshed to the old primary. Unlike POSIX host
rename, official [libctru v2.7.0 archive_rename](https://raw.githubusercontent.com/devkitPro/libctru/v2.7.0/libctru/source/archive_dev.c)
deletes an existing destination and then retries renaming. If that final retry
fails, the destination is missing; it is incorrect to call 3DS replacement atomic.

After a failed final replacement, this adapter inspects the primary. If missing,
it exclusively writes back the known previous bytes and verifies them. This is
still reported as a failed save. If restoration also fails, the already verified
`path.bak` remains the recoverable prior save, the new `path.tmp` is retained, and
the error explicitly names the backup and required primary recovery. The backup
is never moved or modified during restoration. The storage adapter contract
requires failed replacement to preserve the source bytes, but permits removal
of the destination, matching libctru's implementation.

Failure to remove an owned temporary file is reported, and the next write refuses
to overwrite that leftover file. Stale temporary files/directories are never
silently deleted. Invalid/incompatible existing primary data blocks a write so
it cannot destroy the prior backup; a replace-corrupt-save workflow is separate.

Flush/close and readback validate process-visible IO; they are not a power-loss
guarantee. There is no claim of FAT/3DS crash durability, cross-process locking,
symlink hardening of a user-chosen directory, or directory fsync. The adapter
does not accept success if a checked write/flush/close/readback or rename operation
failed. Hardware SD behavior needs separate acceptance.

## Verification

`tests/session_save_tests.cpp` uses synthetic content IDs, independent of game
defaults. It covers all fields and Unicode roundtrip, saved-flag nonmutation,
every file truncation and single-byte corruption, every payload truncation with
repaired length/CRC, malformed nested records, bounds, duplicate identities/UIDs,
CRC-valid deterministic mutations, all injected storage operation failures,
readback damage, failed cleanup, stale files, missing directories, a real partial
write/flush failure forced by a temporary Unix process file-size limit, real local
replacement and retained backups. It also models libctru delete-then-rename
failure, successful primary restoration, and a second failure during restoration
with the old backup retained. Rejected encode/decode/read leave old outputs
unchanged; failed writes preserve the prior save at the primary or verified backup.

Focused host, ASan/UBSan and ARM object checks are separate from whole-project
integration, 3DS linking/packaging and the actual Phone/Record/restart path. These
focused tests alone establish neither a complete save UI nor emulator/hardware
save/load acceptance.
