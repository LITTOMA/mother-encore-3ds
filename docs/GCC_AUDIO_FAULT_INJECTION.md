# Fortified audio-read fault injection

An optimized GCC/glibc build can call `__fread_chk` instead of ordinary `fread`.
The original fault-injection test wrapped only `fread`, so it missed the checked
entry point and failed its short-read observation despite retaining the production
short-read rejection. GCC 12.2 Release with `_FORTIFY_SOURCE=3` reproduced the
failure; the affected object referenced both libc entry points.

The test routes both entry points through one observer and invokes each real
libc function, retaining the checked function's buffer-size argument. Injection
counters require the short read and read error to reach the candidate PCM read.
A temporary-file probe exercises `__fread_chk` even when a compiler proves some
reads safe and optimizes them to ordinary `fread`.

CMake detects the checked libc symbol. Where available it adds
`music_region_prepare_fortified`, compiling the same production adapter and
`tests/music_region_prepare_tests.cpp` with optimization and `_FORTIFY_SOURCE=3`.
The ordinary `music_region_prepare` test remains registered. This preserves
fortification and checks failed-read rollback without changing production audio
behavior, expected traces, PCM fingerprints, pack schemas or save rules.

Both targets are part of `make test` and sanitizer checks. Host NDSP doubles and
parser tests cannot establish audible audio, emulator DSP availability or physical
3DS verification; those require separate platform checks.
