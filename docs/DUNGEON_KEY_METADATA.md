# Catacombs key/lock metadata candidate

This is a data and reference component, not a playable Catacombs scene.
It depends on the Iggy/Root work in PR69. No release catalog, map, session
admission, saved bytes, audio bank or existing game resource changes here.
Key/lock execution and their actual presentation remain to be integrated.

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

Targeted checks currently include13 Python source/codec methods and197 actual
GCC ASan/UBSan checks with leak checking. The generated alignment fixture is
explicitly artificial and separate from the actual568-byte candidate. Windows
CMake configuration and method collection are metadata checks only; no native
Windows execution, complete current CI, console build, emulator, hardware or
complete dungeon pass is claimed for this component.
