# Rejected row-invariant hoist experiment

Decision on 2026-10-02: **reject this hoist; no measured benefit.** The active
header has been restored byte-for-byte to the validated pre-experiment SHA256
`5d1b237d5e37bbb823f29b7d4756a252e68d78c76edba41bf3c9bc6bffbdcb05`.
The rejected header had SHA256
`6e9bdfa3e6070c5692bede929d5a26698a01e93ebb6ff74b2ec9f28718f1ce54`.
Expanded correctness tests remain active. No production artifact, palette, source image, sampling
clock, display resolution, game rule or upstream file changed.

The final restored state passed host 70/70 in 50.48 seconds and ASan/UBSan 70/70
in 129.02 seconds (LeakSanitizer off). Production 3DSX/CIA and the accepted
diagnostic QA hashes remained unchanged.

## ARM outcome and disposition

Six isolated Azahar processes ran the matched probes in AB, BA, AB order. All
96 cases passed: repeated coverage of 7,372,800 linear pixels and 503,316,480
mapped bytes. All 48 paired cases had identical sample/skip counts. Independent
recalculation from the six raw logs matched the validator's summary exactly.

- 400×240: candidate-minus-before median +0.088709 ms (+0.25897%), consistently
  slower across the three pair orders
- 320×180: +0.016426 ms (+0.07481%); smaller than the approximately 0.0631 ms
  within-case timer spread, so there is no clear benefit

Smaller row functions and fewer repeated arithmetic instructions did not produce
a gain. The candidate full-game QA was not loaded, and no full Doll rerun or
further algorithm experiment was started. These guest-clock results do not
establish a physical-hardware slowdown or GPU pixel equivalence.

## Measured reason to test it

The diagnostic Doll 400×240 run reported 21 complete 60-frame windows: total
50.136 ms, battle 38.233 ms, sync 9.164 ms, bottom 1.055 ms, core 0.694 ms,
room 0.635 ms, end 0.305 ms and QA 0.042 ms. Its 63 sampled backgrounds averaged
36.556 ms composition with 85.32% skipped layer samples and the region/direct
paths active. Upload-call time was 0.062 ms. These are emulated guest-clock
measurements, not physical hardware or GPU completion times.

The archived profile ELF's real ARM disassembly still calls both row functions
for each y. Each call repeats the inverse-factor divide and complete conservative
error-envelope calculation. For example, the before true/false row functions
contain `vdiv.f32` at 0x190ac0 / 0x1916cc. The compiler had hoisted some scales
out of each chunk loop, but only as far as a single row.

## Narrow change

The archived candidate's `RegionBackgroundKernel::row_frame` computes the existing inverse factor, error
envelope, oscillation/compression scales and their absolute values once per
layer per composition. `compose_runs` holds two immutable local contexts.
`compose_row` borrows them while its original `SeparableFrame` value copy stays
local. The pointed-to caller frame array remains alive throughout `compose_runs`.

All floating expression grouping, f32 intermediates and outward rounding are
retained. There is no narrowed error bound, reassociation, time quantization,
different texel selection, color change, new allocation or cached frame history.
Sampling, slope/boundary decisions, chunk size, row filling, y/x write order,
stats reset, preparation limits and fallback entry checks are unchanged.
Duplicate output offsets therefore retain their original last-index-wins order.

At 400×240 this moves 480 row preparations to two layer preparations. That is
an instruction-count opportunity, not a performance promise. Context loads,
alias analysis, registers and stack can offset the saved arithmetic; target ARM
comparison is required before any adoption decision.

## Verification

- The full host suite passed 70/70 in 50.40 seconds.
- The full ASan/UBSan suite passed 70/70 in 130.41 seconds; LeakSanitizer was off.
- The final expanded region tests add adjacent-float samples around the existing
  guarded phase limits and two high-amplitude cases. They retain every truncation,
  mapping, duplicate-offset, sentinel, nonfinite-time, allocation-failure and
  source/component-cap test already present.
- Frozen-before and candidate builds produce byte-identical 509-case dispatch
  transcripts, including linear/mapped success and sample/skip counts. They each
  independently compare 19,671,900 linear pixels and 244,318,208 mapped bytes
  against the frozen scalar oracle. The final candidate matrix also passes ASan,
  UBSan and float-cast-overflow checks. Tests explicitly confirm known original
  inside/large-time fallback paths were exercised.
- The standalone paired-case probe independently checks 1,228,800 linear pixels
  and 83,886,080 mapped bytes per build. All 16 host before/candidate cases retain
  identical sample/skip counts. Host timing results are mixed and noisy; they
  do not establish a target speedup.

The extra final dispatch checks were run after the aggregate suites; the kernel
was unchanged between those runs.

## Paired probe contract

`tests/region_row_invariants_probe.cpp` is the same source for host and ARM.
The normal `include` path contains the restored baseline rather than the rejected
candidate. Comparative builds used `-O2` and
`-ffp-contract=off`, checked Doll resources, 400×240 and 320×180, TIME values
0 / 1.25 / 60 / 1000, and two passes. Each case has two warmups followed by eight
mapped-composition timings. Pixel/texture comparisons occur outside the timed
interval; every timed output is still consumed and checked, including padding
and guards. Each successful binary prints 16 cases and `ALL CHECKS PASS`.

ARM builds must use different artifact directories, labels and SD log names.
The probe never reads or writes a game save. Separate before/candidate binaries
and CPU-only timings cannot substitute for a controlled full-frame measurement.
Do not promote a candidate solely because its arithmetic is repeated less often.
