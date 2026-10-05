# Non-TileMap source geometry consumer

`FieldGeometrySpace` consumes `FieldGeometryView` without 3DS or rendering
headers. It indexes all 1,184 actual native shape parts from the full Podunk
resource in a spatial hash. The checked view must outlive the space and remain
unchanged; a successfully reloaded view requires space reconfiguration.

The SceneHost binds each scripted node individually against its exact stable
NodeID and source SHA, with an already checked execution adapter family and
capability. `bind_script` is an adapter registry operation, not an approval of
unknown GDScript. All relevant scripted ancestors must be resolved before
queries can filter collision layers, disabled state or Area monitoring. Unknown
Ready behavior cannot be treated as empty collision because an initial source
shape is disabled, invisible, outside the query, or on another layer.

The host submits dirty local matrices and actual source state changes using
`apply_updates`. The space propagates matrices through the affected subtree,
updates only its affected shape entries, and removes/reinserts their cells.
Failures preserve the previous space. A queued free changes nothing until the
actual source deferred deletion boundary is submitted. Deleted nodes cannot
resurrect. Visibility does not disable physics. Layer/mask changes and Area
monitoring/monitorability remain separate from shape-disabled state.

Supported narrow phase covers the entire current resource: analytic circles,
rectangles and actual decomposed convex parts. All 444 source circles have
uniform native scale (296 at 1, 148 at 2). Future nonuniform circles, sheared
rectangles, capsules, concave shapes, lines and rays fail closed instead of
being approximated or silently discarded. Convex overlap uses transformed
source edge axes and Circle/vertex axes, with the native float interval test.
This consumer does not implement movement recovery or contacts with penetration
responses; the separate motion solver remains responsible for movement.

Queries return source node order and native part order. `overlap_actor` uses
direct-space target collision layers. `monitoring_areas` uses each monitoring
Area's mask against the actor's layer. `ray` uses native Circle first-root
semantics, rectangle clipping, original convex edges and inverse-transformed
normals. The nearest hit wins; equal hits retain source order. This deliberate
source ordering is not a claim to reproduce Godot's unspecified global
broadphase traversal order in tied ray hits.

Private source-format admission and compiler diagnostics are separate from
manual query cases, gameplay, emulator and hardware validation. No automatic
tests are enabled by these files. This geometry capability cannot mark the
whole scene playable while other mechanisms remain pending.
