# Complete source scene structure

`tools/field_node_tree.py` compiles the original 8,686-node Podunk hierarchy
into `data/podunk.encnodetree` (ENCFNTR1 schema 1, capability 3, family
0x454e003c). The 7,406 CanvasItem nodes retain their actual immediate Canvas
parent, transform, modulation, visibility, draw ordering and notification
settings. Source postorder contains every native node, including nodes without
scripts. This resource never admits a complete scene or a native subclass.

The source export uses official Godot 3.6.2 outside the SceneTree, with original
scripts and connections quarantined. Original TSCN script assignments are
then reconstructed independently from the checked source receipt, including
outer instance overrides. `tools/field_script_bindings.py::actual_scripts`
returns actual bindings, explicit-null provenance, and file SHA proofs. The
original attachment count of 2,157 contains one script explicitly removed by
`VendingMachineExterior.tscn`; the actual script count is 2,156. Native process
groups from the quarantine are supplemented by original inherited method
presence, not taken as proof that quarantined scripts have no callbacks.

Clean-clone compile needs the checked-in IR, review, exporter, source inventory
and pinned upstream files referenced by IR. It needs no `reports/` file or
private full native export. Extraction alone requires explicit `--native`,
`--tree` and `--source` paths. JSON and private exports are build inputs, never
runtime game resources.

## Live structure consumer

`FieldNodeTreeData` validates CRC, schema, capability, identity, complete native
class vocabulary, bounded Unicode strings, topology, owners, sibling indices,
Canvas ancestry, exact postorder, script SHA proofs and finite numeric data.
Failed reloads preserve the previous valid owner.

`FieldNodeTreeRuntime` owns a single live hierarchy, including instances cloned
from checked subtrees. Global runtime ObjectIDs and duplicate-name counters
are supplied by the actual shared host; stable source IDs remain distinct.
`source_object()` resolves original scene nodes. `descriptor()` and
`object_identity()` also identify dynamic clones without assigning them a
second saved source identity. Runtime IDs are not copies of Godot's numeric
ObjectIDs from a developer export.

Entering uses original parent-first enter stages, then child-first Ready in
source child order. Exiting processes children in reverse order and scripts
derived-to-base. Re-entering retains Ready-once state; `request_ready()` affects
only the requested node. Node's Ready calls its inherited script before native
derived-class Ready (for example Timer autostart). Typed handlers must preserve
this division: ReadyScript calls the actual source adapter once; ReadyNative
performs derived native subclass behavior, not a second script Ready.

Every native/script instance requires a bound actual adapter receipt and actual
lifecycle/signal handlers. Missing handlers stop the retained cursor. No
default no-op script, unknown material, blanket class approval or second random
stream is provided. A failed handler must reject before observable work; a
failed nested dynamic lifecycle poisons the tree rather than discarding a
partially initialized branch. Parent blocking follows source child traversal.

`get_node()` retains relative paths, parent navigation and property subname
separation; empty paths and the absent unique-owner namespace reject. Absolute
and outside-root paths require the actual external tree registry. `add_child`,
`remove_child`, `move_child`, owner validation and dynamic checked-subtree
instantiation operate on the same state. Native readable-name generation and
arbitrary new factories are not approved by these APIs. Separate original
Grass/Enemy/Shaker factories still require checked source node recipes and
their actual typed factory bridge; cloning an unrelated prototype is forbidden.

Canvas transforms use native float matrix operations and immediate Canvas
parent relationships. Parent self-modulation does not propagate. World caches
invalidate only affected Canvas descendants. Local notifications occur
immediately; world notifications use native head insertion and saved-next
flush ordering, including the initial notification for every entering Canvas.
Original notify flags cover 2,611 world and 941 local-notification nodes.
Typed collision and rendering owners consume these changes. VisibilityChanged
and hide propagate only through visible direct Canvas children. Effective z
uses native relative inheritance and the engine's -4096..4096 range; YSort
and behind-parent flags remain available to the actual unified draw queue.

Process membership has an index; a frame does not scan all 8,686 nodes to find
its process group. Group calls use live tree order and processing uses native
priority, pause inheritance and actual internal-before-script passes. The
inherited pause policy beyond a field root is resolved through the real
external SceneTree parent, not an invented viewport. Input
registration uses the actual Viewport registry. Native animation, timer,
audio, Control layout, raycast, physics interpolation, rendering and their
signals still require their concrete typed owners. Structural metadata does
not substitute for those implementations.

The global deferred-message consumer uses original two-buffer FIFO: callbacks
append to the write buffer and run after all current read-buffer messages.
Actual dead ObjectDB targets are discarded; unknown live methods/properties
reject. Supported values are Nil/bool/int64/double/string/Vector2/ObjectRef;
Array/Dictionary and other values require a future checked typed extension.
Queue deletion is separate, deduplicated, source child-index/parent-ID sorted
using the MIT-attributed Godot SortArray algorithm, including its tie behavior.
Native PREDELETE detaches the parent before deleting children from the tail;
new deletions appended during destruction are consumed in the same flush.

## Verification boundary

Private original exports and raw logs remain outside public source. Real GCC
and devkitARM compilation and source/format admission are separate from gameplay
verification. Manual positive/negative cases are retained as source and are
not run automatically. No behavior probe, test suite, emulator run, hardware
run or complete Podunk activation is claimed by this structural slice.

Source engine semantics are audited against official Godot commit
`3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8`: `scene/main/node.cpp`,
`scene/main/scene_tree.cpp`, `scene/2d/canvas_item.cpp/.h`,
`scene/2d/node_2d.cpp`, `core/message_queue.cpp`, `core/self_list.h` and
`core/sort_array.h`. The adapted sorting header retains the original MIT notice.
