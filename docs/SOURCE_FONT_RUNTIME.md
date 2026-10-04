# Source EBMain runtime extension

The source-font runtime maps the four upstream EBMain resource identities for
13 catalogue locales. The locale selector currently enables English and Simplified
Chinese; atlas coverage does not establish complete runtime support for every locale.

## Integrate

One shared `encore::ctr::SourceFontRenderer` owns the active role and outlives all non-owning BattleRenderer attachments. Before entering any C3D frame:

1. `load_catalog_at_safe_boundary("romfs:/fonts/source-fonts.encfont", "romfs:/fonts", error)`.
2. `select_font_at_safe_boundary(locale.font, error)` using the external locale's original `.tres` resource identity.
3. `admit_selected_font(error)` admits the complete selected face. Alternatively `admit_text` admits just required pages and diagnoses missing source glyphs. It accepts newlines as layout, not glyphs.
4. `battle.attach_source_font(&source_fonts)` for each relevant renderer.

Measure House text with its original ASCII metadata when `source_fonts.handles(cp)` is false, and `source_fonts.glyph_advance(cp, result)` otherwise. In particular, source EBMain_la fallback supplies U+005E while the legacy ASCII atlas does not; replacing all ASCII advance callbacks would change legacy English behavior. `BattleRenderer::text_height(legacy_height)` returns the nonlegacy selected face height and otherwise retains its argument. Native metrics happen to measure 12 pixels for each of these four source faces.

Call `begin_frame()` before C3D_FrameBegin and `end_frame()` only after C3D_FrameEnd to enable mid-frame mutation guards. No text draw performs file IO, texture allocation, eviction or release. Complete face admission removes repeated draw-time admission needs. At a locale transition, after FrameEnd, select/admit the new source face and handle admission failure before committing visible text; restore the prior face if a locale transition must be rolled back. `reset_at_safe_boundary(error)` releases all font textures after C3D_FrameSync. Reset before C2D_Fini/C3D_Fini and destruction. Do not destroy an owner while a frame references it. Stable heap-owned texture addresses remain valid across vector growth.

## Budget and data

ENCFONT v1 holds checked source identities, contiguous face/page/glyph ranges, native Godot advance/ascent/height and tight raster bounds. Header length/CRC, counts, scalar ordering, UTF-8, finite metrics, source-path hygiene, atlas bounds, page shape and resident budget are checked. Load failure is atomic. Texture pages are independently length/CRC-checked before import, then texture format and bounds are checked against metadata.

- 4,120 glyph records, 148,979 metadata bytes.
- Latin and Japanese: one 256×256 LA8 page each, 128 KiB admitted.
- Korean: three pages, 384 KiB admitted.
- Simplified Chinese: five pages, 640 KiB admitted.
- Hard admission maximum: 2 MiB. Only the selected face is admitted; other faces' texture files remain on RomFS. Full active-face admission is currently smaller than the hard budget, so gradual gameplay cannot exhaust it.
- Latin ASCII always uses the original atlas and original drawing path. Its canonical SHA-256 remains `fe8353da53e311e53d3e0b861e38376cf946f4f1e93c777be47f65ed0e25cf39`.

All visible catalogue values (with empty-translation English fallback), all locale display names, and ASCII are considered. Recognized BBCode tags and newline/tab C escapes are excluded from glyph requirements. Available House-bound keys and language labels are packed first; the remaining catalogue follows. Supplementary Unicode scalars are supported, including the source Japanese musical-note scalar U+1F3B6. Pages use lossless 8-bit alpha and luminance, not compressed/thresholded glyphs.

## Source fidelity and exclusions

The generator checks the pinned checkout commit and every reviewed source hash, then loads the source `.tres` resources in an isolated minimal project under official local Godot 3.6.2. Every supported glyph advance is independently matched against the chosen source fallback font rasterizer. Source fallback order is parsed from the reviewed resource. Offsets use native ascent and the selected font's baseline bbox. Upstream is never imported or written in place.

Genuine absent source scalars are omitted and diagnosed explicitly. Native Godot also returns zero advance for these pairs:

- EBMain_la: U+1E3F and U+200B.
- EBMain_ja/ko: U+200B.
- EBMain_zh_cn: U+200B and U+2066.

No replacement square, system font, transliteration or invented glyph is substituted. This is only the EBMain role. Bottle/outline/other role changes, source grammatical macros, shaping/bidi beyond the original per-scalar mechanism, layout acceptance across all 13 locales, GPU raster comparison, package construction, emulator interaction and hardware performance remain outside this lane. The selector currently enables only the English/Simplified-Chinese slice; other locales require separate grammar and presentation validation.

## Font licensing

The source font manifest retains TTF fingerprints and embedded notices. Font
portions preserve their independent terms: Fusion Pixel and Galmuri portions
retain OFL; Nintendo-source glyph rights remain unconfirmed. Full copyright,
OFL texts, reserved-name conditions and source limits are documented in
[the font notices](licenses/SOURCE_FONT_NOTICES.md). Project MIT does not
relicense the fonts.

## Reproduce and focused checks

Use the existing official SDK environment. Run the generator with explicit `--source` (read-only upstream), `--catalog` (native-localization JSON), `--godot` (official 3.6.2), and `--tex3ds` arguments; default outputs are `romfs/fonts/` and `build/multilingual-fonts/`. The checked recipe is `content/source-fonts-review.json`. Reruns reproduce metadata/texture bytes for the same input and toolchain; the source manifest records exact input/tool hashes.

Focused tests compile with C++17, `-Wall -Wextra -Werror`, run the real catalogue plus malformed-input negatives, and check all glyph lookups, four remaps, UTF-8 rejection, missing-source rejection, longest-line widths, and atomic load/selection failure. The historical focused ASan/UBSan checks did not include LeakSanitizer. ARM compiles exercise both the core and actual source-font/BattleRenderer headers against the official installed libctru/Citro2D. These checks do not constitute a full host suite, linked game build, installable package or emulator/hardware validation.

### RomFS staging contract

`tools.source_fonts.stage_files(root)` accepts a RomFS directory and returns `Path('fonts/...') -> bytes` for the pack and ten pages. Offline provenance is read from `content/asset-receipts/fonts/source.json`; redistribution notices are centralized under `licenses/`. The tool validates the pinned upstream commit and every reviewed source hash, exact generator/catalog hashes, pack header/count/CRC/hash/length, and each page's hash/length/CRC. It does not generate assets or write files. A changed catalogue or generator requires regeneration rather than silently staging stale assets.

The earlier bounded regeneration used the 3,879-row catalogue (79 derived rows included, 281 bindings). All glyphs, page bytes and font-pack bytes remained identical to the previously tested assets. Its historical staging check admitted 13 files, including provenance and a duplicate license. Current staging admits only the pack and ten pages; provenance stays outside RomFS and licenses use the central notice collection. Source, catalogue, metadata and page validation remain mandatory.
