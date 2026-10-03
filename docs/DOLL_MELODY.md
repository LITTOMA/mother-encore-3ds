# Doll melody and original exit guard

The next main-route slice continues after the delivered eight-phrase Doll victory
scene. Interacting with the original Doll NPC starts the three source melody
phrases. Mimmie's exit guard remains active until the exact `doll_melody` command;
her two phrases share one dialogue box. Pillow/Minnie/running tutorial is an
optional earlier branch, not a prerequisite forced into this route.

The Room/House/audio/effect resources contain the reviewed command stream,
original translated text, actor bindings, music, flags and effect parameters.
No game-specific fallback table was added to C++. Source identities and exporter
checks are recorded in reports/doll-melody.

The melody wait lasts four seconds. The final phrase begins printing immediately
while its concurrent 0.6-second timer blocks acknowledgement; it is not an
additional pre-text delay. House music/effect calls retain deferred ordering,
while melody music, heal sound and the flag follow their source command phases.
The original Doll remains the talker; only the party leader is an Actor proxy.

A separate dialogue-music voice preserves the distinction between the original
MusicArea owner and direct dialogue music. Stopping an area resource does not
stop the melody, effects or victory jingle. Four concurrent host NDSP-double
voices and targeted stops are covered. Physical DSP playback remains unverified.

The source effect uses a checked external 532-byte package, pixel-center tiled
UVs, original shader math and AnimationPlayer timing. Native 400×240 extends the
source320×180 coordinates one-to-one. The effect is composited after the normal
world/Above foreground, then its lifted leader and original NPC are drawn in
source order. Controlled original GLES2 samples match 1,459,200 ARM kernel pixels
and 4,377,600 actual RGBA8 GPU readback pixels. These scoped samples do not claim
universal shader/TIME or physical hardware equivalence. Isolated emulator400×240
kernel median17.094ms plus upload2.927ms does not meet a60FPS frame budget.

Area contacts now retain source phases: observe previous physics position,
deliver contacts after current physics/idle processing, then resume a Door's
explicit next-idle wait. Player.pause masks Door contacts while retaining
CutsceneArea contacts. Openable-door exit callbacks continue during paused warp.
The native battle_to_ov signal clears previously armed story processing, and
restored player positions replace stale contact observations. Exact edge contact
remains inclusive; no collision clamp or artificial guard priority was added.

Verification includes17,428 melody/guard assertions in both viewports, the
14-case native rounded-player guard probe, four source door intervals and
unchanged actor frame-signal restoration assertions. All44 host and44 ASan/UBSan
groups pass (LeakSanitizer disabled). Reports/melody-integration contains results.
Integrated Azahar ARM gameplay passed the full fresh opening→Lamp→Doll→melody→
upstairs hall path. The effect ran around30FPS and normal world around60FPS in
this scoped diagnostic; Doll was around15FPS. These measurements came from an
instrumented QA binary, not a new production performance acceptance.
No new visual capture or physical-hardware verification is claimed here.
