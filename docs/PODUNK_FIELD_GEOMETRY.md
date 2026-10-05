# Full Podunk collision hierarchy

`tools/field_geometry.py` compiles the reviewed complete Godot 3.6.2 native
scene export into `romfs/data/podunk.encfieldgeometry`. The offline source IR
is `content/podunk-scene-geometry.json`; JSON is never read by the 3DS game.

This independent geometry capability covers all 902 non-TileMap collider
owners and 941 CollisionShape/CollisionPolygon nodes, including 158 disabled
nodes and one explicit null shape. It retains 446 analytic rectangles, 444
analytic circles, all 50 original solid polygons and their 294 actual native
convex parts, without approximating circles as polygons. The schema also
distinguishes capsules, segments, concave segment arrays, rays, and lines;
there are no such direct source instances in this scene.

The complete official-engine export already records actual PhysicsServer
shape-owner decomposition after safe native tree entry. The compiler uses
those parts and their order directly. A supplemental native export provides
local transforms; every world transform must match the complete export.
Both post-entry owner transforms and pre-entry cached transforms are retained.
Ancestors, source node order, Ready order, script SHA, visibility, pause mode,
disabled/one-way state, masks, Area monitoring/monitorability, native gravity,
audio-bus override, safe margin and static velocity are typed data.

`FieldGeometryView` validates version, family, capability, caller-provided
scene identity, CRC, exact directory spans, hierarchy, source string spans,
shape ownership, analytic parameters, polygon convexity, numeric bounds and
native/local transform agreement. Failed loads preserve the previous resource.
The checked source scene string permits cross-binding against Room/Map data.

Geometry admission cannot activate the scene: all 394 script-bearing nodes
in this relevant hierarchy remain pending. The SceneHost must supply their
actual adapters, live transform updates and deferred deletion. Canvas hiding
does not disable physics. `owners_for_layer` supplies source ordered candidate
owners; it is not a substitute for a native narrow-phase solver or pending
script resolution. No generic Godot script interpreter is introduced.

All raw exports and build diagnostics remain outside the repository in the
private working directory. The manual parser cases exist separately and are
not invoked by development or automatic CI. Source-format admission is distinct
from emulator or hardware gameplay validation.
