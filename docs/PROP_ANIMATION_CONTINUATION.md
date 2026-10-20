# Same-animation continuation and wall timeline observations

The wall timeline probe revealed that `AnimationPlayer.play` continues the
current position when the assigned animation name is unchanged. It resets on a
different assigned name or at the exact finished endpoint. The previous shared
`PropClipPlayer` reset position and key cursor on every request, which could
restart an in-progress property animation. The correction retains both for a
same assigned clip and leaves malformed-request rejection unchanged. There is
no new resource format, content, catalog binding or save change.

The exact fixed official engine implementation was read at
[AnimationPlayer::play](https://github.com/godotengine/godot/blob/3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8/scene/animation/animation_player.cpp#L1183).
A separate original-engine probe covers all11 Lever/Plate/Gate animations and
produces231 observations, including exact position, properties, interruption,
same-name continuation and finished replay. Two runs match. The source scenes'
scripts and signals are explicitly quarantined in the existing numeric adapter;
source animation and node property data are preserved.

Before the fix, current GCC ASan/UBSan compilation reported174 failures across
the repeated trace. The first mismatch is row2, Lever RESET: native position0
versus original0.0002500000118744 after the same clip's play request. The same
unchanged231-row projection now passes2775 checks with leak checking. Existing
Root81 observations/2632 checks, Root414 boundary observations/3959 checks and
switch809 observations/4873 checks also pass with current source. No frozen
expectation was edited. Six Python source/adapter/negative-output checks pass.
Actual Windows Debug metadata registers286 CTest checks and1305 Python methods
(922 aggregate+383 dedicated), with exact method identities. That collection
does not execute Windows native binaries.

The independent Wall Door probe has309 observations in6 cases, also equal in
two runs. It retains native AnimationPlayer and AudioStreamPlayer. A declared
`requested_playing` property forwards every observed write to native `playing`;
the original `_shake` method body calls a camera recorder. The private MP3 is
freshly imported and receives the reviewed original `loop=false,offset=0`
settings. The first attempt copied an upstream import descriptor whose cache
binary was absent; that import failure is retained. No upstream imported binary
is copied to resolve it.

The observations constrain the future typed wall timeline:

- Zero-effective-delta processing samples the single future audio/body key;
  repeated zero advances repeat audio assignments. Method tracks do not run
  on zero delta. A first positive step follows the ordinary key interval.
- Audio property writes occur synchronously. Camera method calls use the
  original deferred queue. A large advance can emit animation completion
  before either queued camera call executes.
- Switching to RESET before flushing leaves the queued0.5/1.0second camera
  calls intact. Repeating an active On request continues its position.
- StayOff/StayOn do not assign audio. Explicit manual advances after animation
  completion can resample properties; an automatic live owner must stop
  processing a stopped AnimationPlayer. Audio command observation does not
  measure PCM playback time or audibility.

Current ENCPCL01 still rejects audio/method tracks. The wall probe is not an
admitted partial wall resource and does not weaken that rejection. Complete
wall timeline data, deferred command ownership, camera/audio integration,
controlled-object callbacks, live scene/contact and save admission remain to
implement. The independent shared random camera slice is documented separately.
No playable Catacombs, emulator route, hardware or complete-game pass is claimed.
Evidence is retained under reports/prop-animation-continuation/source-oracle-1.
