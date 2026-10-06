# Podunk dynamic map consumers

The checked SceneActions resource names six source TileMaps and two Stone
CollisionPolygon2D leaves. `FieldMapSpace` consumes those bindings and the
checked map resource; it preserves local position across removal and deferred
attachment, updates collision layer and mask independently, and queries the
original chunk index with translated bounds. Drawing uses live canvas position,
parent, visibility and source tree order. It does not rebuild every map cell
each frame or approve an incomplete scene.

`FieldGeometrySpace` removes a leaf polygon's shape owner immediately when it
is detached. Attaching to a TileMap creates no CollisionObject2D shape owner;
attaching back to the source Area restores the owner and appends native shape
indices. SteppingSounds receives actual live convex parts, rather than an AABB
or point approximation. Resource identity, exact source bindings and admitted
parent capabilities are checked before mutation.

Area monitoring uses the original bilateral mask rule. Direct space queries
retain their query-mask semantics. The reviewed engine sources are Godot
3.6.2-stable:

- [collision_object_2d_sw.h](https://github.com/godotengine/godot/blob/3.6.2-stable/servers/physics_2d/collision_object_2d_sw.h), SHA-256 `d13620a8658ba4debfdeabc03743f38fb45a7353e9984ac27cef2568ca7287d6`.
- [collision_polygon_2d.cpp](https://github.com/godotengine/godot/blob/3.6.2-stable/scene/2d/collision_polygon_2d.cpp), SHA-256 `66d4445e7becb06c81d81087759d5ee140f8c326042b2925a18e217ff877eb33`.

The core and readers compile with the real ARM toolchain and strict warnings.
Positive and negative manual cases are retained without automatic execution.
The authoritative SceneTree and global deferred queue still need to call these
consumers in the actual Podunk scene; the complete scene is not playable yet.
