# Optional frame and region diagnostics

`ENCORE_FRAME_PROFILE` remains disabled in the production build. Its storage,
clock reads, frame counters, renderer accessors and log lines are compiled out
when the define is absent. No region algorithm, sampling expression, shader
clock, input, resource or game rule changes are part of this diagnostic work.

The existing platform makefile accepts `EXTRA_CPPFLAGS=-DENCORE_FRAME_PROFILE`,
but that writes the usual production output paths and a changed define alone
does not force existing objects to rebuild. Prefer the isolated QA builder:

```sh
# Requires the already-built, matching production core objects and staged RomFS.
# Does not rebuild the core or overwrite the production 3DSX.
python3 tools/build_continue_text_qa.py --frame-profile
```

Output: `build/frame-profile-qa/qa.3dsx` and its ELF/map. The builder writes diagnostic output to `reports/frame-profile-qa/`. The SD log
is `encore-frame-profile-qa.log`. The old no-argument builder keeps its existing
`continue-text-qa` outputs/log and does not enable frame profiling. Use a separate emulator profile and save directory for diagnostics. A successful host test is not an ARM build or execution result.

## Timings

`FrameProfile` accepts caller-supplied system ticks. Seven ordered segments are
measured from the beginning of an application loop iteration:

1. core: input and gameplay update, including any loading in that iteration
2. sync: `C3D_FrameBegin(C3D_FRAME_SYNCDRAW)`
3. room: clears and world rendering submission
4. battle: battle/dialogue/menu/overlay rendering and reference borders
5. bottom: lower-screen diagnostic UI and native input display
6. end: `C3D_FrameEnd`
7. QA: `qa_record`, including formatted writes and `fflush`

The total is the elapsed tick span across all seven segments. It excludes the
outer `aptMainLoop` call and the profiler's own final aggregation. These are
guest-system elapsed timings in an emulator, not physical CPU/GPU measurements.
Instrumentation itself can affect scheduling, caches and the lower-screen UI;
there is no uninstrumented-performance-equivalence claim.

A window contains 60 completed frames. Incomplete/failed-begin frames contribute
no partial times. Missing, repeated, out-of-order or backward-clock marks reject
that frame. Arithmetic overflow rejects a frame before its sums are committed.
Changing the battle/world, viewport or battle identity resets a partial window.
A frame whose context changes between start and finish is discarded rather than
publishing mixed rendering under its earlier identity, including the 60th frame.
Published windows retain their original context and monotonically increasing
sequence number. `discarded_frames` is a cumulative count of rejected/incomplete
frames, not part of a particular window's mean.

`FRAME_PROFILE` logs the latest previous completed window, once, at a subsequent
normal QA log emission. If multiple windows finish between emissions, sequence
gaps identify windows replaced by the latest snapshot. This avoids trying to
print a duration before its own log call
has ended. That printing cost belongs to the current frame/window. `first_tick`
and `last_tick` bound the published window; they may include gaps between its
completed frames and should not be substituted for the sum of segment means.
No partial window is promoted merely because a state changes or the app exits.

## Region counters

`BACKGROUND_FRAME` is a separate current-frame sample, emitted only after a
successful composition in that application iteration. It reports:

- `direct`: direct mapped texture generation versus linear surface generation
- `uploaded`: whether this iteration called the surface-upload wrapper; when
  false, this line's `upload_ms` is zero instead of reusing an older value
- `region_ready`: region specialization was prepared successfully
- `region_used`: this composition actually produced nonzero region samples
- `preparation`: existing enum value, 0 inactive / 1 ready / 2 unsupported /
  3 resource limit / 4 allocation failure
- `samples` and `skipped`: existing counters summed over the two source layers
- `prepared_bytes`: existing retained-size estimate, not peak heap or allocator
  overhead
- `compose_ms` and `upload_ms`: last composition and upload-call measurements,
  not the window means or GPU completion latency

At 400×240 a successful full region composition accounts for 192,000 layer
samples plus skips. Ready alone does not imply use: unusual times may retain the
exact baseline fallback. This change exposes existing counters only; it does
not add scalar-trigonometric fallback counters or change kernel branches.

Correctness and the full NewGame→Record→LOAD path take priority over optimization.
Use the segmentation to choose the next isolated experiment, then require the
existing exact pixel/mapped-byte and failure-path tests before performance
comparisons. No Godot GPU or physical 3DS parity is newly asserted here.
