# Startup Settings / Final Confirmation checkpoint

This historical 2026-10-03 checkpoint introduced source settings and final
confirmation after all six naming fields. It does not replace current build
validation or imply complete-game acceptance.

## Verified final source

- Main SHA256: `f5dbe8e71c189069790326373089c004bc9b46743f881055d9e82955fdf10f2d`.
- Source/resource manifest: `f4eda57966ea327d74e125f5c5de5670d35e3267bd6f5be1145a88c95be36b13`, 755 files. Production code did not change during verification.
- Music service lifecycle and GPU implementation are unchanged. Region music remains Dormant with no region IO/buffers/voices. Podunk assets are not staged.
- `ENCSNAP1`, content identity/rules revision 7 and default save bytes remain unchanged. Default snapshot SHA256 `a9cfbb498a20cb2bbb4a0a56dc5a87f9f13e37bac7ebbef14c875b67c93e7e9f`.

## Actual output behavior

- Text-speed choices drive world dialogue including existing local delay/control cells, battle dialogue, victory and outcome clocks; original strict timing and acceleration semantics remain.
- Menu flavors recolor only ten source-registered UI textures at GPU-idle boundaries. Actor/world/background/GPU texture paths are excluded. Cancel restores original/default pixel bytes.
- Source Both/Objects/NPCs/None choices drive the actual supported House interaction-ray prompts, current NPC positions and current supported dialogue conditions.
- Submenu B discards the preview. Confirmation No/B returns to the first field with names/settings retained. Only Yes commits the pending startup/session/RNG/UID/owner as one boundary.

## One final aggregate gate

- Host: 92/92, 53.45 s.
- ASan/UBSan: 92/92, 130.11 s. LeakSanitizer disabled because of the known executor limitation; no leak-check claim.
- Real devkitARM ARMv6K build/link/3DSX and makerom CIA for CPU `0:0:0`, standard spans/certificates `1:1:0`, experimental texture `1:1:1`.
- CTRTool extraction: all 171 staged resource files match in each CIA. Expected homebrew retail-signature failures remain; verified content hashes pass. No CIA installation.
- Isolated diagnostic built against the final source/core objects and staged resources, run in Azahar with generated naming entry and bounded consumer probes. Real setup/settings input, preview cancel, No restart retention and Yes-to-existing-bedroom commit were checked. No full gameplay replay.

## Artifact identities

Source flags default off. `dist/encore-native.*` is the explicit standard GPU development build; compatibility and experimental artifacts are separate.

### standard_gpu_spans

- `dist/encore-native.3dsx`: 70322904 bytes, SHA256 `fa33cd5d37691164769ff6e1c7be1567e318dccdc6405e4bcc1b202f159c998b`.
- `dist/encore-native.cia`: 70566848 bytes, SHA256 `3b08520c0978013bb3895f1e33a28bd04e0c26b0ea404f82c3bd3daa3fd7e7c0`.

### cpu_compatibility

- `dist/checkpoints/startup-settings-cpu-20261003/encore-native.3dsx`: 70299456 bytes, SHA256 `a4d9c35751ad661b53c2c40df96c52ae5d6da30a9c081f314c6875ea047f2f58`.
- `dist/checkpoints/startup-settings-cpu-20261003/encore-native.cia`: 70550464 bytes, SHA256 `363fe8609473ee4ed582115f6df2c07bbb5e28edaf42dd1d462c0f478dfc1e20`.

### texture_experimental

- `dist/checkpoints/startup-settings-texture-experimental-20261003/encore-native.3dsx`: 70330960 bytes, SHA256 `5200647b8e2ad5ec211ba87dbc2d7096136b63972ba8a7191e1e885209d0c143`.
- `dist/checkpoints/startup-settings-texture-experimental-20261003/encore-native.cia`: 70570944 bytes, SHA256 `d3f334cf101f8b8c99805f7af5da161b4269058117fc6e9f955110da61561b44`.

## Limits

- Development checkpoint only, no stable/public release
- Original title intro, DoorToIntro/Introduction and source menu transition choreography remain unported
- House prompt ray uses the existing supported selector, not general Godot wall/body/area occlusion or overlapping-collider ordering
- Prompt art is the original static visible pose; Show/Float/Hide/Press animation remains unported
- No full gameplay replay or full title-to-New-Game acceptance; generated naming entry and bounded isolated consumer probes are identified in smoke receipt
- No user-save IO, CIA installation, audio audibility or Old/New3DS hardware validation
- No final-binary GPU readback or performance benchmark; source palette payload equality is not GPU readback proof
- Experimental Pillow texture hardware sampling remains unqualified; exact-span/CPU fallbacks retained
- Old3DS whole-process peak/free-heap margin and historical Doll-postwin Azahar issue remain unverified
