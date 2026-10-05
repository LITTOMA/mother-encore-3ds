# Source Dandelion Ready and visibility factory

The source-backed `ENCFDAN1` resource contains all 135 original Podunk spawners,
complete source VisibilityNotifier world transforms/rectangles, parent/local
positions, the original dynamic Area2D rectangle, sprite frames, source material
and gradient, and 56 native numeric particle properties. Native defaults were
exported with official Godot 3.6.2 from a quarantined original prototype. The
original script, signal handlers and particle behavior were not executed.

The compiler verifies the pinned source inventory and semantic review, then
emits an independent checked binary and texture receipt. No private native
export or ignored report is needed for clean-clone compilation. Original PNGs
were converted with real tex3ds, using a four-worker pool for the two independent
assets. Neither source authoring JSON nor PNG is loaded by the 3DS consumer.

`FieldDandelionRuntime::ready` actually queues the original spawner Sprite child
for deferred deletion. Visibility entry creates the checked original prototype,
sets its global position while it is still parentless to the spawner's **local**
position, then adds it to the actual source parent. The host supplies the resulting
live world position and authoritative factory identity. Subsequent entries
reuse the existing plant. The GPU renderer draws the source sprite frame with
nearest filtering; scene visibility and YSort ordering stay with the scene host.

The upstream exit handler has an inverted guard: it calls queue_free only when
`_current_dandelion` is null. An existing plant is retained; a null exit explicitly
reports the original script failure. This implementation does not silently fix
that original behavior.

Capability 3 certifies Ready and visibility factory only. Full scene and particle
emission admission remain false. Player body entry requires a real, separately
admitted CPUParticles emission/clock/RNG/render consumer, and currently rejects
before changing frame or pretending to emit. This pending mechanism is retained
in IR/review and cannot be bypassed by a callback that returns an unchecked true.
The particle backend must preserve the original 16-particle global RNG draws,
fractional spawn deltas, sphere emission, one-shot timing and source color ramp.

The lifecycle Host now dispatches the actual Dandelion Ready method (role 12)
and verifies source SHA/scene identity/NodeID/Ready ordinal against the complete
2157-script roster. This moves the next missing lifecycle class to Door, node
`Objects/Doors/NintensHouse`, Ready ordinal 3626. It does not admit the whole map.

Strict GCC/devkitARM object compile and actual resource format admission are
the verification levels. No suite, sanitizer, behavior probe, emulator or
hardware run has been performed for this slice.
