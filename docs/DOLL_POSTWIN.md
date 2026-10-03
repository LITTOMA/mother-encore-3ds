# Original Doll post-win content and source evidence

The pinned `doll_defeated.yaml` now compiles all eight original phrases into the
external Room resource. The data handoff includes the original Doll jumps and
movement, Ninten turns, Mimmie walk/shake/dot emote, Minnie teleport, camera
changes, sound and music policy, two manually acknowledged text phrases, and
ordered actor restoration. The last text phrase retains all three WAIT-delimited
segments and the dynamic player name. No melody, Minnie tutorial, juice or
Pillow encounter is executed by this content.

The first phrase sets `doll_defeated` and clears `poltergeist` in the original
DialogueBox handler order, after the camera idle yield. Phrase1 sets
`pillow_attack`. These are authored script effects; the battle outcome does not
pretend that they happened before the script begins.

Program records gain a checked source-path string after their existing four
fields. The three paths match the original relative dialogue identifiers, so
the runtime can resolve a post-win request through data. Program2/stable3 ends
with DialogueDone and the original0.5-second camera-return duration. Existing
Lamp and Doll-attack programs keep their commands, battle terminals and zero
nonterminal DialogueDone durations.

Minnie uses the actual `Objects/npc3` source node, body11,95×100 texture and
5×4 animation grid. Her initial position remains(472,88); the post-win teleport
uses that same authored position. NPC `event_positions` is an initialization
mechanism and has not been repurposed as an immediate flag-change teleport.
Existing Mimmie and Doll body bindings remain10 and15. Runtime restoration must
move each original collision body and interaction/view shapes with its NPC.

Resource indices0–27 stay fixed. Minnie appends at28/stable29 and SMAAAASH at
29/stable30. Actor indices0–3, all134 old commands, old program identities and
105 collision polygons are preserved. House dialogue IDs4/5, Minnie NPC/profile4
and texture10 append after the old identities. The prefix receipt checks the
old arrays and explicitly records one required semantic correction: Doll Idle
clip43/stable44 gains flag4 because original Actor.play_anim emits
finished_action immediately when playing its idle animation. The old attack
only plays Float, and actor initialization does not invoke that play event.

The native dot emote contains ten keys, so the bounded clip capacity expands
from8 to16. All native Animation values retain exact float encodings in the
receipt. Actor jumps also call the source blend-position operation, and multiple
timed turns need independent timers sharing the actor direction, as in the
original coroutine code. These mechanisms are executed by the shared runtime;
no game-content C++ table is introduced.

## Evidence and limits

- `reports/doll-postwin/` contains the original Godot3.6.2 parser/animation
  receipt, reviewed handler order, preserved-identity receipt, Python compiler
  negatives and pack/source checks
- `reports/doll-postwin-actions-reference-3/` contains2,150 original Actor
  physics samples. The test-only generated fixture covers Doll movement/jump
  concurrency, Mimmie walk/jumps and turns overlapping at five boundary timings
  plus an immediate same-target second turn. Timing is an explicit mechanism
  probe; it is not a full DialogueBox replay
- `reports/doll-postwin-cleanup-reference-3/` contains600 native samples with
  animation playback clocks. Actor.update_npcs copies position/direction
  immediately, then waits for frame_changed and one idle before showing the
  original NPC and removing its actor proxy
- Constant-frame idle clips still emit native frame_changed at their loop
  boundaries. Doll completes at probe frame15; same-direction Minnie/Mimmie at12;
  changed-direction cases at3. An earlier10-frame observation was too short to
  establish completion. The first extended run retained its raw result and
  failed harness assumption; no runtime fast-forward is justified by that
  shorter observation
- Both round packs retain their previous byte identities after dependency
  provenance is refreshed. The audio bank includes source SMAAAASH under stable
  ID30. This verifies extraction/conversion, not DSP audibility

Current source-derived Room is44,032 bytes and House is8,780 bytes. Full runtime,
sanitizer, ARM and package acceptance belong to the final integrated checkpoint;
these source receipts do not establish emulator visuals or physical3DS results.
