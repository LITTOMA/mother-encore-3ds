# Carol, first Dad call, and Record

This stage follows the pinned original main route after the Doll melody. Carol's call starts the Phone's original Ring animation/audio; interacting stops it, plays the hang-up cue and chooses the original dialogue from flags. The first Dad call sets `talked_to_dad` at its source command and queues the entrance blocker for deletion at the end of that scene frame. It does not save, heal or grant money.

Subsequent calls run the checked `dad_normal` graph. The two visible options are Record and Nothing; Cancel uses the source hidden branch. Printing must finish naturally before choices open, including embedded WAITs. The earned-money token reads and clears the earned counter and flag without depositing the amount again. Conditional leader texts retain their original color, delay and newline behavior.

Record opens the original ten-slot menu with checked layout, fonts, palette, activation delay, scrolling and overwrite confirmation. A successful write leaves the slot menu open and remembers that slot for later calls in the current session. The current direct-new-game bootstrap initially selects the first valid one-based slot; original persisted title/settings slot selection is part of the pending load flow. Closing resumes Dad at the saved/unsaved source branch. Save failure leaves the previous slot metadata and flag intact. A corrupt existing slot is reported and cannot be silently overwritten; the lower-screen error can be dismissed with B.

All new content lives in external checked Room/House/choices/save-menu/session/phone resources. C++ implements bounded control flow, callbacks, rendering and storage. Room6 adds explicit forward branch, choice suspension and submenu suspension instructions; House6 adds source delay/newline tokens. Older supported pack revisions keep their original operand restrictions.

## Native storage boundary

Files are written to `sdmc:/3ds/encore-native/save-<one-based-slot>.encsave`. The native schema stores stable identities, current HP/PP, EXP/level, skills, source equipment/key items, money, flags, seen/encountered history, player location/facing and session settings. Derived combat stats are validated against external source rows rather than stored as invented permanent boosts. The first saved file preserves the source's pre-write `saved=false`; the live flag becomes true only after a successful write. Original RNG is not serialized. A future LOAD must reproduce source reconstruction side effects: inventory creation eagerly calls the UID fallback, which randomizes and draws even for a saved UID. This stage does not yet execute LOAD or claim random-stream restoration.

The source-derived template is not a played save. Current scope is the existing single-party, house and initial equipment slice. Unsupported identities, modifiers, status or settings fail validation. NPC transient transforms, running dialogue and menu state are not serialized.

Writes use a verified temporary and backup. libctru replacement is not atomic: it can delete the destination before a failing rename. The adapter attempts recovery and retains verified recovery bytes when needed. This is not a FAT power-loss guarantee.

The original title/Continue flow, applying a loaded snapshot to a fresh scene and death/respawn restoration are not implemented in this stage. Existing files are decoded and domain-validated for slot metadata; that does not claim a playable load path. The external Podunk route remains a development boundary.

## Verification and limits

Source oracles and command layouts are retained in `reports/dad-record` and `reports/house-record-presentation`. Current supported behavior and validation limits are documented in [STATUS](STATUS.md).

Source printing compares 216 frames from official Godot3.6.2, including WAIT, visible glyphs, voice requests and shared RNG draws. Callback integration exercises both400×240 and320×180. Source camera/timing and error paths have host checks; full new gameplay, audible audio and Old/New3DS hardware remain separate verification levels.

The backend has bounded audio lanes, not the original complete spatial/bus graph. Phone spatial attenuation and SaveSelect's bus muffling are not established as equivalent. Emulator NDSP output was unavailable in earlier checks. No screenshot is delivered for this stage.
