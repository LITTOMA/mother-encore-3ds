# Source New Game settings and final confirmation

Integrated development checkpoint. Current final verification is in STARTUP_SETTINGS_CHECKPOINT.md; this does not implement Introduction or establish hardware readiness.

## Scope and source behavior

The six naming fields now continue to the original Settings and final certainty panels. Source labels, layouts, icons, choices, palette values and preview sprites are independent binary resources, not C++ game-content tables.

- Text speed: Fast 0.02, Medium 0.028, Slow 0.035 seconds per character, from `globalData.gd`. Source defaults remain Fast / Plain / Both.
- Flavors: Plain, Mint, Strawberry, Banana, Peanut, Grape and Melon.
- Prompts: Both, Objects, NPCs and None.
- Settings B returns to the pending food field. Submenu B restores the selected value and discards its preview. Confirmation No or B returns to the first name while retaining all pending names and settings. Only confirmation Yes permits startup commit.
- The startup candidate, copied source RNG and UID ledger remain detached throughout naming, settings, preview and restart. No new UID allocation occurs on restart. Failed preparation or cancel preserves the live session and game files.
- Yes still enters the existing development bedroom slice. The original `DoorToIntro` / Introduction story, character/menu transition choreography and title intro have not been substituted or fabricated.

Sources: `Data/NamingSequences/intro.yaml`, `Scripts/UI/NamingScreen/*.gd`, `Maps/Naming screen.tscn`, `globalData.gd`, `uiManager.gd`, `Shaders/MenuFlavors.tres`, `Button Prompt.gd`, `Player.gd`, and the original current House target scenes. Compilers retain exact upstream source hashes.

## Real consumers

1. `HousePresentation::set_text_speed` affects the actual world dialogue loop and preparation of source `CHAR_DELAY` cells. `BattleActionPresentation` and `BattleTextPacer` apply it to actual battle dialogue including victory/outcome text. A/B acceleration and strict source timing comparisons remain unchanged. Existing world tag/control behavior is not replaced. The currently reviewed 81 battle text rows contain no local speed/delay tags; general future battle tag expansion is still unsupported, not silently stripped by this change.
2. The explicit UI-skin allowlist maps original source shader color slots at a GPU-idle boundary. Ten checked UI textures participate: battle/Items boxes, battle plate/labels, world dialogue box and new startup skins. Unlisted character/map/background textures and per-slot Save skins never participate. The non-flavored source Namebox also stays outside the list. Original bytes are retained per matched pixel so default/cancel restores exact existing texture values. The source shader's color-match threshold and palettes are external data. Existing menu disabled-label color and naming ColorRects use the same palette. This does not edit the experimental GPU battle-background renderer or shader paths.
3. House button prompts use the actual supported interaction ray and moving NPC positions, source object/NPC category, selected current dialogue availability, pause/visibility/replacement state and door enablement. The read-only support predicates reuse actual NPC override and phone program selection, so unsupported interactions cannot gain a hint. The settings preview uses original Present/NPC assets and the same filtering.

Prompt limitation: the existing native interaction selector includes implemented NPC, door and phone rectangles, not Godot's general body/area query. Original EventDetector can also hit unrelated static bodies. This checkpoint does not claim general wall/body occlusion or overlapping-collider order parity. The prompt evaluator has a nearer-occlusion input for future integration. Static original A/arrow pose is rendered; Show/Float/Hide/Press choreography is not implemented. A is the explicit 3DS ui_accept mapping.

## Data, persistence and ownership

- `opening.encsettings` (`ENCSETUI` v2) stores source settings/confirmation UI, choices, palettes and the exact skin-path allowlist. The source all-label character-count threshold is also external data: preview animation runs only when every translated label exceeds it; shipped threshold 5 preserves the original rule. Format v1 is rejected rather than using an embedded fallback; capability 1 and save/rules identities stay unchanged.
- `opening.encprompts` (`ENCPRMPT` v1) stores nine current House targets and original prompt/preview resources.
- `opening.encsession` (`ENCNSESS` v3) appends the source-supported setting choices after the unchanged v2 payload. V1 and V2 resource readers remain supported with their original default-only settings scope.
- `ENCSNAP1` save schema, content family, rules revision, initial source snapshot values and opaque inventory UIDs are unchanged. The default snapshot SHA256 remains `a9cfbb498a20cb2bbb4a0a56dc5a87f9f13e37bac7ebbef14c875b67c93e7e9f`. No user save was read or written.
- Unknown speed/flavor/prompt values still fail closed. Successful Record/LOAD roundtrips store and restore the actual supported choices without adding inactive characters to old singleton saves.
- `loading_texture.hpp` owns only sparse pixel mappings for explicitly registered UI paths. Configure once; change selection only after the last frame's GPU work completes. Freeing a sheet removes its mapping; newly loaded battle textures immediately use the selected theme.

## Scoped verification

Focused tests cover pending/cancel/restart/Yes, nondefault Record codec and fresh LOAD, malformed packs, legacy/default bytes, actual speed clocks/RNG, prompt filtering and real T3X palette payloads. These are host and source-data checks. Real ARM object compilation includes the final main and affected runtime files. The final integration gate passed 92 host and 92 ASan/UBSan groups, all three ARM/3DSX/CIA configurations, exact 171-file CIA resource checks and one bounded no-save emulator UI smoke. See STARTUP_SETTINGS_CHECKPOINT.md for exact scope. Hardware validation remains outstanding.

No external upload, archive, backup, publication, screenshot delivery or user save operation was performed.
