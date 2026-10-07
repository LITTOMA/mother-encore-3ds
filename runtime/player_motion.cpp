#include "encore/player_motion.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
namespace encore::upstream {
namespace {
using F = PlayerMotionField;
using T = PlayerMotionText;
using N = PlayerMotionNumber;
using S = PlayerMotionState;
using R = PlayerMotionNode;
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
// An unexecuted future branch is not a normal MOVE initialization gate.
// Invoking that exact source operation still requires its actual owner.
template <class F, class... Args>
bool branch(const F &f, const char *operation, std::string &e, Args &&...args) {
  if (!f) {
    e = std::string("Player actual source branch service unavailable: ") +
        operation;
    return false;
  }
  return f(std::forward<Args>(args)..., e);
}
bool zero(Vec2 v) { return v.x == 0 && v.y == 0; }
bool equal(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }
Vec2 mul(Vec2 v, double x) { return {float(v.x * x), float(v.y * x)}; }
bool finite(Vec2 v) { return std::isfinite(v.x) && std::isfinite(v.y); }

} // namespace
bool PlayerMotionRuntime::live(std::string &e) const {
  auto s = body_ && tree_ ? tree_->state(body_->object()) : nullptr;
  return data_ && !poisoned_ && body_->constructed() &&
                 ready_->body_complete() && s && s->alive && s->inside &&
                 s->bound &&
                 registry_->tree_owner(body_->object()).get() == tree_
             ? true
             : fail(e, "Player motion actual Ready/owner unavailable");
}
bool PlayerMotionRuntime::initialize(
    const PlayerMotionData &d, PlayerInitializationBody &b,
    PlayerReadyRuntime &r, PlayerAnimationGraph &a, FieldNodeTreeRuntime &t,
    FieldGlobalConstructorRuntime &g, FieldGlobalRegistry &registry,
    SourceRandom &random, PlayerMotionHost h, std::string &e) {
  if (data_ || !d.valid() || !b.data() || !b.constructed() || b.tree() != &t ||
      !a.data() || !g.data() ||
      d.identity().upstream_commit != b.data()->identity().upstream_commit ||
      d.identity().source_sha256 != b.data()->identity().source_sha256 ||
      registry.tree_owner(b.object()).get() != &t || !h.controls || !h.input ||
      !h.move_and_slide || !h.ray_rotation || !h.cached_ray || !h.emit ||
      !h.collider_connected || !h.collider_connection || !h.collider_info ||
      !h.is_climbing || !h.has_skill || !h.button_skills || !h.damage_effects ||
      !h.audio_resource || !h.audio_voice || !h.audio_play ||
      !h.audio_stop || !h.timer_left || !h.timer_start || !h.timer_wait ||
      !h.animation_current || !h.dust)
    return fail(e, "Player motion source/actual service set incomplete");
  data_ = &d;
  body_ = &b;
  ready_ = &r;
  graph_ = &a;
  tree_ = &t;
  global_ = &g;
  registry_ = &registry;
  random_ = &random;
  host_ = std::move(h);
  return true;
}
bool PlayerMotionRuntime::read(F f, PlayerInitializationMember &v,
                               std::string &e) const {
  return live(e) && body_->member(data_->field(f), v, e);
}
bool PlayerMotionRuntime::boolean(F f, bool &v, std::string &e) const {
  PlayerInitializationMember x;
  if (!read(f, x, e))
    return false;
  if (x.kind != 1 || !x.value)
    return fail(e, "Player bool member rejected");
  v = x.value->boolean;
  return true;
}
bool PlayerMotionRuntime::number(F f, double &v, std::string &e) const {
  PlayerInitializationMember x;
  if (!read(f, x, e))
    return false;
  if (!x.value || (x.kind != 2 && x.kind != 3))
    return fail(e, "Player numeric member rejected");
  v = x.kind == 2 ? double(x.value->integer) : x.value->real;
  return std::isfinite(v) ? true : fail(e, "Player nonfinite numeric member");
}
bool PlayerMotionRuntime::integer(F f, int64_t &v, std::string &e) const {
  PlayerInitializationMember x;
  if (!read(f, x, e))
    return false;
  if (x.kind != 2 || !x.value)
    return fail(e, "Player integer member rejected");
  v = x.value->integer;
  return true;
}
bool PlayerMotionRuntime::vector(F f, Vec2 &v, std::string &e) const {
  PlayerInitializationMember x;
  if (!read(f, x, e))
    return false;
  if (x.kind != 7)
    return fail(e, "Player vector member rejected");
  v = {float(x.vector[0]), float(x.vector[1])};
  return finite(v) ? true : fail(e, "Player vector range rejected");
}
bool PlayerMotionRuntime::object(F f, FieldObjectId &v, std::string &e) const {
  PlayerInitializationMember x;
  if (!read(f, x, e))
    return false;
  if (x.kind != 8 && x.kind != 0)
    return fail(e, "Player object member rejected");
  v = x.object;
  return true;
}
bool PlayerMotionRuntime::text(F f, std::string &v, std::string &e) const {
  PlayerInitializationMember x;
  if (!read(f, x, e))
    return false;
  if (x.kind != 4 || !x.value)
    return fail(e, "Player string member rejected");
  v = x.value->string;
  return true;
}
bool PlayerMotionRuntime::set_boolean(F f, bool v, std::string &e) {
  PlayerInitializationMember x;
  x.kind = 1;
  auto p = std::make_shared<GlobalYamlValue>();
  p->kind = 1;
  p->boolean = v;
  x.value = p;
  return body_->assign_member(data_->field(f), x, e);
}
bool PlayerMotionRuntime::set_number(F f, double v, std::string &e) {
  if (!std::isfinite(v))
    return fail(e, "Player numeric mutation overflow");
  PlayerInitializationMember x;
  x.kind = 3;
  auto p = std::make_shared<GlobalYamlValue>();
  p->kind = 3;
  p->real = v;
  x.value = p;
  return body_->assign_member(data_->field(f), x, e);
}
bool PlayerMotionRuntime::set_integer(F f, int64_t v, std::string &e) {
  PlayerInitializationMember x;
  x.kind = 2;
  auto p = std::make_shared<GlobalYamlValue>();
  p->kind = 2;
  p->integer = v;
  x.value = p;
  return body_->assign_member(data_->field(f), x, e);
}
bool PlayerMotionRuntime::set_vector(F f, Vec2 v, std::string &e) {
  if (!finite(v))
    return fail(e, "Player vector mutation overflow");
  PlayerInitializationMember x;
  x.kind = 7;
  x.vector = {v.x, v.y};
  return body_->assign_member(data_->field(f), x, e);
}
bool PlayerMotionRuntime::set_text(F f, std::string v, std::string &e) {
  PlayerInitializationMember x;
  x.kind = 4;
  auto p = std::make_shared<GlobalYamlValue>();
  p->kind = 4;
  p->string = std::move(v);
  x.value = p;
  return body_->assign_member(data_->field(f), x, e);
}
bool PlayerMotionRuntime::node(R r, FieldObjectId &v, std::string &e) const {
  if (!live(e) || !tree_->get_node(body_->object(), data_->node(r), v, e))
    return false;
  auto s = tree_->state(v);
  FieldIdentity id;
  return s && s->alive && s->inside && s->bound &&
                 tree_->object_identity(v, id) &&
                 id.upstream_commit == data_->identity().upstream_commit &&
                 id.source_sha256 == data_->identity().source_sha256
             ? true
             : fail(e, "Player motion native child owner rejected");
}
bool PlayerMotionRuntime::action(T t, PlayerInputQuery q, bool &v,
                                 std::string &e) const {
  return host_.input(data_->text(t), q, v, e);
}
bool PlayerMotionRuntime::controls(std::string &e) {
  Vec2 v;
  bool climb;
  if (!host_.controls(v, e) || !finite(v) || !boolean(F::Climbing, climb, e))
    return fail(e, "Player controls actual vector unavailable");
  if (climb)
    v.x = 0;
  return set_vector(F::Input, v, e);
}
bool PlayerMotionRuntime::anim_play_pause(bool playing, bool idle,
                                          std::string &e) {
  if (!live(e) || !data_->business_bindings())
    return fail(e, "Player pause source policy unavailable");
  bool climb;
  if (!boolean(F::Climbing, climb, e))
    return false;
  const auto &p = data_->lifecycle();
  if (climb) {
    FieldObjectId animation;
    if (!node(R::AnimationPlayer, animation, e) ||
        !branch(host_.animation_speed, "climbing pause", e, animation,
                playing ? p.playing_scale : p.paused_scale))
      return false;
  } else if (idle && !ready_->set_anim_state(data_->text(T::IdleAnimation), e))
    return false;
  PlayerInitializationMember member;
  if (!body_->member(p.paused_animation, member, e) || member.kind != 4 ||
      !member.value)
    return fail(e, "Player paused animation actual String unavailable");
  auto paused = member.value->string;
  const std::string current = playing ? paused : graph_->current();
  auto loop =
      std::find_if(p.looped_animations.begin(), p.looped_animations.end(),
                   [&](const auto &v) { return v.first == current; });
  if (loop != p.looped_animations.end() && (playing || paused.empty())) {
    if (!graph_->set_scale(
            loop->second, float(playing ? p.playing_scale : p.paused_scale), e))
      return false;
    auto value = std::make_shared<GlobalYamlValue>();
    value->kind = 4;
    value->string = playing ? std::string{} : current;
    PlayerInitializationMember next;
    next.kind = 4;
    next.value = value;
    if (!body_->assign_member(p.paused_animation, next, e))
      return false;
  }
  return true;
}
bool PlayerMotionRuntime::collision_masks(bool enabled, std::string &e) {
  if (!data_->business_bindings())
    return fail(e, "Player collision mask source policy unavailable");
  for (auto bit : data_->lifecycle().collision_masks)
    if (!branch(host_.collision_mask, "collision mask", e, bit, enabled))
      return false;
  return true;
}
bool PlayerMotionRuntime::pause(bool stop_running, bool start_idle,
                                bool emit_signal, std::string &e) {
  if (!live(e) || !data_->business_bindings())
    return fail(e, "Player source pause policy unavailable");
  const auto &p = data_->lifecycle();
  if (!branch(host_.party_call, "pause Flash", e, p.pause_flash,
              std::vector<FieldDeferredValue>{}) ||
      !branch(host_.party_call, "stop AfterImage", e,
              data_->text(T::AfterimageStopMethod),
              std::vector<FieldDeferredValue>{}) ||
      !branch(host_.party_call, "pause party timers", e, p.pause_timers,
              std::vector<FieldDeferredValue>{}) ||
      !anim_play_pause(false, start_idle, e) ||
      !set_boolean(F::Crouch, false, e) || !set_boolean(F::Walking, false, e))
    return false;
  int64_t state;
  bool done;
  if (!integer(F::State, state, e) || !boolean(F::TakeoffDone, done, e))
    return false;
  if (!(state == data_->state(S::Teleporting) && done) &&
      !set_integer(F::State, data_->state(S::Move), e))
    return false;
  if (!set_boolean(F::Paused, true, e))
    return false;
  PlayerInitializationMember timer;
  if (!body_->member(p.takeoff_timer, timer, e) ||
      (timer.kind != 8 && timer.kind != 0))
    return fail(e, "Player takeoff timer actual owner unavailable");
  if (timer.object &&
      !branch(host_.timer_stop, "takeoff timer stop", e, timer.object))
    return false;
  FieldObjectId audio, misc;
  if (!set_vector(F::Input, {}, e) || !node(R::AudioPlayer, audio, e) ||
      !branch(host_.media_playing, "audio playing", e, audio, false) ||
      !branch(host_.media_paused, "audio stream paused", e, audio, true) ||
      !node(R::MiscTimer, misc, e) ||
      !branch(host_.timer_paused, "MiscTimer paused", e, misc, true))
    return false;
  bool tap;
  if (!boolean(F::TapRun, tap, e))
    return false;
  if ((stop_running || !tap) &&
      (!set_boolean(F::TapRun, false, e) || !set_running(false, e)))
    return false;
  return collision_masks(false, e) &&
         (!emit_signal ||
          host_.emit(body_->object(), data_->text(T::PausedSignal), {}, e));
}
bool PlayerMotionRuntime::unpause(bool emit_signal, std::string &e) {
  if (!live(e) || !data_->business_bindings())
    return fail(e, "Player source unpause policy unavailable");
  bool area;
  if (!branch(host_.current_scene_area, "current AreaRoom", e, area))
    return false;
  if (!area)
    return true;
  FieldObjectId audio, misc;
  const auto &p = data_->lifecycle();
  if (!anim_play_pause(true, false, e) || !node(R::AudioPlayer, audio, e) ||
      !branch(host_.media_paused, "audio stream paused", e, audio, false) ||
      !node(R::MiscTimer, misc, e) ||
      !branch(host_.timer_paused, "MiscTimer paused", e, misc, false) ||
      !branch(host_.party_call, "resume Flash", e, p.resume_flash,
              std::vector<FieldDeferredValue>{}) ||
      !branch(host_.party_call, "resume party timers", e, p.resume_timers,
              std::vector<FieldDeferredValue>{}) ||
      !set_boolean(F::Paused, false, e) || !collision_masks(true, e))
    return false;
  int64_t state;
  if (!integer(F::State, state, e))
    return false;
  if (state == data_->state(S::Teleporting)) {
    if (!branch(host_.party_call, "landing collisions", e,
                data_->text(T::CollisionsMethod),
                std::vector<FieldDeferredValue>{false}) ||
        !set_boolean(F::Running, true, e) ||
        !set_integer(F::State, data_->state(S::Landing), e) ||
        !set_number(F::Speed, data_->number(N::LandingSpeed), e))
      return false;
  } else if (!set_integer(F::State, data_->state(S::Move), e))
    return false;
  if (emit_signal &&
      !host_.emit(body_->object(), data_->text(T::UnpausedSignal), {}, e))
    return false;
  FieldObjectId ray, collider;
  if (!node(R::EventRay, ray, e) || !host_.cached_ray(ray, collider, e) ||
      !set_event_collider(collider, e))
    return false;
  bool running;
  if (!boolean(F::Running, running, e))
    return false;
  return !running || set_running(true, e);
}
bool PlayerMotionRuntime::collisions(bool enabled, std::string &e) {
  if (!live(e) || !data_->business_bindings())
    return fail(e, "Player collision source policy unavailable");
  FieldObjectId shape;
  if (!tree_->get_node(body_->object(), data_->lifecycle().collision_path,
                       shape, e))
    return false;
  return branch(host_.shape_disabled, "source collisions", e, shape, !enabled);
}
bool PlayerMotionRuntime::direction_and_input(Vec2 direction, std::string &e) {
  if (!live(e) || !finite(direction) || !set_vector(F::Input, direction, e) ||
      !set_vector(F::Direction, direction, e))
    return false;
  FieldObjectId ray;
  return node(R::EventRay, ray, e) &&
         host_.ray_rotation(ray,
                            float(std::atan2(direction.y, direction.x) -
                                  data_->number(N::RayAngleOffset)),
                            e) &&
         ready_->blend_position(direction, e);
}
bool PlayerMotionRuntime::exit_camera(std::string &e) {
  Vec2 direction;
  return live(e) && set_integer(F::State, data_->state(S::Move), e) &&
         vector(F::Direction, direction, e) &&
         ready_->blend_position(direction, e) &&
         ready_->set_anim_state(data_->text(T::IdleAnimation), e);
}
bool PlayerMotionRuntime::update_party_member(std::string &e) {
  return live(e) && ready_->update_party_member(e);
}
bool PlayerMotionRuntime::stop_run(std::string &e) {
  PlayerAudioVoice v;
  if (!host_.audio_voice({}, data_->text(T::RunVoice), v, e))
    return false;
  return !v.exists || !v.playing ||
         host_.audio_stop(data_->text(T::RunVoice), e);
}
bool PlayerMotionRuntime::set_running(bool enabled, std::string &e) {
  if (!set_boolean(F::Running, enabled, e))
    return false;
  bool paused, climb;
  int64_t state;
  if (!boolean(F::Paused, paused, e) || !boolean(F::Climbing, climb, e) ||
      !integer(F::State, state, e))
    return false;
  if (!paused && state == data_->state(S::Move) && enabled && !climb) {
    std::string sound;
    if (!text(F::RunSound, sound, e))
      return false;
    auto path = data_->text(T::FootstepsFormat);
    auto p = path.find("%s");
    if (p == path.npos)
      return fail(e, "Player footsteps format rejected");
    path.replace(p, 2, sound);
    bool exists;
    if (!host_.audio_resource(path, data_->text(T::RunVoice), exists, e) ||
        !exists)
      return fail(e, "Player actual footsteps resource unavailable");
    PlayerAudioVoice voice;
    if (!host_.audio_voice(path, data_->text(T::RunVoice), voice, e))
      return false;
    if (!voice.exists || !voice.same_stream || !voice.playing)
      if (!host_.audio_play(path, data_->text(T::RunVoice), e))
        return false;
  }
  return enabled || stop_run(e);
}
bool PlayerMotionRuntime::set_event_collider(FieldObjectId next,
                                             std::string &e) {
  FieldObjectId old;
  if (!object(F::EventCollider, old, e))
    return false;
  if (old == next)
    return true;
  if (next) {
    auto owner = registry_->tree_owner(next);
    auto state = owner ? owner->state(next) : nullptr;
    if (!state || !state->inside || !state->alive)
      return fail(e, "Player ray actual collider not live");
    bool connected;
    if (!host_.collider_connected(next, body_->object(),
                                  data_->text(T::ColliderMethod), connected,
                                  e) ||
        (!connected &&
         !host_.collider_connection(next, body_->object(),
                                    data_->text(T::ColliderMethod), true, e)) ||
        !host_.emit(body_->object(), data_->text(T::EnteredSignal),
                    {FieldObjectRef{next}}, e))
      return false;
  }
  if (old) {
    bool connected;
    if (!host_.collider_connected(old, body_->object(),
                                  data_->text(T::ColliderMethod), connected,
                                  e) ||
        (connected && !host_.collider_connection(old, body_->object(),
                                                 data_->text(T::ColliderMethod),
                                                 false, e)) ||
        !host_.emit(body_->object(), data_->text(T::ExitedSignal),
                    {FieldObjectRef{old}}, e))
      return false;
  }
  // Both synchronous signals observe the previous source member.
  return body_->assign_variant_node(data_->field(F::EventCollider), next, e);
}
bool PlayerMotionRuntime::move_state(float dt, std::string &e) {
  bool paused, door;
  if (!boolean(F::Paused, paused, e) ||
      !global_->boolean(FieldGlobalMemberRole::EnteringDoor, door, e))
    return false;
  if (paused || door)
    return stop_run(e);
  FieldObjectId ray, collider;
  if (!node(R::EventRay, ray, e) || !host_.cached_ray(ray, collider, e) ||
      !set_event_collider(collider, e) || !controls(e))
    return false;
  return movement(dt, e);
}
bool PlayerMotionRuntime::update_party_positions(Vec2 old, double multiplier,
                                                 std::string &e) {
  auto s = tree_->state(body_->object());
  if (!s)
    return fail(e, "Player local pose unavailable");
  Vec2 now = s->local[2];
  double count = std::round(
      std::max(std::abs(old.x - now.x), std::abs(old.y - now.y)) * multiplier);
  if (!std::isfinite(count) || count < 0 ||
      count > double(std::numeric_limits<int32_t>::max()))
    return fail(e, "Player partySpace count overflow");
  for (int32_t i = 0; i < int32_t(count); ++i) {
    double t = double(i + 1) / count;
    Vec2 p{float(old.x + (std::round(now.x) - old.x) * t),
           float(old.y + (std::round(now.y) - old.y) * t)};
    FieldGlobalPartySpaceValue removed;
    if (!global_->push_front_party_space(p, e) ||
        !global_->pop_back_party_space(removed, e))
      return false;
  }
  return true;
}
bool PlayerMotionRuntime::movement(float dt, std::string &e) {
  Vec2 input, direction, velocity, knockback;
  double speed;
  bool tap, climb, pressed, released, held, crouch, running, paused,
      substantial, spinning;
  FieldObjectId ray, anim, timer, crouch_timer, blink, character;
  if (!vector(F::Input, input, e) || !vector(F::Direction, direction, e) ||
      !number(F::Speed, speed, e) || !boolean(F::TapRun, tap, e) ||
      !boolean(F::Climbing, climb, e) || !boolean(F::Spinning, spinning, e) ||
      !boolean(F::Paused, paused, e) ||
      !boolean(F::Substantial, substantial, e) ||
      !boolean(F::Crouch, crouch, e) || !boolean(F::Running, running, e) ||
      !node(R::EventRay, ray, e) || !node(R::AnimationPlayer, anim, e) ||
      !node(R::Timer, timer, e) || !node(R::CrouchTimer, crouch_timer, e) ||
      !node(R::BlinkTimer, blink, e) || !object(F::PartyMember, character, e) ||
      !action(T::ToggleAction, PlayerInputQuery::Held, held, e) ||
      !action(T::ToggleAction, PlayerInputQuery::JustPressed, pressed, e) ||
      !action(T::ToggleAction, PlayerInputQuery::JustReleased, released, e))
    return false;
  auto crouch_start = [&]() {
    bool skill;
    if (!host_.has_skill(character, data_->text(T::TeleportSkill), skill, e))
      return false;
    return !skill || host_.timer_start(crouch_timer,
                                       data_->number(N::TimerDefaultTime), e);
  };
  if (!zero(input) || tap) {
    velocity = mul(tap ? direction : input, speed);
    if (!set_vector(F::Velocity, velocity, e))
      return false;
    if (!zero(input)) {
      direction = input;
      if (!set_vector(F::Direction, direction, e) ||
          !host_.ray_rotation(ray,
                              std::atan2(direction.y, direction.x) -
                                  float(data_->number(N::RayAngleOffset)),
                              e) ||
          !host_.emit(body_->object(), data_->text(T::MovedSignal), {}, e))
        return false;
    }
    // Source moved observers run synchronously and can change these members.
    if (!boolean(F::Climbing, climb, e) || !boolean(F::Crouch, crouch, e) ||
        !boolean(F::Running, running, e) || !boolean(F::Paused, paused, e) ||
        !boolean(F::Substantial, substantial, e) || !boolean(F::TapRun, tap, e))
      return false;
    if (climb && !branch(host_.animation_speed,"animation_speed",e,anim, 1))
      return false;
    if (held || tap) {
      if (pressed && !crouch && !running && !climb) {
        crouch = true;
        if (!set_boolean(F::Crouch, true, e) || !crouch_start())
          return false;
      }
      if (pressed && tap) {
        tap = false;
        if (!set_boolean(F::TapRun, false, e) || !set_running(false, e))
          return false;
      }
      if (!paused && substantial && !set_running(true, e))
        return false;
      if (!ready_->set_anim_state(data_->text(T::RunAnimation), e) ||
          !graph_->set_scale(data_->text(T::FaintedWalkScale),
                             data_->number(N::FaintedRunScale), e) ||
          !set_number(F::Speed, data_->number(N::RunSpeed), e))
        return false;
    } else {
      if (!set_number(F::Speed, data_->number(N::WalkSpeed), e) ||
          !set_boolean(F::Crouch, false, e))
        return false;
      if (!climb &&
          (!ready_->set_anim_state(data_->text(T::WalkAnimation), e) ||
           !graph_->set_scale(data_->text(T::FaintedWalkScale),
                              data_->number(N::FaintedWalkScale), e)))
        return false;
      if (released && !tap && !set_running(false, e))
        return false;
      if (!stop_run(e))
        return false;
    }
    if (!spinning && !ready_->blend_position(direction, e))
      return false;
  } else {
    if (!set_vector(F::Velocity, {}, e) || !set_boolean(F::Walking, false, e))
      return false;
    if (released && crouch) {
      bool done;
      if (!boolean(F::CrouchDone, done, e) || !set_boolean(F::Crouch, false, e))
        return false;
      if (done) {
        if (!start_teleport(data_->manual_teleport(), e))
          return false;
      } else if (!set_number(F::Speed, data_->number(N::RunSpeed), e) ||
                 !set_boolean(F::TapRun, true, e) ||
                 !set_vector(F::Velocity,
                             mul(direction, data_->number(N::RunSpeed)), e))
        return false;
    }
  }
  auto state = tree_->state(body_->object());
  if (!state)
    return fail(e, "Player Kinematic body unavailable");
  Vec2 old = state->local[2], returned;
  if (!vector(F::Velocity, velocity, e) || !number(F::Speed, speed, e) ||
      !host_.move_and_slide(
          body_->object(),
          mul(velocity, dt * (speed / data_->number(N::MovementDivisor))),
          returned, e) ||
      !set_vector(F::Velocity, returned, e) ||
      !vector(F::Knockback, knockback, e) ||
      !host_.move_and_slide(body_->object(), knockback, returned, e) ||
      !set_vector(F::Knockback, returned, e))
    return false;
  state = tree_->state(body_->object());
  Vec2 pos = state->local[2];
  substantial = std::max(std::round(std::abs(old.x - pos.x)),
                         std::round(std::abs(old.y - pos.y))) > 0 ||
                std::abs(returned.x) > data_->number(N::KnockbackThreshold) ||
                std::abs(returned.y) > data_->number(N::KnockbackThreshold);
  if (!set_boolean(F::Substantial, substantial, e) ||
      (substantial && !set_boolean(F::Idle, false, e)))
    return false;
  if (substantial) {
    if (!set_boolean(F::Walking, true, e) ||
        !set_boolean(F::Crouch, false, e) ||
        !update_party_positions(old, 1, e) || !boolean(F::Running, running, e))
      return false;
    double left;
    if (!host_.timer_left(timer, left, e))
      return false;
    if (running && left == 0 &&
        (!host_.timer_start(timer, data_->number(N::TimerDefaultTime), e) ||
         (!climb && !host_.dust(e))))
      return false;
  } else {
    if (!ready_->set_anim_state(data_->text(T::IdleAnimation), e) ||
        !set_boolean(F::Walking, false, e) ||
        !set_boolean(F::TapRun, false, e) || !set_running(false, e) ||
        (climb && !branch(host_.animation_speed,"animation_speed",e,anim, 0)) ||
        !boolean(F::Crouch, crouch, e))
      return false;
    if (pressed && !crouch) {
      if (!host_.emit(body_->object(), data_->text(T::MovedSignal), {}, e) ||
          !set_boolean(F::Crouch, true, e) || !crouch_start())
        return false;
    } else if (pressed && crouch && !set_boolean(F::Crouch, false, e))
      return false;
    if (!boolean(F::Crouch, crouch, e))
      return false;
    if (crouch) {
      if (!graph_->travel(data_->text(T::CrouchAnimation), e))
        return false;
    } else {
      double left;
      bool idle;
      if (!host_.timer_left(blink, left, e) || !boolean(F::Idle, idle, e))
        return false;
      if (left == 0) {
        if (!idle || spinning) {
          if (!ready_->set_anim_state(data_->text(T::IdleAnimation), e))
            return false;
          double wait = data_->number(N::BlinkFrom) +
                        random_->randf() * data_->number(N::BlinkSpan);
          if (!host_.timer_wait(blink, wait, e) ||
              !host_.timer_start(blink, data_->number(N::TimerDefaultTime), e))
            return false;
        } else if (!ready_->set_anim_state(data_->text(T::BlinkAnimation), e))
          return false;
      }
    }
  }
  auto transform = tree_->state(body_->object())->local;
  transform[2] = {std::round(transform[2].x), std::round(transform[2].y)};
  if (!tree_->set_local(body_->object(), transform, e))
    return false;
  std::shared_ptr<const GlobalLoadObjectArray> objects, party;
  if (!global_->array(FieldGlobalMemberRole::PartyObjects, objects, e) ||
      !global_->array(FieldGlobalMemberRole::Party, party, e))
    return false;
  bool can_climb = true;
  for (auto id : objects->values) {
    bool value;
    if (!host_.is_climbing(id, value, e))
      return false;
    if (value)
      can_climb = false;
  }
  if (can_climb && party->values.size() != 1) {
    bool relay;
    if (!host_.has_skill(character, data_->text(T::RelaySkill), relay, e))
      return false;
    if (relay) {
      bool next, prev;
      if (!action(T::NextAction, PlayerInputQuery::JustPressed, next, e) ||
          !action(T::PreviousAction, PlayerInputQuery::JustPressed, prev, e) ||
          (next && !branch(host_.swap_spin,"swap_spin",e,1)) || (prev && !branch(host_.swap_spin,"swap_spin",e,-1)))
        return false;
    }
  }
  bool accept, can_interact, door;
  if (!action(T::AcceptAction, PlayerInputQuery::JustPressed, accept, e) ||
      !boolean(F::Paused, paused, e) ||
      !boolean(F::CanInteract, can_interact, e) ||
      !global_->boolean(FieldGlobalMemberRole::EnteringDoor, door, e))
    return false;
  FieldObjectId collider;
  if (accept && !paused && can_interact && !door) {
    if (!host_.cached_ray(ray, collider, e))
      return false;
    if (collider) {
      if (!boolean(F::Crouch, crouch, e))
        return false;
      return crouch ? use_telepathy(e) : interact_with(e);
    }
  }
  return true;
}
bool PlayerMotionRuntime::press_prompt(FieldObjectId source, std::string &e) {
  auto owner = registry_->tree_owner(source);
  auto state = owner ? owner->state(source) : nullptr;
  if (!state || !state->alive)
    return fail(e, "Player prompt actual source unavailable");
  for (auto child : state->children) {
    auto c = owner->state(child);
    if (!c || !c->alive)
      return fail(e, "Player prompt actual child unavailable");
    if (c->name == data_->text(T::PromptPath))
      return branch(host_.press_prompt,"press_prompt",e,child);
  }
  return true;
}
bool PlayerMotionRuntime::interact_with(std::string &e) {
  bool empty;
  if (!live(e) || !branch(host_.ui_stack_empty,"ui_stack_empty",e,empty))
    return false;
  if (!empty)
    return true;
  if (!set_event_collider(0, e))
    return false;
  FieldObjectId ray, collider;
  if (!node(R::EventRay, ray, e) || !host_.cached_ray(ray, collider, e))
    return false;
  if (!collider)
    return branch(host_.dialogue,"dialogue",e,data_->text(T::NoProblemDialogue));
  PlayerColliderInfo info;
  if (!host_.collider_info(collider, info, e))
    return false;
  if (info.name.find(data_->text(T::InteractNameToken)) != std::string::npos) {
    for (auto target : {collider, info.parent}) {
      if (!target)
        continue;
      PlayerColliderInfo c;
      if (!host_.collider_info(target, c, e))
        return false;
      if (!c.interact)
        continue;
      if (c.has_dialog && !c.dialog)
        return true;
      if (!branch(host_.party_turn,"party_turn",e,target) || !branch(host_.interact,"interact",e,target))
        return false;
      break;
    }
    return press_prompt(collider, e);
  }
  return info.area || branch(host_.dialogue,"dialogue",e,data_->text(T::NoProblemDialogue));
}
bool PlayerMotionRuntime::use_telepathy(std::string &e) {
  FieldObjectId character, ray, collider;
  if (!live(e) || !object(F::PartyMember, character, e))
    return false;
  bool skill;
  if (!host_.has_skill(character, data_->text(T::TelepathySkill), skill, e))
    return false;
  if (!skill)
    return branch(host_.dialogue,"dialogue",e,data_->text(T::NothingDialogue));
  if (!set_event_collider(0, e) || !node(R::EventRay, ray, e) ||
      !host_.cached_ray(ray, collider, e))
    return false;
  if (!collider)
    return branch(host_.dialogue,"dialogue",e,data_->text(T::NoProblemDialogue));
  PlayerColliderInfo c, p;
  if (!host_.collider_info(collider, c, e) ||
      (c.parent && !host_.collider_info(c.parent, p, e)))
    return false;
  FieldObjectId target = p.telepathy ? c.parent : (c.telepathy ? collider : 0);
  if (!target)
    return branch(host_.dialogue,"dialogue",e,data_->text(T::NoThoughtsDialogue)) &&
           press_prompt(collider, e);
  auto &info = target == collider ? c : p;
  if (!branch(host_.player_turn,"player_turn",e,target))
    return false;
  if (info.has_thoughts) {
    if (target != collider && !branch(host_.telepathy_effect,"telepathy_effect",e,target, true))
      return false;
    return branch(host_.telepathy,"telepathy",e,target) && press_prompt(collider, e);
  }
  if (target == collider || info.no_problem_thoughts)
    return branch(host_.dialogue,"dialogue",e,data_->text(T::NoThoughtsDialogue));
  return branch(host_.dialogue,"dialogue",e,data_->text(T::StrayThoughtsDialogue)) &&
         press_prompt(collider, e);
}
bool PlayerMotionRuntime::turn_to(Vec2 relative,bool axis_x,bool axis_y,std::string &e) {
  if(!live(e)||!finite(relative)) return false;
  if(!axis_x&&!axis_y) return true;
  Vec2 direction=relative;
  auto sign=[](float value){return value>0?1.0f:value<0?-1.0f:0.0f;};
  if((std::abs(relative.x)>std::abs(relative.y)||!axis_y)&&relative.x!=0&&axis_x)
    direction={sign(relative.x),0};
  else if(axis_y&&relative.y!=0)direction={0,sign(relative.y)};
  if(!set_vector(F::Direction,direction,e))return false;
  // Source _turn_to blends only Idle, then calls the original state adapter.
  auto states=graph_->data();
  if(!states)return fail(e,"Player source Idle graph missing");
  auto idle=std::find_if(states->states().begin(),states->states().end(),[&](const auto &state){return state.name==data_->text(T::IdleAnimation);});
  if(idle==states->states().end())return fail(e,"Player source Idle blend state missing");
  if(!zero(direction)&&!graph_->set_blend(idle->parameter,direction,e))return false;
  return ready_->set_anim_state(data_->text(T::IdleAnimation),e);
}
bool PlayerMotionRuntime::calculate_steps(std::string &e) {
  Vec2 previous;
  if (!vector(F::LastStep, previous, e))
    return false;
  double distance;
  if (!number(F::StepDistance, distance, e))
    return false;
  auto pos = tree_->state(body_->object())->local[2];
  distance += std::hypot(pos.x - previous.x, pos.y - previous.y);
  if (!set_number(F::StepDistance, distance, e))
    return false;
  if (distance < data_->number(N::StepDistance))
    return true;
  int64_t steps;
  if (!integer(F::Steps, steps, e) ||
      steps == std::numeric_limits<int64_t>::max())
    return fail(e, "Player source step counter overflow");
  ++steps;
  if (!set_integer(F::Steps, steps, e) || !set_number(F::StepDistance, 0, e) ||
      !set_vector(F::LastStep, pos, e))
    return false;
  FieldObjectId character;
  std::vector<PlayerDamageEffect> effects;
  if (!object(F::PartyMember, character, e) ||
      !host_.damage_effects(character, data_->text(T::DamageEffect), effects,
                            e))
    return false;
  for (const auto &v : effects) {
    if (v.steps == 0)
      return fail(e, "Player source damage steps divisor zero");
    if (steps == std::numeric_limits<int64_t>::min() && v.steps == -1)
      return fail(e, "Player source damage modulo overflow");
    if (steps % v.steps == 0 &&
        !branch(host_.damage,"damage",e,v.value, v.variation, Vec2{}, true))
      return false;
  }
  return true;
}
bool PlayerMotionRuntime::physics_tail(std::string &e) {
  int64_t state, mode;
  if (!integer(F::State, state, e) || !integer(F::TeleportMode, mode, e))
    return false;
  if (state != data_->state(S::Move) && state != data_->state(S::Teleporting) &&
      state != data_->state(S::Landing) && !set_running(false, e))
    return false;
  if ((state == data_->state(S::Move) ||
       (state == data_->state(S::Teleporting) &&
        mode == data_->manual_teleport())) &&
      !calculate_steps(e))
    return false;
  bool damage, paused;
  if (!boolean(F::ContinuousDamage, damage, e) ||
      !boolean(F::Paused, paused, e))
    return false;
  Vec2 hit, k;
  if (damage && !paused) {
    if (!vector(F::HitDirection, hit, e) ||
        !set_vector(F::Knockback, mul(hit, data_->number(N::KnockbackScale)),
                    e))
      return false;
    FieldObjectId flash;
    std::string animation;
    if (!node(R::FlashAnimation, flash, e) ||
        !host_.animation_current(flash, animation, e))
      return false;
    if (animation != data_->text(T::InvulnerableAnimation)) {
      int64_t value, variance;
      if (!integer(F::AttackDamage, value, e) ||
          !integer(F::DamageVariance, variance, e) ||
          !branch(host_.damage,"damage",e,value, variance, hit, true))
        return false;
    }
  }
  if (!vector(F::Knockback, k, e))
    return false;
  double length = std::hypot(k.x, k.y), step = data_->number(N::KnockbackDecay);
  if (!set_vector(F::Knockback,
                  length <= step ? Vec2{} : mul(k, 1 - step / length), e))
    return false;
  bool debug;
  if (!boolean(F::DebugSpeed, debug, e))
    return false;
  return !debug || set_number(F::Speed, data_->number(N::DebugSpeed), e);
}
bool PlayerMotionRuntime::physics(float dt, const FieldNodeBinding &binding,
                                  std::string &e) {
  if (!live(e) || !std::isfinite(dt) || dt < 0 ||
      tree_->state(body_->object())->binding.stable_id != binding.stable_id ||
      tree_->state(body_->object())->binding.family != binding.family ||
      tree_->state(body_->object())->binding.script_sha != binding.script_sha ||
      tree_->state(body_->object())->binding.class_index !=
          binding.class_index ||
      tree_->state(body_->object())->binding.capability != binding.capability ||
      tree_->state(body_->object())->binding.native_class !=
          binding.native_class ||
      tree_->state(body_->object())->binding.identity.scene_id !=
          binding.identity.scene_id ||
      tree_->state(body_->object())->binding.identity.upstream_commit !=
          binding.identity.upstream_commit ||
      tree_->state(body_->object())->binding.identity.source_sha256 !=
          binding.identity.source_sha256)
    return fail(e, "Player actual physics source binding rejected");
  int64_t state;
  if (!integer(F::State, state, e))
    return false;
  bool ok = true;
  if (state == data_->state(S::Move))
    ok = move_state(dt, e);
  else if (state == data_->state(S::Teleporting))
    ok = teleport_state(dt, e);
  else if (state == data_->state(S::Landing))
    ok = landing_state(dt, e);
  else if (state == data_->state(S::AttackPrep))
    ok = attack_hold(e);
  else if (state == data_->state(S::Soot))
    ok = soot_state(e);
  else if (state == data_->state(S::Camera)) {
    FieldObjectId camera;
    Vec2 input, offset;
    ok = controls(e) &&
         ready_->set_anim_state(data_->text(T::IdleAnimation), e) &&
         vector(F::Input, input, e);
    if (ok && !zero(input))
      ok = node(R::Camera, camera, e) && host_.camera_offset &&
           host_.camera_offset(camera, offset, e) &&
           ready_->blend_position(offset, e);
  } else if (state == data_->state(S::Bouncing)) {
    Vec2 direction, returned;
    double speed;
    ok =
        vector(F::Direction, direction, e) && number(F::Speed, speed, e) &&
        set_vector(F::Velocity,
                   mul(direction, -speed * data_->number(N::BounceScale)), e) &&
        host_.move_and_slide(
            body_->object(),
            mul(direction, -speed * data_->number(N::BounceScale)), returned,
            e);
  } else if (state != data_->state(S::Attack) &&
             state != data_->state(S::Jumping))
    ok = fail(e, "Unknown Player source motion state");
  if (ok)
    ok = physics_tail(e);
  if (!ok)
    poisoned_ = true;
  return ok;
}

bool PlayerMotionRuntime::input(const PlayerInputEvent &event, std::string &e) {
  bool paused, climbing, spinning, door;
  if (!live(e) || !boolean(F::Paused, paused, e) ||
      !boolean(F::Climbing, climbing, e) ||
      !boolean(F::Spinning, spinning, e) ||
      !global_->boolean(FieldGlobalMemberRole::EnteringDoor, door, e))
    return false;
  int64_t state;
  if (!integer(F::State, state, e))
    return false;
  if (paused || state != data_->state(S::Move) || door)
    return true;
  auto pressed = [&](T x) {
    return std::find(event.pressed.begin(), event.pressed.end(),
                     data_->text(x)) != event.pressed.end();
  };
  if (!event.echo && pressed(T::CancelAction) && !climbing && !spinning) {
    FieldObjectId character;
    std::vector<std::string> actions;
    std::string current;
    if (!object(F::PartyMember, character, e) ||
        !host_.button_skills(character, actions, e) ||
        !text(F::CurrentSkill, current, e))
      return false;
    std::sort(actions.begin(), actions.end());
    if (actions.empty())
      current.clear();
    else if (std::find(actions.begin(), actions.end(), current) ==
             actions.end())
      current = actions.front();
    if (!set_text(F::CurrentSkill, current, e) || !do_attack(e))
      return false;
  }
  if (pressed(T::ScopeAction)) {
    bool damage, can_pause;
    if (!boolean(F::ContinuousDamage, damage, e) ||
        !global_->boolean(FieldGlobalMemberRole::CanPause, can_pause, e))
      return false;
    if (!damage && can_pause &&
        !set_integer(F::State, data_->state(S::Camera), e))
      return false;
  }
  return true;
}
bool PlayerMotionRuntime::do_attack(std::string &e) {
  std::string skill;
  if (!text(F::CurrentSkill, skill, e))
    return false;
  bool swing = skill == data_->text(T::SwingSkill),
       beam = skill == data_->text(T::BeamSkill),
       cast = skill == data_->text(T::FireSkill) ||
              skill == data_->text(T::FreezeSkill) ||
              skill == data_->text(T::ThunderSkill);
  if (cast)
    return fail(e, "Player PKOV complete source factory/coroutine pending "
                   "before attack mutation");
  if (!swing && !beam && !cast)
    return skill.empty() ? true
                         : fail(e, "Unknown source button skill capability");
  if (!host_.admit_special || !host_.admit_special(skill, e))
    return fail(e, "Player actual attack owner pending");
  FieldObjectId timer, outline;
  if (swing) {
    if (!set_integer(F::State, data_->state(S::Attack), e) ||
        !set_boolean(F::Crouch, false, e) || !set_boolean(F::Idle, false, e) ||
        !set_boolean(F::TapRun, false, e) || !set_running(false, e) ||
        !set_boolean(F::Walking, false, e))
      return false;
    return attack_unleash(e);
  }
  if (beam) {
    if (!node(R::AimTimer, timer, e) ||
        !host_.timer_start(timer, data_->number(N::TimerDefaultTime), e) ||
        !set_integer(F::State, data_->state(S::AttackPrep), e) ||
        !set_boolean(F::Crouch, false, e) || !set_boolean(F::Idle, false, e) ||
        !set_boolean(F::TapRun, false, e) || !set_running(false, e) ||
        !set_boolean(F::Walking, false, e))
      return false;
  } else if (!graph_->travel(data_->text(T::CastAnimation), e) ||
             !set_integer(F::State, data_->state(S::AttackPrep), e) ||
             !node(R::PkTimer, timer, e) ||
             !host_.timer_wait(timer, data_->number(N::PkAimSeconds), e) ||
             !host_.timer_start(timer, data_->number(N::TimerDefaultTime), e) ||
             !node(R::OutlineAnimation, outline, e) || !host_.animation_play ||
             !host_.animation_play(outline, data_->text(T::FlashAnimation), e))
    return false;
  return true;
}
bool PlayerMotionRuntime::attack_hold(std::string &e) {
  Vec2 old, input, direction;
  FieldObjectId character, aim, camera, ray;
  double left;
  std::string lead;
  if (!vector(F::Direction, old, e) || !object(F::PartyMember, character, e) ||
      !host_.character_name || !host_.character_name(character, lead, e) ||
      !node(R::AimTimer, aim, e) || !host_.timer_left(aim, left, e) ||
      (left == 0 && !controls(e)) || !vector(F::Input, input, e))
    return false;
  if (!zero(input)) {
    if (!set_vector(F::Direction, input, e) || !node(R::EventRay, ray, e) ||
        !host_.ray_rotation(ray,
                            std::atan2(input.y, input.x) -
                                float(data_->number(N::RayAngleOffset)),
                            e) ||
        !ready_->blend_position(input, e))
      return false;
  }
  if (!vector(F::Direction, direction, e))
    return false;
  if (lead == data_->attack_character_names()[0]) {
    if (!graph_->travel(data_->text(T::ShootHoldAnimation), e))
      return false;
    if (left == 0 && !equal(old, direction)) {
      if (!node(R::Camera, camera, e) || !host_.camera_move ||
          !host_.camera_move(
              camera,
              {float(direction.x * data_->number(N::CameraOffsetX)),
               float(direction.y * data_->number(N::CameraOffsetY))},
              data_->number(N::CameraOffsetTime), e))
        return false;
    }
  } else if (lead == data_->attack_character_names()[1]) {
    if (!graph_->travel(data_->text(T::CastHoldAnimation), e))
      return false;
  } else if (!set_integer(F::State, data_->state(S::Move), e))
    return false;
  bool released;
  if (!action(T::CancelAction, PlayerInputQuery::JustReleased, released, e))
    return false;
  return !released || attack_unleash(e);
}
bool PlayerMotionRuntime::attack_unleash(std::string &e) {
  std::string skill;
  if (!text(F::CurrentSkill, skill, e) || !host_.admit_special ||
      !host_.admit_special(skill, e))
    return fail(e, "Player attack concrete owners unavailable");
  if (!set_integer(F::State, data_->state(S::Attack), e))
    return false;
  FieldObjectId timer, camera, outline;
  if (skill == data_->text(T::SwingSkill)) {
    bool incapacitated;
    FieldObjectId character;
    if (!object(F::PartyMember, character, e) || !host_.incapacitated ||
        !host_.incapacitated(character, incapacitated, e) ||
        !host_.audio_play(data_->text(T::BatAudio), data_->node(R::AudioPlayer),
                          e) ||
        !host_.bat_feedback || !host_.bat_feedback(e) ||
        !graph_->travel(data_->text(T::BatAnimation), e) ||
        !graph_->set_scale(data_->text(T::BatScale),
                           data_->number(incapacitated
                                             ? N::BatIncapacitatedScale
                                             : N::BatNormalScale),
                           e))
      return false;
  } else if (skill == data_->text(T::BeamSkill)) {
    Vec2 direction;
    if (!vector(F::Direction, direction, e) ||
        !graph_->set_blend(data_->text(T::ShootBlend), direction, e) ||
        !graph_->travel(data_->text(T::ShootAnimation), e) ||
        !node(R::Camera, camera, e) || !host_.camera_return ||
        !host_.camera_return(camera, data_->number(N::CameraReturnTime), e) ||
        !node(R::AimTimer, timer, e) || !host_.timer_stop ||
        !host_.timer_stop(timer, e))
      return false;
  } else if (skill == data_->text(T::FireSkill) ||
             skill == data_->text(T::FreezeSkill) ||
             skill == data_->text(T::ThunderSkill)) {
    if (!graph_->travel(data_->text(T::CastAnimation), e) ||
        !node(R::OutlineAnimation, outline, e) || !host_.animation_play ||
        !host_.animation_play(outline, data_->text(T::NormalAnimation), e) ||
        !node(R::PkTimer, timer, e) || !host_.timer_stop ||
        !host_.timer_stop(timer, e))
      return false;
  } else if (!set_integer(F::State, data_->state(S::Move), e))
    return false;
  return true;
}
bool PlayerMotionRuntime::start_teleport(int64_t mode, std::string &e) {
  auto it = std::find_if(data_->teleport_modes().begin(),
                         data_->teleport_modes().end(),
                         [&](const auto &m) { return m.id == mode; });
  if (!live(e) || it == data_->teleport_modes().end() || !host_.admit_special ||
      !host_.admit_special(data_->text(T::TeleportSkill), e))
    return fail(e, "Player teleport concrete source owners unavailable");
  if (it->takeoff > 0)
    return fail(e, "Player takeoff SceneTreeTimer/pause owner pending before "
                   "teleport mutation");
  FieldObjectId outline;
  Vec2 direction;
  if (!set_number(F::Speed, data_->number(N::RunSpeed), e) ||
      !set_running(true, e) ||
      !set_integer(F::State, data_->state(S::Teleporting), e) ||
      !set_boolean(F::CrouchDone, false, e) ||
      !set_boolean(F::MaxSpeed, false, e) ||
      !set_boolean(F::TakeoffDone, false, e) ||
      !set_integer(F::TeleportMode, mode, e) ||
      !vector(F::Direction, direction, e) ||
      !set_vector(F::Velocity, mul(direction, data_->number(N::RunSpeed)), e) ||
      !node(R::OutlineAnimation, outline, e) || !host_.animation_play ||
      !host_.animation_play(outline, data_->text(T::NormalAnimation), e))
    return false;
  return true;
}
bool PlayerMotionRuntime::teleport_state(float, std::string &e) {
  bool paused;
  if (!boolean(F::Paused, paused, e))
    return false;
  FieldTransform world;
  if (!tree_->world_transform(body_->object(), world, e))
    return false;
  Vec2 old = world[2];
  if (paused)
    return fail(e, "Source teleport old_pos unassigned while paused");
  if (!host_.party_call || !controls(e))
    return fail(e, "Player teleport native party owners pending");
  Vec2 input, direction;
  bool takeoff, maxspeed;
  int64_t mode;
  double speed;
  if (!vector(F::Input, input, e) || !vector(F::Direction, direction, e) ||
      !boolean(F::TakeoffDone, takeoff, e) ||
      !boolean(F::MaxSpeed, maxspeed, e) ||
      !integer(F::TeleportMode, mode, e) || !number(F::Speed, speed, e))
    return false;
  auto p = std::find_if(data_->teleport_modes().begin(),
                        data_->teleport_modes().end(),
                        [&](const auto &v) { return v.id == mode; });
  if (p == data_->teleport_modes().end())
    return fail(e, "Player teleport mode unknown");
  auto reverse = mul(direction, -1);
  double angle = data_->number(N::RayAngleOffset) / 2;
  auto rotate = [&](double a) {
    return Vec2{
        std::round(float(reverse.x * std::cos(a) - reverse.y * std::sin(a))),
        std::round(float(reverse.x * std::sin(a) + reverse.y * std::cos(a)))};
  };
  if (!takeoff && !zero(input) && !equal(input, reverse) &&
      !equal(input, rotate(angle)) && !equal(input, rotate(-angle))) {
    direction = input;
    FieldObjectId ray;
    if (!set_vector(F::Direction, direction, e) || !node(R::EventRay, ray, e) ||
        !host_.ray_rotation(ray,
                            std::atan2(direction.y, direction.x) -
                                float(data_->number(N::RayAngleOffset)),
                            e) ||
        !ready_->blend_position(direction, e))
      return false;
  }
  FieldObjectId timer;
  if (!node(R::TakeoffTimer, timer, e))
    return false;
  if (!maxspeed) {
    if (speed < p->cap)
      speed += p->acceleration;
    else {
      speed = p->cap;
      if (!set_number(F::Speed, speed, e) ||
          !set_boolean(F::MaxSpeed, true, e) ||
          !host_.party_call(data_->text(T::PlayFlashMethod),
                            {data_->text(T::TeleportPulse)}, e) ||
          !host_.party_call(data_->text(T::AfterimageStartMethod), {}, e) ||
          (p->takeoff > 0 && !host_.timer_start(timer, p->takeoff, e)))
        return false;
    }
  } else if (takeoff)
    speed *= p->multiplier;
  if (!set_number(F::Speed, speed, e))
    return false;
  Vec2 returned;
  if (!set_vector(F::Velocity, mul(direction, speed), e) ||
      !host_.move_and_slide(body_->object(), mul(direction, speed), returned,
                            e) ||
      !ready_->set_anim_state(data_->text(T::RunAnimation), e) ||
      !graph_->set_scale(data_->text(T::FaintedWalkScale),
                         data_->number(N::FaintedRunScale), e) ||
      !tree_->world_transform(body_->object(), world, e))
    return false;
  bool toggle;
  if (!action(T::ToggleAction, PlayerInputQuery::JustPressed, toggle, e))
    return false;
  if ((mode == data_->manual_teleport() && toggle) ||
      (!takeoff &&
       std::max(std::abs(old.x - world[2].x), std::abs(old.y - world[2].y)) <=
           data_->number(N::CrashDistance))) {
    FieldObjectId crouch, camera, resource;
    if (!set_running(false, e) || !set_boolean(F::Substantial, false, e) ||
        !set_boolean(F::Idle, true, e) ||
        !set_integer(F::State, data_->state(S::Soot), e) ||
        !set_boolean(F::MaxSpeed, false, e) || !host_.timer_stop ||
        !host_.timer_stop(timer, e) ||
        !graph_->travel(data_->text(T::SootAnimation), e) ||
        !node(R::CrouchTimer, crouch, e) ||
        !host_.timer_start(crouch, data_->number(N::TimerDefaultTime), e) ||
        !host_.party_call(data_->text(T::PlayFlashMethod),
                          {data_->text(T::ResetFlash)}, e) ||
        !host_.party_call(data_->text(T::AfterimageStopMethod), {}, e) ||
        !node(R::Camera, camera, e) || !host_.camera_shake ||
        !host_.camera_shake(camera, speed / data_->number(N::CrashShakeDivisor),
                            data_->number(N::CrashShakeTime), direction, e) ||
        !object(F::CrashResource, resource, e) || !host_.audio_play_resource ||
        !host_.audio_play_resource(resource, data_->text(T::CrashVoice), e))
      return false;
  }
  return update_party_positions(old, data_->number(N::PartyTeleportDistance),
                                e);
}
bool PlayerMotionRuntime::landing_state(float dt, std::string &e) {
  FieldTransform world;
  Vec2 direction, returned;
  double speed;
  if (!tree_->world_transform(body_->object(), world, e) ||
      !number(F::Speed, speed, e) || !vector(F::Direction, direction, e))
    return false;
  speed -= data_->number(N::LandingSlowdown) * dt;
  if (!set_number(F::Speed, speed, e) ||
      !set_vector(F::Velocity, mul(direction, speed), e) ||
      !host_.move_and_slide(body_->object(), mul(direction, speed), returned,
                            e) ||
      !update_party_positions(world[2], 1, e))
    return false;
  if (speed <= data_->number(N::WalkSpeed)) {
    if (!host_.party_call ||
        !host_.party_call(data_->text(T::CollisionsMethod), {true}, e) ||
        !set_integer(F::State, data_->state(S::Move), e))
      return false;
  }
  return true;
}
bool PlayerMotionRuntime::soot_state(std::string &e) {
  bool paused;
  FieldObjectId timer;
  double left;
  if (!boolean(F::Paused, paused, e) || !node(R::CrouchTimer, timer, e) ||
      !host_.timer_left(timer, left, e))
    return false;
  if (paused || left != 0)
    return true;
  Vec2 input;
  if (!controls(e) || !vector(F::Input, input, e))
    return false;
  return zero(input) || (set_vector(F::Direction, input, e) &&
                         set_integer(F::State, data_->state(S::Move), e));
}
// Projectile/coroutine factories remain explicit typed native owners. No packed
// scene is admitted by the presence of a callback alone.
bool PlayerMotionRuntime::projectile_shoot(std::string &e) {
  if (!live(e) || !host_.admit_special ||
      !host_.admit_special(data_->text(T::ShootMethod), e) ||
      !host_.projectile_instance || !host_.beam_setup)
    return fail(e, "Player Beam factory/native lifecycle pending");
  FieldObjectId resource, spawn, current, parent;
  if (!object(F::PartyMember, current, e) || !node(R::BulletSpawn, spawn, e))
    return false;
  PlayerInitializationMember member;
  if (!body_->member(data_->text(T::BeamMember), member, e) || member.kind != 8)
    return fail(e, "Player actual Beam resource unbound");
  resource = member.object;
  std::shared_ptr<FieldNodeTreeRuntime> created;
  FieldObjectId root;
  if (!host_.projectile_instance(resource, created, root, e) || !created ||
      registry_->tree_owner(root).get() != created.get() ||
      !global_->object(FieldGlobalMemberRole::CurrentScene, current, e))
    return fail(e, "Player Beam source factory owner rejected");
  auto scene = registry_->tree_owner(current);
  if (!scene ||
      !scene->get_node(current, data_->text(T::SceneObjects), parent, e) ||
      !created->add_child(parent, root, e))
    return false;
  FieldObjectId head;
  if (!created->get_node(root, data_->text(T::BeamHead), head, e))
    return false;
  auto state = created->state(head);
  if (!state || !state->inside || !state->bound || state->ready_first)
    return fail(e, "Player Beam complete native/script Ready pending");
  FieldTransform pose;
  Vec2 direction;
  if (!tree_->world_transform(spawn, pose, e) ||
      !vector(F::Direction, direction, e) ||
      !host_.beam_setup(head, direction, pose[2],
                        std::atan2(pose[0].y, pose[0].x), e))
    return false;
  return created->set_visible(root, true, e);
}
bool PlayerMotionRuntime::projectile_cast(std::string &e) {
  return fail(e, "Player PKOV native/script/coroutine factory pending");
}
bool PlayerMotionRuntime::cast_step(FieldObjectId, std::string &e) {
  return fail(e, "Player PKOV timer coroutine not admitted");
}
bool PlayerMotionRuntime::timer_timeout(FieldObjectId timer, std::string &e) {
  FieldObjectId actual;
  if (!live(e))
    return false;
  for (auto role : {R::BlinkTimer, R::CrouchTimer, R::TakeoffTimer, R::AimTimer,
                    R::PkTimer}) {
    if (!node(role, actual, e))
      return false;
    if (timer != actual)
      continue;
    if (role == R::BlinkTimer)
      return set_boolean(F::Idle, true, e);
    if (role == R::CrouchTimer) {
      bool crouch;
      if (!boolean(F::Crouch, crouch, e))
        return false;
      return !crouch ||
             (set_boolean(F::CrouchDone, true, e) && host_.party_call &&
              host_.party_call(data_->text(T::PlayFlashMethod),
                               {data_->text(T::TeleportFlash)}, e));
    }
    if (role == R::TakeoffTimer)
      return fail(
          e, "Player takeoff SceneTreeTimer/camera coroutine owner pending");
    if (role == R::AimTimer) {
      int64_t state;
      Vec2 direction;
      FieldObjectId camera;
      if (!integer(F::State, state, e))
        return false;
      if (state != data_->state(S::AttackPrep))
        return true;
      return vector(F::Direction, direction, e) && node(R::Camera, camera, e) &&
             host_.camera_move &&
             host_.camera_move(
                 camera,
                 {float(direction.x * data_->number(N::CameraOffsetX)),
                  float(direction.y * data_->number(N::CameraOffsetY))},
                 data_->number(N::CameraOffsetTime), e);
    }
    return fail(e, "Player cycling PK native effect owner pending");
  }
  return fail(e, "Player unknown actual Timer callback rejected");
}
bool PlayerMotionRuntime::native_callback(std::string_view method,
                                          std::string &e) {
  if (!live(e))
    return false;
  if (method == data_->text(T::AttackFinishedMethod)) {
    FieldObjectId shape;
    return node(R::BatCollision, shape, e) && host_.shape_disabled &&
           host_.shape_disabled(shape, true, e) &&
           set_integer(F::State, data_->state(S::Move), e) &&
           set_boolean(F::TapRun, false, e);
  }
  if (method == data_->text(T::ColliderMethod))
    return set_event_collider(0, e);
  if (method == data_->text(T::ShootMethod))
    return projectile_shoot(e);
  if (method == data_->text(T::CastMethod))
    return projectile_cast(e);
  for (auto pair : {std::pair<T, R>{T::BlinkFinishedMethod, R::BlinkTimer},
                    {T::CrouchFinishedMethod, R::CrouchTimer},
                    {T::TakeoffFinishedMethod, R::TakeoffTimer},
                    {T::AimFinishedMethod, R::AimTimer},
                    {T::PkFinishedMethod, R::PkTimer}})
    if (method == data_->text(pair.first)) {
      FieldObjectId timer;
      return node(pair.second, timer, e) && timer_timeout(timer, e);
    }
  return fail(e, "Unknown Player source method capability rejected");
}
} // namespace encore::upstream
