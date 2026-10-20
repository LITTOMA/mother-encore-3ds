# Catacombs key/lock metadata candidate

This is a data, method-state and reference component, not a playable Catacombs scene.
It depends on the Iggy/Root work in PR69. No release catalog, map, session
admission, saved bytes, audio bank or existing game resource changes here.
The shared core executes the scoped key/lock method states. Scene ownership,
actual animation timelines, UI/audio consumers and save admission remain to be integrated.

The reviewed fixed source is7d9246600fffe518408f5830d4848635019005a3.
The source compiler admits the three Small Key and three Locked Door instances,
their stable IDs and source leaf-name object flags, the Catacombs region
counter, initial/minimum values, signed deltas and the locked-message identity.
Both source scenes emit `flags_updated`; the notification is not optional in
this admitted subset. Counter capacity is bounded by the admitted pickups.
The existing save already contains `keys.Catacombs=0`; no new game-save format
is needed to describe this candidate. Live counter/flag save admission is still
restricted by the existing session policy and is not relaxed here.

The independent ENCDKEY1 resource uses format1/capability1 and three bounded
sections: canonical ASCII path strings, one32-byte policy and32-byte node rows.
The C++ owner validates source pin, checksum, directory/bounds/padding, known
node/flag kinds, unique saved identities, counter bounds and complete canonical
string ownership. Rejected loads preserve the previous owner. The CLI only
writes candidates under build; it cannot stage RomFS or activate a scene.

`DungeonKeyRuntime` admits complete stable-ID flag coverage, the scene/region
and a bounded counter. Generation handles reject old-scene callbacks. Original
method gates and ordered requests preserve counter writes, synchronous flag
observer snapshots, indicator requests, audio occupancy input, hiding, deferred
collision and deletion. Saved keys queue deletion; saved locks request Opened
and disable their collider/prompt. Capacity or identity failures preserve the
entire previous state and event queue. This is a detached component: requests
do not themselves play sound, display dialogue or advance animation time.

Original source methods run in official Godot3.6.2 with byte-identical
FlaggableObject/Small Key/Lock scripts and explicit scene/player/UI/audio
services. Two independent runs produce identical18 observations. They confirm
counter-before-flag notification, notification-before-key hide/audio/deferred
collision, queued deletion, no-key denial, open requests, the explicit open
method and already-saved objects. AnimationPlayer is a request service in this
fixture; its source timeline, physical contact, graphics and audible output
are not claimed.

The source Locked Door scene connects `animation_finished` to a method absent
from Lock and FlaggableObject. The isolated negative case connects successfully,
logs exactly the missing-method diagnostic on emission and preserves live
state. It intentionally exits17 after writing all observations. Any other
diagnostic or exit status rejects the run. Only the original project's
`unused_argument=false` setting is retained for unchanged callback parameters;
the service stubs' earlier diagnostic failures are preserved separately.

Targeted parser checks include13 Python source/codec methods and197 actual
GCC ASan/UBSan checks with leak checking. An additional1713 GCC ASan/UBSan
method-state checks compile the new components and file IO directly from source.
The method-state tests replay16 valid
original-method projections twice; the two missing-callback dispatch negatives
remain separately verified in the original engine. The fixture also exercises
counter limits, missing/duplicate identities, stale handles, dead/wrong-kind
callbacks and rollback after partially attempted event production. The generated alignment fixture is
explicitly artificial and separate from the actual568-byte candidate. Windows
CMake configuration and method collection are metadata checks only; no native
Windows execution, complete current CI, emulator, hardware or complete dungeon
pass is claimed here. The metadata-only b9befdc console job118467297475 built
genuine3DSX/CIA and checked packaged resources; that does not verify the later
method-state component or playable Catacombs.
