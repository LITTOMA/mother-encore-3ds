#pragma once
#include <cstdint>

namespace encore::upstream {
class RoomView;
struct Vec2 { float x=0, y=0; };
enum class MotionAnimation : uint8_t { Idle, Walk, Run, Crouch };
struct WalkInput { int8_t x=0, y=0; bool toggle=false, paused=false, entering_door=false; };
struct WalkState {
    Vec2 position{}, direction{}, velocity{};
    float speed=0;
    bool crouch=false, tap_run=false, running=false, substantial=false, walking=false, previous_toggle=false;
    MotionAnimation animation=MotionAnimation::Idle;
    uint32_t moved_signals=0;
};
struct SlideResult { Vec2 position{}, velocity{}; };
// The movement state machine does not implement a physics engine. Collision
// backends must return a verified move_and_slide result at the fixed 60 Hz.
// Queries must not mutate world state, so a rejected step is transactional.
class MotionSolver {
public:
    virtual ~MotionSolver()=default;
    virtual bool slide(Vec2 position, Vec2 velocity, SlideResult& result) const=0;
};
class FreeMotionSolver final : public MotionSolver {
public:
    bool slide(Vec2 position, Vec2 velocity, SlideResult& result) const override;
};
// Audited opening-player MOVE scope: single party member, no climbing,
// knockback, teleport, attacks, relay, interaction or presentation callbacks.
// Invalid inputs/backend results leave state unchanged. This is not M0 motion.
bool advance_walk(WalkState& state, WalkInput input, const MotionSolver& solver,const RoomView& content);
// controlsManager._get_vector_sign, including round-before-sign behavior.
// Upstream `threshold := 0` infers an integer argument in Godot 3.6.
bool controls_vector(Vec2 input, int32_t threshold, Vec2& result);
}
