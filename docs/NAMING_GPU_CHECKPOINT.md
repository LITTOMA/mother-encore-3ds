# Naming, Pillow row-linear GPU and persistent player checkpoint

This is a historical development checkpoint from 2026-10-02. Later startup
fields and settings are covered in STARTUP_SETTINGS_CHECKPOINT.md; this record
does not describe the full current feature set or a complete game.

## Three merged lanes

- Source-backed first Ninten naming field; source English case panels, command group, defaults, rejection and cancellation. Nickname is carried by the existing session into house text, battle party plate and source-keyed dialogue/reward text, Record metadata and LOAD. Save schema and round packs are unchanged. Remaining five startup fields, settings, final confirmation and original intro are not implemented.
- Frozen exact row-linear Pillow sparse-span generation plus bounded PICA batch. Source defaults remain GPU=0/certificates=0; the development artifact explicitly enables 1:1. CPU compatibility artifact is retained. No texture-strip research was merged.
- Address-stable player/camera owner used by the existing house world. New Game/LOAD retain fresh independent-owner semantics. A staged retained binding is read-only and cannot reset live state. Actual cross-scene Podunk activation and full BVH are still independent work.

## Consolidated final gate

Host: 81/81 passed in 49.18 seconds. ASan/UBSan: 81/81 passed in 122.99 seconds. LeakSanitizer disabled in this executor; no leak-check claim. One aggregate run per configuration; no unchanged repeated full tests.

Real devkitARM CPU-off 0:0 and GPU/certificate-on 1:1 3DSX and CIA builds passed. GPU CIA RomFS extraction matches all 152 staged input files byte-for-byte; content hash verification passed. Homebrew Ticket/TMD/AccessDescriptor signatures fail retail checks as expected. No CIA installation.

The fixed 5,839-file upstream inventory passed read-only verification.

## Bounded ARM smoke

One final merged-code ARM diagnostic typed mixed-case Ab through the source keyboard/case toggle and accepted into the playable bedroom; then explicitly generated a post-Lamp origin. It reached actual Pillow entry, resolved live battle text with Ab, rendered 100 active row-linear GPU frames, entered Bash targeting, cancelled back to Commands, preserved the exact player/camera owner throughout this house/battle route, and exited normally.

This isolated generated diagnostic uses a fresh emulator profile and prohibits player-save IO. It is not a full title/story traversal, new GPU readback or new performance benchmark. Naming’s standalone production title→Ab→bedroom smoke and Pillow’s independent 768000-pixel readback remain separate, exact-hash evidence; neither is silently relabeled as this final package’s full acceptance.

## Historical artifact identities

- `dist/encore-native.3dsx`: 63880380 bytes; SHA256 `3c753cc49ea432881700883cd2edeb90c8a23f4c3fc1559bfe49cc1489eff99f`.
- `dist/encore-native.cia`: 64103360 bytes; SHA256 `8f8376789828cf55a42c8cf5298f368df02069c82ba325e71a0d2ed3d358269c`.
- `dist/encore-native.elf`: 28871328 bytes; SHA256 `c9fc8499a6ece6ef091017cea5fec91c738a3df8c79587c2ea595dc0dcf891ec`.
- `dist/encore-native.smdh`: 14016 bytes; SHA256 `ece8a6bb99d396700f25b669631498ef9c28f13a91b249be77e3e085eefd7bcf`.

## Important limits

No actual player-save access, CIA installation, audible DSP or physical Old/New3DS validation. The historical complete-NewGame Doll-postwin Azahar crash remains unresolved. No full-story or full-game acceptance. Pillow preparation, memory margin and actual hardware performance remain unverified. The independent row-linear candidate measured approximately 14.96 FPS versus the previous separate CPU diagnostic approximately 5.4 FPS; these are cloud-emulator-specific observations.
