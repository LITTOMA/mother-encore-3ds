# Bounded rules6 → rules7 LOAD migration

2026-10-02. This is a read-only native-save compatibility bridge for the Pillow/Minnie feature. It is not a save-recovery tool, a Godot save importer, or a general version migration framework.

## What is accepted

The exact legacy identity is content family `0x454e0002`, content revision `1`, rules revision `6`, save schema `1`. The destination is the same content family/revision and save schema with rules revision `7`. The frozen historical resources in `content/legacy-rules6/` are source-derived defaults and validation content from the reviewed checkpoint, not user profiles or played saves. Their provenance/byte fingerprints are retained in `manifest.json`, together with the old Room, House and NativeSession IR. The compiler validates the frozen fingerprints and compatible target defaults/scope before emitting the independent `rules6-to7.encmigration` resource (98,666 bytes).

The generic save decoder remains unchanged and exact-version strict. The slot entry reads a file once, tries current strict decode, and otherwise performs strict decode with the declared historical identity. It then validates the entire legacy snapshot with the original NativeSession, Room, House, Round and Items resources before preparing it with the current resources/font. Both checks must pass before outputs are committed. Legacy validation retains the old camera bounds, flag mutation registry, item/status/settings/level constraints, and known seen-dialogue/encounter identities. A rules6 label cannot smuggle new Mom-room positions, Pillow encounters, or Minnie progress into the new scope.

No snapshot field is changed. All 178 flags were already present; `minnie_leave` and `minnie_door` remain their actual saved false values. `pillow_attack=true` is valid old progress, including its Doll route meaning, and is preserved. No flag, default, UID, money, playtime, settings, facing, saved timestamp, or RNG state is inferred. Re-encoding the prepared snapshot with its old identity produces exactly the original bytes in the focused tests. Missing/unknown data or non-lossless current restore fails with a reason, leaving outputs untouched. Prepared item views borrow only the current owners, never the temporary historical bundle.

## Old slot preservation and Record

Scanning and LOAD never write the primary, backup or any temporary save file, and do not consume RNG. They do not substitute `.bak` files. No actual user save/profile was accessed in development.

The existing writer guard is intentionally unchanged: Record over a rules6 occupied slot fails before any write. The platform reports `Old/incompatible slot preserved. Choose an empty slot to Record.` and follows the existing failed-save acknowledgment branch, without claiming success or setting the saved flag. Select an empty slot in Record to create a rules7 save; that new slot can LOAD normally and does not require the historical bundle. The old primary and backup remain available for rollback. If no empty slot exists, there is no new deletion/recovery/overwrite UI in this change; cross-version overwrite requires separately reviewed work.

## Verification and limitations

Focused host checks only: 220 C++ assertions and four Python asset tests passed, represented by the `session_migration` and `session_migration_assets` CTest targets. Generated fixtures cover field/UID/money preservation, all legacy cameras, explicit false registries, valid new rules7 state, new-scope back-label rejection, unknown identity/revision/schema/scene/fields, truncated/bad-checksum files, invalid historical bundles, transactional output, one-read/no-write behavior, guarded old-slot Record refusal, and explicit new-slot Record/reload in memory.

The initial host compile failed because existing House/Items/Round owners deliberately disallow moves; the historical bundle now owns those resources by unique pointers, leaving their classes untouched. Full regression, sanitizers, ARM linking, emulator/GUI, actual SD-card and hardware checks were not run for this lane; they require separate integration validation. No live saves were used. Historical versions other than this exact rules6 scope remain rejected.
