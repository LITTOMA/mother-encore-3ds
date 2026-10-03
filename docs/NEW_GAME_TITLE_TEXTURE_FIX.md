# New Game title-path texture lifetime fix — 2026-10-03

The earlier GPU1:1:0 3DSX (fa33cd5d…) reproduced a Teddy
texture failure from the normal title screen. Teddy and all other resources were
present and unchanged. The prior startup-settings diagnostic omitted title-art
loading and therefore did not cover this resource-lifetime overlap.

The fix releases title GPU textures before loading the mutually exclusive naming
renderer, while retaining checked title/restore data. Returning to title reloads
its art. A failed naming preparation clears pending state and restores title.
Importer diagnostics now separate file errors and metadata rejection from the
SDK decode/allocation failure, with free linear bytes on 3DS for the latter.

## Final artifact and actual gate

- GPU mode: 1:1:0; source defaults remain 0:0:0.
- Production 3DSX SHA256: `79d900e8aaa2cc6e127be75f73983ca7a387e244db79c413bb6a7589b86fd61a`.
- Real ARM/3DSX and CIA packaging completed. CIA was not installed or run.
- Parsed the exact 3DSX embedded RomFS: all171 files, including127 T3X files,
  equal checked staging and the prior resource inventory byte for byte.
- Focused progressive-loader host and ASan/UBSan tests pass; LeakSanitizer disabled.
- Azahar2126.1.2, OpenGL, disposable generated New3DS-mode profile. The exact
  production binary, without generated entry/input or diagnostic main, passed:
  title→New Game→cancel→title→re-entry→Ninten/Ana/Lloyd/Pippi/Teddy/food→settings
  speed/flavor/prompts→No retained restart→Yes→bedroom→live movement.
- Settings B returned to retained food; Mint preview cancellation restored Plain.
  Medium/Mint/Objects and all names survived the No restart. SELECT after gameplay
  reloaded the title in reference view. No user saves accessed; no `.encsave` created.

No full game, hardware, save/LOAD, audio or performance guarantee is added.
Introduction remains unported; this fixes access to the existing bedroom slice.
