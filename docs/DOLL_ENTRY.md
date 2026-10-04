# Source Doll entrance resources

Pinned source: Act2v0.4.1.0, commit `7d9246600fffe518408f5830d4848635019005a3`.
This slice provides the original Doll entrance, roster, menu and background data. Doll combat, victory, levels and postbattle flags are outside this slice. The caller must preserve current party HP/PP/EXP and reject unimplemented commands; baseline participant rows must never reset party state.

## Resource identity

`content/doll-entry.json` is explicit reviewed input. `tools/doll_entry_asset.py extract` refreshes it only when requested; ordinary compilation does not re-extract source. The compiler produces `romfs/data/doll-entry.encbattle`, referenced by Room battle stable ID2, player instance0, Doll instance2. The binary has no dependency on Room bytes, avoiding a resource-hash cycle.

The pack reuses the checked opening transition/font/menu resources, Ninten's existing texture and `graphics/ui/house/doll.t3x`. Full Mimmie and Doll sheets plus emote art already exist in the checked house/actor bundles; no duplicate textures are introduced. The cinematic Room adapter supplies source Float, Mimmie directional animation and exclamation tracks.

New assets are the source 35×51 Doll battle sprite, lossless indexed Baby176×172 texture and original4×4 palette. Enemy center is source `(160,73.5)` before display anchoring. Doll starts level2/HP38, offense4, defense9, and carries source EXP8/cash10 as roster data; no reward is applied by this bundle. DialogueBox calls `start_battle(0,false,...)`, so the source menu has Bash, Items and Defend. The original boss encounter sound is audio stableID1002; its PCM/bank metadata preserve source rate/channels and one-shot behavior. The preexisting three audio IDs are unchanged. No actual audio-playback claim is made.

## Checked schema and shader operations

Opening remains ENCBTL01 version1; its inherited portrait-position bug is corrected by the same data compiler. Doll uses version2 with the same12 sections and a116-byte Background row. It adds scroll, ping-pong movement, compression, palette resource/speed/source frame divisor and optional explicit fixed-row policy. Unknown schema, fields, nonfinite values, invalid resources, contradictory palette policy and unresolved zero divisors fail closed. No interlace, oscillation/compression modulation or nondefault blending is silently discarded.

The scalar kernel follows the pinned shader order: barrel → oscillation → compression → movement → nearest source sampling → palette replacement preserving source alpha → opacity. Its coefficients, art and adapter policy come from resources. Lamp's existing fused/mapped hot path stays unchanged. Default400×240 expands the viewport at one source pixel per pixel;320×180 remains the reference mode. STRETCH_TILE uses the actual176×172 source texture as its UV divisor, as confirmed by native Control/texture geometry.

## Explicit source palette defect

The pinned Baby BBG enables palette shifting but omits `palette_anim_frame_count`. The source importer does not supply it; no runtime setter exists. The shader divides by the resulting default0. Replacing it with the palette image height4 would invent behavior.

A bounded official Godot3.6.2 GLES2 probe ran on the cloud desktop with llvmpipe. It compared both original layer buffers at four native times against every fixed palette row at identical TIME. All eight original buffers exactly matched rows0 and2, which are identical in the source palette, and did not match rows1/3. The actual uniform reports `null`; the source omission remains in the IR and the binary frame divisor remains0.

A separate, explicit external compatibility policy selects row0 for this exact source bundle and reviewed backend result. Compilation checks the evidence and source hashes; the runtime rejects a zero divisor without an explicit valid row, and rejects a fixed row combined with a positive divisor. This is a documented backend adaptation for undefined source arithmetic, not a portable GLSL rule. It does not prove complete CPU/GPU pixel equivalence or the full original game's rendering.

Evidence and all eight PNGs are in `reports/doll-background-reference/`, including source hashes, official release archive SHA512 verification, engine executable hash, raw renderer/audio initialization log and native comparison JSON. The dummy-audio warning does not affect this isolated image probe. `tools/prepare_doll_background_reference.py` recreates the fixture in a fresh build directory; it executes no original game scripts.

## Verification and limits

Dedicated tests: `tests/test_doll_entry_asset.py`, `tests/doll_entry_data_tests.cpp`, and `tests/test_baby_background_kernel.py`/`baby_background_kernel_tests.cpp`. They check deterministic data compilation, original lossless image pixels, exact source roster/geometry, source-policy fingerprints, staging, every truncation, CRC-correct malformed data, all newly admitted shader operations and source-alpha preservation. Historical host and ASan/UBSan checks did not include LeakSanitizer.

The kernel worker checked 6,061,057 extended-path pixel comparisons and retained frozen Lamp output parity, including mapped texture bytes. A real ARM renderer object compiled successfully; that is not ARM execution. This resource work did not run Azahar or hardware, and does not establish Baby frame rate. The parent integration/build/emulator report is the authority for the final runnable checkpoint.

## Parent portrait entry correction

The party portrait is a child of the information plate, itself under the animated PlayerInfo control. Previously the compiler emitted this parent translation for the plate and labels but omitted it for the portrait, exposing a second Ninten before the transition had advanced. Both entry bundles now apply the original PlayerInfo Y track before the later portrait show tween. Independent official Godot3.6.2 Control/VBox samples confirm portrait Y193→116 over0–0.3seconds; the subsequent show tween remains116→96. The original battle timing reference was not rewritten.

The enemy transition offset intentionally keeps the source Sprite.offset alone: BattleSystem._add_enemy duplicates CharacterSprite then replaces its position with the actor screen position. Adding the prior CharacterSprite child position would be an incorrect9-pixel shift.

## CPU background optimization after the first device run

The first actual ARM/Azahar run exposed roughly301ms of Baby CPU composition per frame. The initial general scalar implementation was functionally correct within its stated comparison scope but too slow to advance the entrance normally. This observation is separate from the earlier Lamp performance result.

The revised CPU path prepares a checked fixed-palette color table and preblended pair colors, fuses the two eligible layers, caches source-coordinate terms, and can write directly into the existing mapped texture. It preserves the source canvas and sampling resolution. Eligibility is determined entirely by decoded layer coefficients; unsupported combinations retain the original scalar mechanism.

The later guarded trigonometric optimization prepares phase-independent terms. It skips a literal scalar cosine only when a conservative error interval, including float argument/addition/multiplication rounding and texture-boundary padding, stays inside one exact sampled texel. Samples near boundaries and large phases use the frozen literal formula. There is no reduced-resolution image, quantized time, or unconditional nearest-neighbor approximation. The original scalar kernel is retained as a separate test oracle in `tests/baby_background_scalar_reference.hpp`.

Host tests compare the actual Doll resource at1,041 times per display mode, including mapped texture bytes. Host timing ratios and successful ARM compilation do not establish emulator or hardware frame rate; the applicable checkpoint measurements must be considered separately.
