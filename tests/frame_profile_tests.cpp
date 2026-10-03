#include "encore/frame_profile.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
unsigned checks = 0;
void check(bool result, const char* message) {
    ++checks;
    if (!result) { std::fprintf(stderr, "Frame profile: %s\n", message); std::exit(1); }
}
using Profile = encore::FrameProfile;
const Profile::Context battle{{1, 400, 240, 2}};
bool frame(Profile& p, uint64_t begin, uint64_t step = 10,
           Profile::Context context = battle) {
    p.start(begin, context);
    for (size_t i = 0; i < Profile::stage_count; ++i)
        check(p.mark(i, begin + (i + 1) * step), "ordered marks accepted");
    return p.finish(context);
}
}

int main() {
    Profile p(10);
    for (unsigned i = 0; i < 59; ++i) check(!frame(p, i * 100), "partial window is unpublished");
    check(p.pending_frames() == 59 && !p.snapshot().sequence, "59 complete frames stay pending");
    check(frame(p, 5900), "60th complete frame publishes");
    const auto first = p.snapshot();
    check(first.frames == 60 && first.context == battle && first.sequence == 1, "snapshot identity");
    check(first.first_tick == 0 && first.last_tick == 5970, "window tick bounds");
    for (size_t i = 0; i < Profile::stage_count; ++i) check(first.mean_ms[i] == 1, "stage mean includes QA stage");
    check(first.mean_ms[Profile::total_index] == 7, "total includes all seven stages");
    check(p.pending_frames() == 0, "completed window reset");

    // Model C3D_FrameBegin failure: mark core, then start the next frame.
    p.start(6000, battle); check(p.mark(0, 9000), "failed-frame prefix accepted");
    check(!frame(p, 10000), "replacement complete frame stays pending");
    check(p.discarded_frames() == 1 && p.pending_frames() == 1, "failed prefix not accumulated");
    for (unsigned i = 1; i < 60; ++i) frame(p, 10000 + i * 100);
    check(p.snapshot().sequence == 2 && p.snapshot().mean_ms[0] == 1, "failed begin does not pollute window");
    check(p.snapshot().mean_ms[Profile::total_index] == 7, "rejected frame absent from total");

    p.start(20000, battle); check(!p.mark(1, 20010), "out-of-order first mark rejected");
    check(!p.mark(0, 20020) && !p.finish(battle), "invalid frame cannot resume");
    p.start(20100, battle); check(!p.mark(0, 20099), "backward clock rejected");
    p.start(20200, battle); check(p.mark(0, 20201), "incomplete prefix");
    check(!p.finish(battle) && p.pending_frames() == 0, "missing stages reject without accumulation");
    p.start(20300, battle); check(!p.mark(Profile::stage_count, 20301), "unknown stage rejected");
    p.start(20400, battle); check(p.mark(0, 20401) && !p.mark(0, 20402), "duplicate stage rejected");
    check(p.snapshot().sequence == 2, "invalid frames preserve published evidence");

    frame(p, 21000); frame(p, 21100);
    Profile::Context other{{1, 320, 180, 2}};
    check(!frame(p, 22000, 20, other) && p.pending_frames() == 1, "context change drops partial window");
    check(p.snapshot().context == battle, "old published context retained until new window complete");
    for (unsigned i = 1; i < 60; ++i) frame(p, 22000 + i * 200, 20, other);
    check(p.snapshot().context == other && p.snapshot().mean_ms[6] == 2, "new viewport has its own QA mean");
    check(p.snapshot().mean_ms[7] == 14, "new context total correct");
    const auto prior = p.snapshot().sequence;
    for (unsigned i = 0; i < 60; ++i) frame(p, 40000, 0, other);
    check(p.snapshot().sequence == prior + 1 && p.snapshot().mean_ms[7] == 0, "zero duration is valid");

    // A viewport or battle transition inside the 60th frame must not publish
    // a window bearing its old context but containing the new rendering.
    Profile transition(10);
    for (unsigned i = 0; i < 59; ++i) frame(transition, i * 100);
    transition.start(5900, battle);
    for (size_t i = 0; i < Profile::stage_count; ++i) transition.mark(i, 5910 + i * 10);
    check(!transition.finish(other) && !transition.snapshot().sequence, "mixed-context 60th frame is not published");
    check(transition.pending_frames() == 59 && transition.discarded_frames() == 1, "mixed frame leaves earlier sums untouched");
    frame(transition, 6000, 10, other);
    check(transition.pending_frames() == 1, "new context drops prior partial window");

    for (double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        Profile bad(invalid); bad.start(0, battle);
        check(!bad.mark(0, 1) && !bad.finish(battle) && !bad.pending_frames(), "invalid clock scale fails closed");
    }
    Profile overflow(1);
    overflow.start(0, battle);
    for (size_t i = 0; i < Profile::stage_count; ++i)
        check(overflow.mark(i, std::numeric_limits<uint64_t>::max()), "large monotonic ticks accepted");
    check(!overflow.finish(battle) && overflow.pending_frames() == 1, "one large frame accumulated");
    check(!frame(overflow, 0, 1) && overflow.pending_frames() == 1, "sum overflow rejects frame atomically");
    check(overflow.discarded_frames() == 1, "overflow discard recorded");
    std::printf("Frame profile: %u checks, completed windows, QA time, contexts and rejection paths\n", checks);
}
