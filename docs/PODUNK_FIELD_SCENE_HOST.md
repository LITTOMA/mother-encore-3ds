# Complete source lifecycle roster and checked SceneHost

`content/podunk-scene-lifecycle.json` contains all 2157 script-bearing nodes,
ordered by the complete original 8686-node postorder Ready traversal. It also
stores 13 FlagLandmark profiles, 19 inherited FlaggableObject profiles, the
actual AreaRoom parameters, source map/visit tables, 178 normal flag keys and
the DebugStartPos world position. No authoring JSON is loaded by the game.

`tools/field_scene_host.py` compiles independent `ENCFSCN1`, format 1, family
`0x454e001c`, capability 1. Compilation requires the checked authoring IR,
semantic review, upstream inventory and their pinned upstream source files.
No ignored reports or private native export is needed for clean-clone compile.
Extraction alone requires explicit complete native/source exports.

Godot 3.6.2 `Node::_propagate_ready` runs children in source sibling order before
their parent. `Node::NOTIFICATION_READY` calls `call_multilevel_reversed`; the
GDScript implementation recursively invokes base `_ready` before derived
`_ready`. The audited FlaggableObject → ItemHolder → Present/DroppedItem chain
does not explicitly invoke `._ready()`. Native engine source was inspected,
without running a behavior probe.

The SceneHost borrows immutable, cross-bound Grass/NPC/Enemy/Tint/Sprite packs
and initialized actual consumers. All RNG consumers must be initialized with
the same authoritative SourceRandom before this host is configured. Typed
descriptor IDs, node paths, scene pin and Ready ordinals are checked. Only an
actual successful Ready registers script SHA and capability. Geometry admission
uses that exact node receipt; no caller-provided generic script approval exists.
Registry family is the lifecycle schema; capabilities use `1 << FieldSceneRole`.
Grass retains its existing pack family `0x454e0017`.

The actual shared host signal bus owns synchronous connection order. Required
callbacks read/write the save-backed dictionaries, install flag/area/switch
listeners, queue source deletion, change actual visibility and access genuine
inventory/party/context state. It must remove connected callbacks when the
source node is freed and before the SceneHost is destroyed. An unavailable
callback rejects configuration; successful callbacks cannot be empty approval
stubs. FlaggableObject empty keys use the checked current scene name and source
node name. Normal flag setters ignore unknown or absent dictionary keys; object
flag setters insert and honor the source emit option.

FlagLandmark checks flags before connecting the flag signal. A queued deletion
retains the original visibility and physics state. `commit_deleted` is called
only after the actual deferred boundary; it then removes the geometry subtree
and returns the map gate as Deleted. AreaRoom initializes switches, sets visit
flags with synchronous emission, and applies source flyingman party membership;
map override and area-left region comparisons use binary source tables.

FlaggableObject's inherited Ready and flag/reset methods are implemented, but
ItemHolder/Present/DroppedItem remain pending for their inventory/animation
mechanisms. Startup stops at the first missing source adapter and preserves its
cursor. Failure after a callback begins poisons startup, so retries do not
repeat RNG or side effects. This resource never certifies scene compatibility.
The Dandelion Ready/factory bridge (role 12) executes all 135 original spawners;
its particle body interaction remains explicitly pending. The next unsupported
leaf is Door at Ready ordinal 3626, node `Objects/Doors/NintensHouse`. The full
unresolved roster remains visible. The separate Emotes bridge (role 11) retains
all 71 original embedded script instances and exact source proofs.

Verification is source format admission and strict GCC/devkitARM object
compilation only. Manual positive/negative case source exists but has not run.
No emulator, hardware, sanitizer, 3DSX or CIA conclusion follows from this slice.
