# Bounded melodyBG mechanism evidence

Pinned game commit: `7d9246600fffe518408f5830d4848635019005a3`.
Pinned reference engine: official Godot 3.6.2 `3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8`.
No screenshots, screen captures, image encoding or image emission were used. The graphics evidence is a numeric offscreen Viewport test.

## Implemented scope

The independent `romfs/world-effect/melody.encfx` resource is 532 bytes. Its checked `ENCWFX01` schema owns the source 16×10 indexed RGBA image, color palette, six color keys, source canvas extent, appearance/disappearance times and shader coefficients. Header, schema/capability/rules identity, pinned commit, CRC, directory spans/strides, float bounds, monotonic keys and palette references are checked before replacing an existing owner. It has no dependency on battle presentation packs.

`WorldEffect` reproduces the source 0.5-second SceneTreeTween alpha transitions (default linear transition, EASE_OUT), 1.2-second looping AnimationPlayer modulate track, and native float32 clocks/interpolation. The native approximate key lookup is retained: a time just before a key may select that key and use a tiny negative interpolation weight. This is an engine comparison rule, not a visual correction. The original source Animation resource was used directly for the headless clock reference.

The source shader's local copy of COLOR is unused, but Godot's canvas shader multiplies `final_modulate` after the user fragment when the shader does not explicitly use MODULATE. Tint and fade therefore remain visible. This is supported both by the engine canvas post-step and by the actual pinned GLES2 output.

TextureRect STRETCH_TILE supplies texture-size UVs and repeat sampling even though the imported texture's repeat flag is false. With the source project's GPU pixel snap enabled, the engine vertex shader adds exactly 1e-5 to UV. The vertical expression is, in source order:

`v += amplitude_y * cos(frequency_y*u + cos(translation_ping_pong_y*TIME)*speed_y) * cos(amplitude_ping_pong_y*TIME); v += TIME*move_y/move_divisor`

For the pinned data this is `v += .5*cos(2*u + 2*cos(2*TIME)); v -= 2*TIME`. TIME is supplied globally and never reset on appearance. The kernel computes one cosine per column and preserves source float operation order; there is no approximate mesh, trig reassociation, time quantization, or pixel-size reduction.

The source rectangle is 320×180 centered on the captured camera position. The explicitly separate 400×240 display adapter extends its bounds at 1:1, preserving source UV origin at screen (40,30), without stretching 16×10 artwork. The renderer's x/y draw offset is captured center minus current camera center. Gameplay, actor lift order, reparenting and dialogue integration belong to the world/House adapter, outside this module.

## Evidence

- `native.log`, `native.json`: actual official GLES2 llvmpipe source probe, 20 cases
- `reference.json`, `native-rgba.zlib`: compact numeric byte oracle, 1,459,200 RGBA pixels; no PNG or screen image
- `comparison.json`: exact pixel bytes for 16 shader samples (8 global TIME values at each canvas size), opaque color, partial alpha blend and zero alpha
- `timing.bin`, `timing-native.log`, `timing-comparison.log`: 140 native frames; exact float32 modulate, alpha, animation position and finished-signal state
- `host.log`: 6,913,150 assertions, including all 532 truncations, malformed-pack rejection/rollback, repeated/interrupted lifecycle and 6,912,000 center-crop pixel comparisons across 120 times
- `compiler-tests.log`: seven Python groups for strict source/compiler validation, including float32-collapsed keys
- `sanitizer.log`: focused ASan/UBSan checks; LeakSanitizer disabled for the documented ptrace environment constraint

The optimized host kernel measured about 0.27–0.32 ms/frame for 400×240 over 300 frames. This is a host CPU measurement, not target hardware performance. The CTR renderer header cross-compiled with official devkitARM; SDK header pedantic warnings were present, with no effect-specific compile errors.

## Reproduction

```
python3 tools/world_effect_assets.py extract   # explicit source IR refresh only
python3 tools/world_effect_assets.py compile
python3 tests/world_effect_assets_test.py -v
world_effect_tests romfs/world-effect/melody.encfx
world_effect_tests --timing romfs/world-effect/melody.encfx reports/world-effect-reference/timing.bin
python3 tests/world_effect_compare_reference.py --probe /path/to/world_effect_tests
```

For a fresh source render, prepare with `tests/world_effect_reference.py --out <fresh project>` and run the official GUI Godot binary with GLES2, fixed FPS 60 and `-s probe.gd`. The copied shader substitutes a uniform for TIME solely to make numerical samples reproducible. The expanded cases additionally apply the documented source-origin offset; the 320×180 cases use zero offset. No original checkout is modified. `tests/world_effect_timing_reference.py --godot <official headless binary>` regenerates the separate timing reference without graphics.

## Limits

This proves the enumerated numeric samples and native timeline, not complete game UI parity. The CTR path samples into straight RGBA8 and uses normal source-alpha C2D blending. Arbitrary fractional tint/alpha can introduce byte-quantization differences from a floating-point GLES shader, especially over non-black backgrounds; exact PICA blend/readback and full-frame ordering are not established by the transparent-target model. Different CPU/GPU cosine implementations can differ near nearest-sampler boundaries outside the sampled times. Physical Old/New 3DS performance, visual acceptance, audio and whole-story verification remain separate tasks.
