# Runtime locale architecture, first playable checkpoint

The source language tables are no longer an irreversible English-only content projection. `localization.enclocale` contains the 3,800 original CSV key rows, 13 original locale slots, original file/line provenance, plus 79 explicitly derived current-slice formatting records. `native-localization.json` retains raw CSV values before the source importer's C-escape processing. The checked sidecar stores original source keys and exact expected legacy values/token signatures. Unknown or changed bindings reject; actor paths, script IDs, RNG identities, saved names and gameplay rules are never inferred from translated display strings.

The pinned upstream `global.gd` lists 13 enabled languages. CSV `zh_CN` maps to source `zh_Hans_CN`. Native Godot 3.6.2 probes verified exact locale lookup, regional-language lookup, empty-value English fallback, missing-key return, and importer C-escapes. The ten old compiled `skills` resources are inventoried but unreferenced by this upstream project, so they are not ambiguously overlaid onto current CSV keys.

This acceptance exposes only English and Simplified Chinese through L/R on the title's lower-screen adapter. The other eleven resource sets remain retained, with an explicit native-selection blocker. Polish/Russian/Ukrainian need the source custom-name declension mechanism; the other languages have not completed native presentation acceptance. Preference decoding applies the same admission rule and cannot bypass it. Presence in the catalogue is not a claim of playable-language coverage.

Language is an independent application preference at `language.encprefs`, using a stable locale code, version, length and CRC. It uses exclusive temporary creation, read-back, a previous-value backup and checked commit. It never changes per-slot session data, names, favorite food, flags or save compatibility. Corrupt/unavailable preferences produce a visible diagnostic rather than silently asserting success.

## Real consumers

- Source-remapped Chinese title option images; only eight small option textures differ. Source English title/background assets remain unchanged. Title textures release before naming and lazy reload on cancellation, preserving the packaged Teddy fix.
- Naming prompts and command labels, startup settings rows/options/preview/final confirmation.
- All 49 current House phrase spans, with translated speaker names, source delays, cash tokens, choice labels, WAIT/newline controls and source-name clipping. Changed translated WAIT segment counts are presentation-owned, separate from source scheduler identities. Chinese inline WAIT does not invent a newline.
- Current Lamp/Doll/Pillow battle text and command labels, plus initial item name/description. Original source templates and actor contexts are explicit data; runtime nicknames are substituted as values, never parsed as template code.
- Strict common UTF-8 decoding, codepoint-based battle/world pacing, codepoint-safe prefixes, source font selection and source-glyph validation. RichTextLabel's additional long-run wrapping is retained for localized world dialogue; native wrapping probe is recorded.

The source EBMain font service has four resource identities with native Godot metrics. Only the selected face is GPU-resident, admitted outside active frames; 128–640 KiB, bounded at 2 MiB. English ASCII stays on the unchanged original atlas. Genuine missing upstream glyphs are rejected, with no invented replacement. See SOURCE_FONT_RUNTIME.md.

## Explicit remaining scope

This is a first native localization slice, not full multilingual game completion. The naming input keyboard and allowed saved-name domain remain the established Latin/ASCII subset; translated input methods/default-name sequences are not claimed. The save/LOAD card's Bottle/outline text, other font roles, full Settings menu, original Introduction and untranslated development diagnostics remain outside this checkpoint. Podunk scene expansion must add explicit source/span bindings before a non-English phrase can run. No automatic translation is generated.

Original gameplay packs and ASCII atlases are unchanged. New executable presentation services and checked external locale/font/title data are independently staged. No hardware, audio-audibility, complete game playthrough, 13-language layout QA, or whole regression-suite pass is implied by focused checks.
