#pragma once
#include "encore/movement.hpp"
#include "encore/room_data.hpp"
#include <array>
#include <cstdint>
namespace encore::upstream {
enum class ActorExecution : uint16_t { DirectionalProxy=1, TimelineProxy=2, NpcDirectionalProxy=3 };
struct ActorPositionMove { Vec2 target{}; float speed=0; };
struct ActorTurn { Vec2 target{}; double interval=0,remaining=0; float angle=0; uint32_t timer_order=0,wait_order=0; bool active=false,waiting=false; };
struct ActorJump {
    double elapsed=0, length=0; float height=0; bool active=false;
    uint8_t remaining=1; bool repeated=false,waiting=false;
    double wait_remaining=0; uint32_t timer_order=0;
};
enum class ActorMoveType : uint8_t { Position=0, Step=1 };
enum class ActorPathEntryKind : uint8_t { Move=0, Wait=1 };
struct ActorPathEntry { ActorPathEntryKind kind=ActorPathEntryKind::Move; Vec2 vector{}; double duration=0; };
enum class ActorPathWait : uint8_t { None=0, Action=1, Movement=2, Timer=3 };
struct ActorMovePath {
    std::array<ActorPathEntry,16> entries{};
    uint8_t count=0,cursor=0;
    ActorMoveType type=ActorMoveType::Position;
    ActorPathWait wait=ActorPathWait::None;
    uint16_t animation_motion=UINT16_MAX;
    float speed=0;
    bool moonwalk=false,loop=false;
    double remaining=0;
    uint32_t wait_order=0;
};
// Bounded adapter for the actual lamp_attack/doll_attack replacements. This is not
// the original NPC's wandering controller or a generic action interpreter.
struct ActorActionState {
    RoomView content;
    uint32_t profile_index=kRoomNoIndex,clip_index=kRoomNoIndex,emote_clip_index=kRoomNoIndex;
    Vec2 position{}, direction{}, velocity{},blend_direction{};
    Vec2 sprite_position{}, sprite_offset{}, emote_position{};
    uint16_t frame=0, emote_frame=0;
    bool shadow_visible=false, moving=false, initialized=false;
    uint32_t finished_movement=0, finished_action=0;
    ActorPositionMove movement{};
    std::array<ActorMovePath,4> move_paths{};
    uint32_t next_wait_order=0;
    ActorMoveType movement_type=ActorMoveType::Position;
    bool moonwalk=false;
    uint16_t animation_motion=0,playing_motion=0;
    uint8_t playing_direction=0;
    bool directional_animation=false,talking=false;
    std::array<ActorJump,4> jumps{};
    Vec2 shake_base{}, shake_magnitude{};
    double shake_remaining=0, animation_elapsed=0, emote_elapsed=0;
    uint16_t shake_half_steps=0;
    bool shaking=false, emote_playing=false;
    std::array<ActorTurn,4> turns{};
    uint32_t shake_timer_order=0;
    bool rotating=false;
    // Original Actor._looping is shared by move_queue and recursive shake.
    // stop_loop finishes the current iteration; it does not cancel movement.
    bool looping=false;
};
bool initialize_actor(ActorActionState& actor,const RoomView& content,uint32_t profile_index,Vec2 position,Vec2 direction);
bool actor_move_position(ActorActionState& actor,Vec2 target,float speed);
// Copies a bounded path; values are supplied by the checked external resource.
// Each vector/wait resumes the same source coroutine, not a new command.
bool actor_move_path(ActorActionState& actor,const ActorPathEntry* entries,size_t count,float speed,ActorMoveType type,bool moonwalk=false,uint16_t animation_motion=UINT16_MAX,bool loop=false,bool queue=false);
bool actor_move_path(ActorActionState& actor,uint32_t path_index);
// Source teleportactors leaves existing movement targets and waits intact.
bool actor_teleport(ActorActionState& actor,Vec2 position);
bool actor_set_talking(ActorActionState& actor,bool talking);
// Direct set_direction. Dialogue actorsturn must use actor_turn_to instead.
bool actor_turn(ActorActionState& actor,Vec2 direction);
// Source queue=true always waits for finished_action, including while idle.
// Both directions normalize immediately; the target is captured before waiting.
bool actor_turn_to(ActorActionState& actor,Vec2 direction,double interval,bool queue=false);
bool actor_jump(ActorActionState& actor,float height,double length,unsigned repetitions=1);
bool actor_shake(ActorActionState& actor,Vec2 magnitude,double length);
bool actor_stop_loop(ActorActionState& actor);
bool actor_play_clip(ActorActionState& actor,uint32_t clip_index);
bool actor_play_emote(ActorActionState& actor,uint32_t clip_index);
// Fixed 60 Hz physics: actor._physics_process, then SceneTreeTween processing.
// Actor layer=mask=0 is intentional; never route these moves through world walls.
bool actor_physics_step(ActorActionState& actor);
// SceneTree idle phase order: animations, DialogueBox WaitTimer callbacks,
// then SceneTreeTimers. Newly created timers are eligible in this frame; a
// replacement timer created by a timer callback waits until the next frame.
bool actor_idle_animations(ActorActionState& actor,double delta=1.0/60.0);
bool actor_scene_timers(ActorActionState& actor,double delta=1.0/60.0);
// Convenience only when no commands occur between those two idle phases.
bool actor_idle_step(ActorActionState& actor,double delta=1.0/60.0);
}
