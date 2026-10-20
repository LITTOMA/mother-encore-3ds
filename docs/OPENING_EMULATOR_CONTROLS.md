# Opening input regression on the genuine build

Historical opening scenarios used B for running. The checked FieldMenus factory
defaults now bind Crouch/Run to X. Their route also approached the locked door's
trigger sideways while walking; the original door evaluates its ram condition
on `body_entered`, requiring a running player facing north.

The three opening routes that ram this door now step back before running north
with the factory binding. All previous waits, progression targets, repeat limits,
projections and outcome assertions remain unchanged. Four tests check the menu
resource, source condition, ordinary approach and preservation of old outcomes.
Old input files and both failed runs remain in the evidence.

The actual 0f203b2 3DSX from genuine artifact 12260126937 passed two independent
cold runs of the 90-checkpoint Chinese route. It uses ordinary New Game/HID input
and the declared START introduction skip; no save fixture, memory writes,
injected flags, items, position or RNG. Stable gameplay projections match
exactly. The route covers START/PSI, Lamp victory, the physical door ram, Doll
victory/learned PSI, melody/Carol/Dad and House–Podunk–House–Podunk transitions.

The receipt verifies the 264,975,024-byte 3DSX with SHA-256
`723537dcc1d69289ff7e59e8459847f62a20cb16e260e999b5a98eecbdf3c508`.
The read-only observer checks DSP availability and the scenario's explicit music
consumption assertions. Title/Poltergeist/house stream observations and field
region ownership are retained; these do not establish perceptual audio quality.
Successful Doll-room and outdoor screenshots were visually inspected for scene
and character continuity, without pixel-equivalence claims.

The original comparison uses the pinned Pause/PSI method oracle and reviewed
source contracts. It is not a full parallel original-game playthrough, complete
introduction playback, CIA installation or hardware acceptance. It also does
not activate the newer detached dungeon consumer. Raw failed/passed logs,
observations, inputs, source/binding proofs and cleanup receipts are under
`reports/opening-emulator-controls/ordinary0f-1`. Portable profiles are removed
after each run. The unused downloaded CIA was receipt-checked and retired;
the real 3DSX and receipts remain available.
