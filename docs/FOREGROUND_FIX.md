# Original house foreground restoration

The reported bedroom position (485,471) is the original collision contact, not an escaped physics body. The source wall begins at y480 and the player hull extends nine pixels below its origin. An independent Godot3.6.2 probe using the original source polygons and player hull matched the native solver exactly for360 frames across seven focused routes, including continued downward pressure and diagonal contact.

The display defect was the missing Above TileMap pass. The original scene parents the player under Objects and draws Above afterward. Its WallBlack atlas tile at (480,464) masks the player's lower pixels. The native renderer previously imported14 Y-sorted Objects overlays but omitted all276 Above tiles.

The repair exports the original276 tiles with their original atlas UVs and alpha corners into independent room content. RoomOverlay flag1 selects a fixed foreground pass after sorted objects and actors; flag0 retains the existing Y-sort behavior. Unknown flags are rejected. Existing Resource identities and physics geometry are preserved. No coordinate clamp, invented obstacle or replacement rectangle is added.

Validation includes source-cell/atlas coverage, original alpha patterns, checked content decoding, collision-contact regression and real3DSX/CIA compilation. Internal emulator screenshot observation was blocked by automatic review despite the user's clarification permitting internal QA. Therefore this patch has no new visible emulator acceptance claim; no screenshots are delivered.

The separate periodic shaking after Lamp victory is original behavior: Lamp starts Room Shaker, battle suppresses its visible callback, and Doll attack's last phrase explicitly stops it. The upper-right hall door is the unported Mom-room route; B returns to the safe point. The sister room is the left door, opened by approaching upward while holding B to run.
