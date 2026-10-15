# Postwin NPC restoration ownership

Pinned original: 7d9246600fffe518408f5830d4848635019005a3 (Act2 0.4.1.0). Original actor.gd:249–278 copies the retiring Actor pose, waits for frame_changed and idle_frame, and queues deletion of that individual node. A later Actor replacing the same NPC is a separate lifetime.

The native House restoration wait previously remembered only an actor slot. Mimmie's pending attack restoration could finish after doll_defeated rebound that slot, releasing the new Actor before its (-16,0), (0,-72) steps. She remained at (128,160), with collision blocking the bedroom exit.

Each checked BindActor now allocates a monotonically increasing binding revision. Restoration completion requires that identity and a pending request. A rebind cancels the previous request/wait, including deferred controlled-player cleanup. Zero, unknown, stale, future and duplicate completion requests fail without changing the current owner. Revision exhaustion fails closed. No game content, generated resources, rule or save schema changes.

Local MSVC clean shared-core build passed the 55,171-check playable opening: title, actual naming, Introduction skip callbacks, Lamp and Doll victory rewards, original postwin, physical Doll melody/bedroom exit/stairs/Carol/phone route, first Dad, Record to an exclusive real save file, Continue LOAD and checked restored movement. It asserts original Mimmie position (112,88), restored collision and stale-callback rejection. Earlier opening coverage used explicit NPC/phone warps and missed this defect. No such postwin interaction warps remain.

The checked emulator scenario opening-phone-handoff.json extends the natural route with actual inputs through melody, Carol, first Dad, repeat-call joystick choices and opening/cancelling SaveSelect. It does not write a save or exercise LOAD in the emulator. Console build, sanitizer and emulator results will be recorded after their actual completion. Hardware remains unverified.

The extended emulator route initially exposed test-route overshoot into an adjacent bedroom; the hallway waypoint now leaves stopping margin and checks the post-release corridor bounds. SaveSelect is observed with save_phase=Slots and the owning StoryRunning world/House stages; the separate house_paused bit is false during scripted menus and must not be mistaken for input ownership. No gameplay code was changed to satisfy these corrections.

Official Godot 3.6.2 was rerun: all 2,150 postwin motion samples exactly reproduce the admitted test fixture, including Mimmie final (112,88). The corrected shorter physical route passes 54,851 checks in MSVC and Windows Clang ASan/UBSan; the latter retains two interception_win warnings and is not a substitute for Linux sanitizer validation. A new clean core also passed the 67-check Map/Record identity test. Logs and original receipts are retained in build/postwin-npc-restoration.

## Verified console and emulator boundary

Game source 78d92612791ba734f742787b0f0cd1dc7c4249b0 passed genuine ARM/3DSX/CIA build and 323 packaged-resource checks (Actions 38686611003). 3DSX SHA-256: 76794f86ff802ede72c172a6f25a16651e8ab50b30287fc4555c18c3906e56aa; CIA: dc7842a7be38a4553b5229dbd5b860e4eef8d9cad49acf300c360f6f85a78081. Downloaded files were verified.

Azahar postwin-phone-4 passed two fresh cold starts with all 88 checkpoints and identical result projections. This covers normal startup, SELECT/START safety, original Lamp/Doll action victories, 11 XP/level 2, learned PSI selection, Doll melody, physical bedroom exit/hallway/stairs, Carol, first Dad, repeat-call Circle Pad Nothing, opening and cancelling Record, then free movement. It does not write a save or exercise LOAD in the emulator. Original clock-seeded HP differences are recorded and excluded from deterministic progress/menu projections.

The same game passed postwin-map-1 twice, 19 checkpoints each, compared against the original Present methods. That separate route explicitly starts with a checked artificial key-held snapshot; it does not claim natural Mick key acquisition. All four temporary profiles were removed. Original reference comparison remains scoped to the stated original methods/source contracts; this is not an independently run complete original-game playthrough or pixel comparison.

Full Linux Clang ordinary tests and default ASan/UBSan each passed 146/146 at 78d9261 (Actions 38686620514, job 116262837987). GCC completed 160/161 CTest groups; its failed Python aggregate exposed stale fixtures now fixed in dependency #54. The new complete matrix must be rerun after those corrections; targeted 94 source/format, 17 Phone and 17 strict renderer/loading tests passed. Later dependency changes only format a renderer loop and repair tests/provenance; do not relabel the earlier emulator receipt as a run of a later commit. Hardware and CIA installation remain unverified.

Evidence: build/postwin-npc-restoration and build/emulator-regression/postwin-phone-4 / postwin-map-1. Retired b965 binaries were fingerprint-checked then removed (489,907,280 bytes), preserving receipts/notices/logs. Together with prior batches, 1,959,641,942 bytes of superseded outputs were removed. One interrupted owned emulator workspace was separately cleaned; no user saves or private DSP component were changed.
