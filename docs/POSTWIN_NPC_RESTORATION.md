# Postwin NPC restoration ownership

Pinned original: 7d9246600fffe518408f5830d4848635019005a3 (Act2 0.4.1.0). Original actor.gd:249–278 copies the retiring Actor pose, waits for frame_changed and idle_frame, and queues deletion of that individual node. A later Actor replacing the same NPC is a separate lifetime.

The native House restoration wait previously remembered only an actor slot. Mimmie's pending attack restoration could finish after doll_defeated rebound that slot, releasing the new Actor before its (-16,0), (0,-72) steps. She remained at (128,160), with collision blocking the bedroom exit.

Each checked BindActor now allocates a monotonically increasing binding revision. Restoration completion requires that identity and a pending request. A rebind cancels the previous request/wait, including deferred controlled-player cleanup. Zero, unknown, stale, future and duplicate completion requests fail without changing the current owner. Revision exhaustion fails closed. No game content, generated resources, rule or save schema changes.

Local MSVC clean shared-core build passed the 55,171-check playable opening: title, actual naming, Introduction skip callbacks, Lamp and Doll victory rewards, original postwin, physical Doll melody/bedroom exit/stairs/Carol/phone route, first Dad, Record to an exclusive real save file, Continue LOAD and checked restored movement. It asserts original Mimmie position (112,88), restored collision and stale-callback rejection. Earlier opening coverage used explicit NPC/phone warps and missed this defect. No such postwin interaction warps remain.

The checked emulator scenario opening-phone-handoff.json extends the natural route with actual inputs through melody, Carol, first Dad, repeat-call joystick choices and opening/cancelling SaveSelect. It does not write a save or exercise LOAD in the emulator. Console build, sanitizer and emulator results will be recorded after their actual completion. Hardware remains unverified.

The extended emulator route initially exposed test-route overshoot into an adjacent bedroom; the hallway waypoint now leaves stopping margin and checks the post-release corridor bounds. SaveSelect is observed with save_phase=Slots and the owning StoryRunning world/House stages; the separate house_paused bit is false during scripted menus and must not be mistaken for input ownership. No gameplay code was changed to satisfy these corrections.

Official Godot 3.6.2 was rerun: all 2,150 postwin motion samples exactly reproduce the admitted test fixture, including Mimmie final (112,88). The corrected shorter physical route passes 54,851 checks in MSVC and Windows Clang ASan/UBSan; the latter retains two interception_win warnings and is not a substitute for Linux sanitizer validation. A new clean core also passed the 67-check Map/Record identity test. Logs and original receipts are retained in build/postwin-npc-restoration.
