#include "encore/field_player_transitions.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace encore::upstream {
namespace {
bool finite(Vec2 v) { return std::isfinite(v.x) && std::isfinite(v.y); }
Vec2 lerp(Vec2 a, Vec2 b, float t) {
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}
float quart(float t) {
  t = std::clamp(t, 0.f, 1.f);
  const auto k = 1 - t;
  return 1 - k * k * k * k;
}
Vec2 sample(const std::vector<FieldTransitionArrowKey> &keys, double t,
            double length) {
  if (keys.empty())
    return {};
  t = std::fmod(t, length);
  for (size_t i = 0; i < keys.size(); ++i) {
    const auto &k = keys[i];
    auto &next = keys[(i + 1) % keys.size()];
    double end = i + 1 == keys.size() ? next.time + length : next.time;
    if (t >= k.time && (t < end || i + 1 == keys.size()))
      return lerp(k.value, next.value, float((t - k.time) / (end - k.time)));
  }
  const auto &last = keys.back();
  return lerp(last.value, keys.front().value,
              float((t + length - last.time) /
                    (length + keys.front().time - last.time)));
}
} // namespace
bool FieldPlayerTransitionsRuntime::fail(const char *s) {
  error_ = s;
  return false;
}
const FieldTransitionActor *
FieldPlayerTransitionsRuntime::actor(const FieldTransitionContext &c,
                                     uint32_t id) const {
  for (const auto &a : c.party)
    if (a.id == id)
      return &a;
  return nullptr;
}
FieldTransitionInstance *FieldPlayerTransitionsRuntime::instance(uint32_t id) {
  for (auto &i : instances_)
    if (i.id == id)
      return &i;
  return nullptr;
}
bool FieldPlayerTransitionsRuntime::context(FieldTransitionContext &c) {
  if (!data_ || !error_.empty())
    return false;
  if (!host_.context(c, error_)) {
    if (error_.empty())
      error_ = "Transition source context failed";
    return false;
  }
  if (!c.player || c.party.empty() || c.party.size() > 32 ||
      c.party[0].id != c.player || c.party[0].kind != 1 ||
      !c.party[0].camera_id || !finite(c.velocity))
    return fail("Transition actual global player/party identity rejected");
  std::set<uint32_t> ids;
  for (const auto &a : c.party) {
    if (!a.id || !ids.insert(a.id).second || a.kind < 1 || a.kind > 2 ||
        (a.kind == 1 && a.id != c.player) || !finite(a.position) ||
        !finite(a.direction) || !finite(a.shadow_position))
      return fail("Transition actual actor/shape bindings rejected");
    for (const auto &g : a.body_geometry)
      if (g.area || g.stable_id != a.id)
        return fail("Transition KinematicBody collider identity rejected");
    for (const auto &g : a.area_geometry)
      if (!g.area || !g.stable_id)
        return fail("Transition actor child Area collider identity rejected");
  }
  return true;
}
bool FieldPlayerTransitionsRuntime::command(FieldTransitionCommand c) {
  if (!error_.empty())
    return false;
  if (!host_.apply(c, error_)) {
    if (error_.empty())
      error_ = "Transition typed host operation failed";
    return false;
  }
  return true;
}
bool FieldPlayerTransitionsRuntime::initialize(
    const FieldPlayerTransitionsData &d, FieldPlayerTransitionsHost h,
    std::string &e) {
  if (!d.valid() || !h.bind || !h.context || !h.flag || !h.cached_ray ||
      !h.apply) {
    e = "Transition complete typed shape/player/camera host required";
    return false;
  }
  if (!h.bind(d, e))
    return false;
  data_ = &d;
  host_ = std::move(h);
  error_.clear();
  instances_.clear();
  tasks_.clear();
  starts_.clear();
  visuals_.clear();
  for (const auto &r : d.records()) {
    FieldTransitionInstance s;
    s.id = r.id;
    s.arrow_position = r.sprite_position;
    s.arrow_offset = r.sprite_offset;
    s.arrow_rotation = r.sprite_rotation;
    instances_.push_back(s);
  }
  // Source binding precedes Player construction; live queries belong to callbacks.
  e.clear();
  return true;
}
bool FieldPlayerTransitionsRuntime::ray(const FieldTransitionDescriptor &r,
                                        const FieldTransitionContext &c,
                                        bool &result) {
  uint32_t collider = 0;
  if (!host_.cached_ray(r, c.party[0], collider, error_))
    return false;
  auto s = instance(r.id);
  result = collider == c.player && s && s->nearby;
  return true;
}
bool FieldPlayerTransitionsRuntime::update(FieldTransitionInstance &s,
                                           const FieldTransitionContext &c) {
  const auto *r = data_->record(s.id);
  bool skill = false, near = false;
  if (!host_.flag(data_->skill_flag(), skill, error_) || !ray(*r, c, near))
    return false;
  const bool enabled = (r->flags & 4) != 0;
  const bool show =
      near && skill && enabled &&
      std::find(data_->prompt_modes().begin(), data_->prompt_modes().end(),
                c.prompt_mode) != data_->prompt_modes().end();
  s.prompt_from_position = s.arrow_position;
  s.prompt_from_scale = s.arrow_scale;
  s.prompt_target_position = r->sprite_position;
  if (s.inside) {
    s.prompt_target_position.x += p(FieldTransitionParameter::ArrowOffsetX);
    s.prompt_target_position.y += p(FieldTransitionParameter::ArrowOffsetY);
  }
  s.prompt_target_scale = show ? Vec2{1, 1} : Vec2{};
  s.prompt_time = 0;
  if (show) {
    s.arrow_playing = true;
    if(native_animation_&&!native_animation_(s.id,error_))return false;
  }
  return command({FieldTransitionCommandKind::CanInteract, s.id, c.player,
                  uint32_t(!(s.inside && skill && enabled))});
}
bool FieldPlayerTransitionsRuntime::ready(uint32_t id) {
  auto s = instance(id);
  auto r = data_ ? data_->record(id) : nullptr;
  if (!s || !r || s->ready)
    return fail("Transition duplicate/unknown source Ready");
  s->ready = true;
  if (r->kind == 1) {
    // Godot3 Node._ready uses GDScript.call_multilevel_reversed: base first.
    // Base Ready calls the derived update_state while _arrow_pos is still ZERO.
    // Its tween captures the source Sprite scale before leaf Ready sets ZERO.
    s->arrow_scale = r->sprite_scale;
    if (!update_state(id, true))
      return false;
    s->prompt_target_position = {};
    s->arrow_scale = {};
    if (!command({FieldTransitionCommandKind::ArrowPose, id, r->sprite_id, 0,
                  s->arrow_position, s->arrow_scale, r->sprite_rotation}))
      return false;
    return true;
  }
  return true;
}
bool FieldPlayerTransitionsRuntime::update_state(uint32_t id, bool silent) {
  auto s = instance(id);
  auto r = data_ ? data_->record(id) : nullptr;
  if (!s || !s->ready || r->kind != 1)
    return fail("Transition controlled state update scope rejected");
  FieldTransitionContext c;
  if (!context(c) || !update(*s, c))
    return false;
  s->pending_state = true;
  s->silent_state = silent;
  return true;
}
bool FieldPlayerTransitionsRuntime::contacts(const FieldGeometrySpace &space) {
  FieldTransitionContext c;
  if (!context(c))
    return false;
  for (const auto &a : c.party)
    if (a.valid && (a.body_geometry.empty() || a.area_geometry.empty()))
      return fail("Transition contacts require actual native actor geometry snapshot");
  std::map<uint32_t, std::vector<uint32_t>> hits;
  FieldGeometryFilter f;
  f.bodies = false;
  f.areas = true;
  f.monitoring_only = true;
  std::vector<FieldGeometryContact> q;
  for (const auto &a : c.party)
    if (a.valid) {
      for (unsigned type = 0; type < 2; ++type)
        for (const auto &g : type ? a.area_geometry : a.body_geometry) {
          if (g.area && !g.monitorable)
            continue;
          if (!space.overlap_actor(g, f, 4096, q, error_))
            return false;
          for (const auto &hit : q) {
            for (const auto &area : data_->areas())
              if (area.id == hit.stable_id &&
                  ((area.mask & g.layer) || (area.layer & g.mask))) {
                if (type == 0 && a.id == c.player) {
                  bool jump = false;
                  for (const auto &record : data_->records())
                    if (record.kind == 1)
                      for (auto index : record.areas)
                        if (data_->areas()[index].id == area.id)
                          jump = true;
                  if (jump)
                    hits[area.id].push_back(a.id);
                }
                if (type == 1) {
                  bool stair = false;
                  for (const auto &record : data_->records())
                    if (record.kind == 2 &&
                        data_->areas()[record.areas[0]].id == area.id)
                      stair = true;
                  if (stair)
                    hits[area.id].push_back(a.id);
                }
              }
          }
        }
    }
  for (auto &s : instances_)
    if (s.ready) {
      const auto &r = *data_->record(s.id);
      if (r.kind == 1) {
        const bool inside =
            std::find(hits[data_->areas()[r.areas[0]].id].begin(),
                      hits[data_->areas()[r.areas[0]].id].end(),
                      c.player) != hits[data_->areas()[r.areas[0]].id].end();
        const bool near =
            std::find(hits[data_->areas()[r.areas[1]].id].begin(),
                      hits[data_->areas()[r.areas[1]].id].end(),
                      c.player) != hits[data_->areas()[r.areas[1]].id].end();
        if (s.inside != inside) {
          s.inside = inside;
          if (!update(s, c))
            return false;
        }
        if (s.nearby != near) {
          s.nearby = near;
          if (near && c.running) {
            bool seen = false;
            if (!ray(r, c, seen))
              return false;
            if (seen) {
              s.inside_scale = p(FieldTransitionParameter::RunInsideScale);
              s.run_inside_waiters.push_back(
                  p(FieldTransitionParameter::RunInsideSeconds));
              if (!command({FieldTransitionCommandKind::InsideShapeScale,
                            s.id,
                            data_->areas()[r.areas[0]].shape_id,
                            0,
                            {s.inside_scale, s.inside_scale}}))
                return false;
            }
          } else if (!near && !update(s, c))
            return false;
        }
      } else {
        auto list = hits[data_->areas()[r.areas[0]].id];
        std::vector<uint32_t> ordered;
        for (auto id : s.moving_actors)
          if (std::find(list.begin(), list.end(), id) != list.end())
            ordered.push_back(id);
        for (const auto &a : c.party)
          if (std::find(list.begin(), list.end(), a.id) != list.end() &&
              std::find(ordered.begin(), ordered.end(), a.id) ==
                  ordered.end()) {
            ordered.push_back(a.id);
            if (a.kind == 2 &&
                !command(
                    {FieldTransitionCommandKind::ConstraintDirection,
                     s.id,
                     a.id,
                     0,
                     {float((r.flags & 1) != 0), float((r.flags & 1) == 0)}}))
              return false;
          }
        for (auto id : s.moving_actors)
          if (std::find(ordered.begin(), ordered.end(), id) == ordered.end()) {
            const auto *a = actor(c, id);
            if (a && a->kind == 2 &&
                !command({FieldTransitionCommandKind::ConstraintDirection,
                          s.id,
                          id,
                          0,
                          {0, 0}}))
              return false;
          }
        s.moving_actors.swap(ordered);
      }
    }
  return true;
}
bool FieldPlayerTransitionsRuntime::prepare_point(JumpTask &t) {
  const auto &r = *data_->record(t.transition);
  FieldTransitionContext c;
  if (!context(c))
    return false;
  const auto *a = actor(c, t.actor);
  if (!a || !a->valid)
    return fail("Jump coroutine actual actor freed/unknown");
  if (t.point >= r.points.size())
    return finish(t);
  const auto point = r.points[t.point];
  Vec2 dir{point.x - a->position.x, point.y - a->position.y};
  const float length = std::hypot(dir.x, dir.y);
  if (length > 0) {
    dir.x /= length;
    dir.y /= length;
  }
  if (!(r.flags & 1))
    dir.x = 0;
  if (!(r.flags & 2))
    dir.y = 0;
  if (t.leader && !command({FieldTransitionCommandKind::Vibration,
                            r.id,
                            a->id,
                            0,
                            {p(FieldTransitionParameter::VibrationWeak),
                             p(FieldTransitionParameter::VibrationStrong)},
                            {},
                            p(FieldTransitionParameter::VibrationDevice),
                            p(FieldTransitionParameter::VibrationSeconds)}))
    return false;
  if (!command(
          {FieldTransitionCommandKind::ActorDirection, r.id, a->id, 0, dir}) ||
      !command({FieldTransitionCommandKind::ActorIdle, r.id, a->id}))
    return false;
  t.waiting_signal =
      instance(t.transition)->jumping && (t.leader || a->jumps == 1);
  t.started = false;
  t.time = 0;
  t.from = a->position;
  t.target = {point.x - a->shadow_position.x, point.y - a->shadow_position.y};
  return t.waiting_signal || launch(t);
}
bool FieldPlayerTransitionsRuntime::launch(JumpTask &t) {
  const auto &r = *data_->record(t.transition);
  t.waiting_signal = false;
  t.started = true;
  t.time = 0;
  visuals_.push_back({r.id, t.actor, t.leader, false, false, 0});
  if (t.leader &&
      !command({FieldTransitionCommandKind::PlayerJumpState, r.id, t.actor}))
    return false;
  if (!command({FieldTransitionCommandKind::ActorAnimation,
                r.id,
                t.actor,
                0,
                {},
                {},
                0,
                0,
                data_->animations()[1]}))
    return false;
  if (t.leader) {
    if (!command({FieldTransitionCommandKind::Vibration,
                  r.id,
                  t.actor,
                  0,
                  {p(FieldTransitionParameter::VibrationWeak),
                   p(FieldTransitionParameter::VibrationStrong)},
                  {},
                  p(FieldTransitionParameter::VibrationDevice),
                  p(FieldTransitionParameter::VibrationSeconds)}))
      return false;
    Vec2 camera = r.points[t.point];
    if (t.point + 1 < r.points.size())
      camera = {(camera.x + r.points[t.point + 1].x) / 2,
                (camera.y + r.points[t.point + 1].y) / 2};
    camera.x += p(FieldTransitionParameter::CameraOffsetX);
    camera.y += p(FieldTransitionParameter::CameraOffsetY);
    if (!command({FieldTransitionCommandKind::CameraMove,
                  r.id,
                  t.actor,
                  0,
                  camera,
                  {},
                  0,
                  p(FieldTransitionParameter::CameraSeconds)}))
      return false;
  }
  return true;
}
bool FieldPlayerTransitionsRuntime::signal_jump(uint32_t id) {
  for (auto &t : tasks_)
    if (t.transition == id && !t.done && t.waiting_signal && !launch(t))
      return false;
  return true;
}
bool FieldPlayerTransitionsRuntime::start_party_actor(PartyStart &s) {
  if (s.next >= s.actors.size())
    return true;
  FieldTransitionContext c;
  if (!context(c))
    return false;
  auto a = actor(c, s.actors[s.next]);
  if (!a || !a->valid)
    return fail("Jump party source object disappeared");
  if (a->kind == 2 && !a->active)
    return true;
  JumpTask t;
  t.transition = s.transition;
  t.actor = a->id;
  t.leader = a->id == c.player;
  if (a->kind == 2 && !command({FieldTransitionCommandKind::ActorActive,
                                s.transition, a->id, 0}))
    return false;
  tasks_.push_back(t);
  if (!prepare_point(tasks_.back()))
    return false;
  if (s.next == 0 && !signal_jump(s.transition))
    return false;
  ++s.next;
  s.wait = p(FieldTransitionParameter::PartyStagger);
  return true;
}
bool FieldPlayerTransitionsRuntime::source_node(uint32_t id, uint32_t kind) {
  auto *d = data_ ? data_->record(id) : nullptr;
  auto *s = instance(id);
  if (!id || !d || d->kind != kind || !s || !s->ready || !error_.empty())
    return fail("Transition source node/type/lifecycle rejected");
  return true;
}
bool FieldPlayerTransitionsRuntime::accept_source(uint32_t id) {
  return source_node(id, 1) && accept_impl(id);
}
bool FieldPlayerTransitionsRuntime::physics_source(uint32_t id, float dt) {
  return source_node(id, 2) && physics_impl(dt, id);
}
bool FieldPlayerTransitionsRuntime::process_source(uint32_t id, double dt) {
  if (!source_node(id, 1) || !std::isfinite(dt) || dt < 0 || dt > 60)
    return fail("Transition source process identity/delta rejected");
  FieldTransitionContext c;
  if (!context(c))
    return false;
  auto *s = instance(id);
  const auto &r = *data_->record(id);
  bool nearby = false;
  if (!ray(r, c, nearby))
    return false;
  return !nearby || update(*s, c);
}
bool FieldPlayerTransitionsRuntime::bind_native_animation(
    std::function<bool(uint32_t,std::string&)>fn,std::string&e){
  if(native_animation_||!fn){e="Transition native animation owner already bound/missing";return false;}
  native_animation_=std::move(fn);e.clear();return true;
}
bool FieldPlayerTransitionsRuntime::animation_native_source(uint32_t id,double dt){
  auto*s=instance(id);auto*r=data_?data_->record(id):nullptr;
  if(!native_animation_||!s||!r||r->kind!=1||!s->ready||!std::isfinite(dt)||dt<0||dt>60)return fail("Transition native AP owner/delta rejected");
  if(!s->arrow_playing)return true;
  s->animation_time+=dt;s->arrow_offset=sample(r->offset_keys,s->animation_time,r->arrow_length);
  s->arrow_rotation=sample(r->rotation_keys,s->animation_time,r->arrow_length).x*float(3.14159265358979323846/180.0);
  return command({FieldTransitionCommandKind::ArrowAnimation,s->id,r->sprite_id,0,s->arrow_offset,{},s->arrow_rotation});
}
bool FieldPlayerTransitionsRuntime::idle_native_source(uint32_t id, double dt,
                                                       bool processing) {
  return source_node(id, 1) && idle_impl(dt, processing, id, false);
}
bool FieldPlayerTransitionsRuntime::accept() { return accept_impl(0); }
bool FieldPlayerTransitionsRuntime::accept_impl(uint32_t source) {
  FieldTransitionContext c;
  if (!context(c))
    return false;
  for (auto &s : instances_)
    if ((!source || s.id == source) && s.ready &&
        data_->record(s.id)->kind == 1) {
      const auto &r = *data_->record(s.id);
      if (!context(c))
        return false;
      bool nearby = false;
      if (!ray(r, c, nearby))
        return false;
      if (!nearby && !s.jumping)
        continue;
      bool skill = false;
      if (!host_.flag(data_->skill_flag(), skill, error_))
        return false;
      if (s.inside && skill && (r.flags & 4) && c.player_move &&
          !c.player_paused && c.stack_empty) {
        if (!command({FieldTransitionCommandKind::CameraCurrent, s.id,
                      r.camera_id}) ||
            !command(
                {FieldTransitionCommandKind::PlayerPause, s.id, c.player, 0}) ||
            !command(
                {FieldTransitionCommandKind::PlayerLayer, s.id, c.player, 0}))
          return false;
        s.jumping = true;
        s.nearby = false;
        if (!update(s, c))
          return false;
        for (size_t i = 1; i < c.party.size(); ++i) {
          const auto &a = c.party[i];
          if (a.jumps == UINT32_MAX)
            return fail("Source follower jumps counter overflow");
          if (!command({FieldTransitionCommandKind::ActorJumps, s.id, a.id,
                        a.jumps + 1}))
            return false;
          if (a.physics &&
              (!command({FieldTransitionCommandKind::ActorIdle, s.id, a.id}) ||
               !command(
                   {FieldTransitionCommandKind::ActorPhysics, s.id, a.id, 0})))
            return false;
        }
        PartyStart st;
        st.transition = s.id;
        for (const auto &a : c.party)
          st.actors.push_back(a.id);
        starts_.push_back(st);
        if (!start_party_actor(starts_.back()))
          return false;
      } else if (s.jumping && !signal_jump(s.id))
        return false;
    }
  return true;
}
bool FieldPlayerTransitionsRuntime::finish(JumpTask &t) {
  FieldTransitionContext c;
  if (!context(c))
    return false;
  const auto *a = actor(c, t.actor);
  if (!a)
    return fail("Jump completion source actor missing");
  if (t.leader) {
    if (!command({FieldTransitionCommandKind::Vibration,
                  t.transition,
                  t.actor,
                  0,
                  {p(FieldTransitionParameter::VibrationWeak),
                   p(FieldTransitionParameter::VibrationStrong)},
                  {},
                  p(FieldTransitionParameter::VibrationDevice),
                  p(FieldTransitionParameter::VibrationSeconds)}) ||
        !command({FieldTransitionCommandKind::ActorDirection,
                  t.transition,
                  t.actor,
                  0,
                  {std::round(a->direction.x), std::round(a->direction.y)}}) ||
        !command({FieldTransitionCommandKind::PlayerLayer, t.transition,
                  t.actor, 1}) ||
        !command({FieldTransitionCommandKind::PlayerUnpause, t.transition,
                  t.actor, 1}) ||
        !command({FieldTransitionCommandKind::CameraCurrent, t.transition,
                  a->camera_id}) ||
        !command({FieldTransitionCommandKind::CameraReturn,
                  t.transition,
                  t.actor,
                  0,
                  {},
                  {},
                  0,
                  p(FieldTransitionParameter::ReturnSeconds)}))
      return false;
    instance(t.transition)->jumping = false;
    t.done = true;
    if (!signal_jump(t.transition) ||
        !command({FieldTransitionCommandKind::ResetPartyPositions, t.transition,
                  t.actor}))
      return false;
  } else {
    if (!a->jumps)
      return fail("Source follower jump counter underflow");
    if (!command(
            {FieldTransitionCommandKind::ActorIdle, t.transition, t.actor}) ||
        !command({FieldTransitionCommandKind::ActorActive, t.transition,
                  t.actor, 1}) ||
        !command(
            {FieldTransitionCommandKind::ActionDone, t.transition, t.actor}) ||
        !command({FieldTransitionCommandKind::ActorJumps, t.transition, t.actor,
                  a->jumps - 1}))
      return false;
    if (!c.player_jumping && a->jumps == 1 &&
        (!command({FieldTransitionCommandKind::ActorPhysics, t.transition,
                   t.actor, 1}) ||
         !command({FieldTransitionCommandKind::ActorFindPath, t.transition,
                   t.actor})))
      return false;
    t.done = true;
  }
  return true;
}
bool FieldPlayerTransitionsRuntime::actor_action_done(uint32_t id) {
  FieldTransitionContext c;
  if (!context(c) || !actor(c, id))
    return fail("Transition source action_done actor rejected");
  for (auto &s : starts_)
    if (s.next < s.actors.size() && s.actors[s.next] == id && s.wait <= 0 &&
        !start_party_actor(s))
      return false;
  return true;
}
bool FieldPlayerTransitionsRuntime::physics_step(float dt) {
  return physics_impl(dt, 0);
}
bool FieldPlayerTransitionsRuntime::physics_impl(float dt, uint32_t source) {
  if (!std::isfinite(dt) || dt <= 0 || dt > 1)
    return fail("Transition physics delta rejected");
  FieldTransitionContext c;
  if (!context(c))
    return false;
  if (!c.substantial_movement || !c.walking)
    return true;
  for (auto &s : instances_)
    if ((!source || s.id == source) && s.ready &&
        data_->record(s.id)->kind == 2) {
      const auto &r = *data_->record(s.id);
      const float dir = (r.flags & 2) ? -1 : 1;
      const bool horizontal = (r.flags & 1) != 0;
      for (auto id : s.moving_actors) {
        const auto *a = actor(c, id);
        if (!a || !a->valid)
          continue;
        if (c.running) {
          Vec2 speed = horizontal ? Vec2{0, c.velocity.x / r.step_length * dir}
                                  : Vec2{c.velocity.y / r.step_length * dir, 0};
          if (!command({FieldTransitionCommandKind::MoveAndSlide,
                        s.id,
                        id,
                        0,
                        speed,
                        {},
                        dt}))
            return false;
        } else if (id == c.player) {
          const auto input = horizontal ? a->direction.x : a->direction.y;
          s.step_distance += input;
          if (std::abs(s.step_distance) >= r.step_length) {
            s.step_distance -= r.step_length * input;
            Vec2 shift =
                horizontal ? Vec2{0, input * dir} : Vec2{input * dir, 0};
            if (!command({FieldTransitionCommandKind::ActorTranslate, s.id, id,
                          0, shift}) ||
                !command({FieldTransitionCommandKind::PartySpaceMove, s.id, id,
                          0, shift}))
              return false;
          }
        }
      }
    }
  return true;
}
bool FieldPlayerTransitionsRuntime::idle_frame(double dt, bool processing) {
  return idle_impl(dt, processing, 0, true);
}
bool FieldPlayerTransitionsRuntime::idle_impl(double dt, bool processing,
                                              uint32_t source,
                                              bool source_process) {
  if (!std::isfinite(dt) || dt < 0 || dt > 60)
    return fail("Transition idle delta rejected");
  FieldTransitionContext c;
  if (!context(c))
    return false;
  // SceneTreeTimer defaults process_always=true even when inherited Node
  // processing pauses.
  for (auto &s : instances_)
    if ((!source || s.id == source) && s.ready) {
      bool reset = false;
      for (auto &wait : s.run_inside_waiters) {
        wait -= dt;
        if (wait < 0)
          reset = true;
      }
      s.run_inside_waiters.erase(std::remove_if(s.run_inside_waiters.begin(),
                                                s.run_inside_waiters.end(),
                                                [](double v) { return v < 0; }),
                                 s.run_inside_waiters.end());
      if (reset) {
        s.inside_scale = 1;
        const auto &r = *data_->record(s.id);
        if (!command({FieldTransitionCommandKind::InsideShapeScale,
                      s.id,
                      data_->areas()[r.areas[0]].shape_id,
                      0,
                      {1, 1}}))
          return false;
      }
    }
  for (auto &s : starts_)
    if ((!source || s.transition == source) && s.next < s.actors.size()) {
      s.wait -= dt;
      if (s.wait < 0 && !start_party_actor(s))
        return false;
    }
  for (auto &s : instances_)
    if ((!source || s.id == source) && s.pending_state &&
        s.prompt_target_scale.x == 1) {
      s.pending_state = false;
      const auto &r = *data_->record(s.id);
      if (!command({FieldTransitionCommandKind::StateChanged,
                    s.id,
                    r.id,
                    uint32_t(s.silent_state),
                    {float((r.flags & 4) != 0), 0}}))
        return false;
    }
  for (auto &v : visuals_)
    if ((!source || v.transition == source) && !v.done && !v.animated) {
      v.time += dt;
      if (v.time >= p(FieldTransitionParameter::CrouchSeconds)) {
        v.animated = true;
        v.time = 0;
        if (!command({FieldTransitionCommandKind::ActorAnimation,
                      v.transition,
                      v.actor,
                      0,
                      {},
                      {},
                      0,
                      0,
                      data_->animations()[2]}) ||
            !command({FieldTransitionCommandKind::ActorShadow, v.transition,
                      v.actor, 0}))
          return false;
      }
    }
  if (!processing)
    return true;
  for (auto &s : instances_)
    if ((!source || s.id == source) && s.ready &&
        data_->record(s.id)->kind == 1) {
      const auto &r = *data_->record(s.id);
      bool nearby = false;
      if (source_process && !ray(r, c, nearby))
        return false;
      if (source_process && nearby && !update(s, c))
        return false;
      s.prompt_time += dt;
      const auto v = quart(
          float(s.prompt_time / p(FieldTransitionParameter::PromptSeconds)));
      s.arrow_position =
          lerp(s.prompt_from_position, s.prompt_target_position, v);
      s.arrow_scale = lerp(s.prompt_from_scale, s.prompt_target_scale, v);
      if (s.arrow_playing && !native_animation_) {
        s.animation_time += dt;
        s.arrow_offset =
            sample(r.offset_keys, s.animation_time, r.arrow_length);
        s.arrow_rotation =
            sample(r.rotation_keys, s.animation_time, r.arrow_length).x *
            float(3.14159265358979323846 / 180.0);
      }
      if (!command({FieldTransitionCommandKind::ArrowPose, s.id, r.sprite_id, 0,
                    s.arrow_position, s.arrow_scale, s.arrow_rotation}) ||
          (!native_animation_ &&
           !command({FieldTransitionCommandKind::ArrowAnimation, s.id,
                     r.sprite_id, 0, s.arrow_offset})))
        return false;
      if (s.pending_state &&
          (s.prompt_target_scale.x == 1 ||
           s.prompt_time >= p(FieldTransitionParameter::PromptSeconds))) {
        s.pending_state = false;
        if (!command({FieldTransitionCommandKind::StateChanged,
                      s.id,
                      r.id,
                      uint32_t(s.silent_state),
                      {float((r.flags & 4) != 0), 0}}))
          return false;
      }
    }
  for (auto &v : visuals_)
    if ((!source || v.transition == source) && !v.done && v.animated) {
      v.time += dt;
      const auto &r = *data_->record(v.transition);
      const double length = p(FieldTransitionParameter::JumpSeconds),
                   scale_delay = p(FieldTransitionParameter::ScaleDelay),
                   down = p(FieldTransitionParameter::DescendFactor) * length;
      const double ascend_end = v.leader ? length + scale_delay : length;
      {
        double t = v.time;
        float offset =
            t <= ascend_end
                ? -r.height * quart(float(t / length))
                : -r.height *
                      (1 - std::pow(std::clamp(float((t - ascend_end) / down),
                                               0.f, 1.f),
                                    2));
        const float scale_t = quart(float((t - scale_delay) / length));
        Vec2 scale = lerp({p(FieldTransitionParameter::ScaleFromX),
                           p(FieldTransitionParameter::ScaleFromY)},
                          {1, 1}, scale_t);
        if (!command({FieldTransitionCommandKind::ActorVisualOffset,
                      v.transition,
                      v.actor,
                      0,
                      {0, offset}}) ||
            !command({FieldTransitionCommandKind::ActorVisualScale,
                      v.transition, v.actor, 0, scale}))
          return false;
        if (t >= ascend_end + down) {
          v.done = true;
          if (!command({FieldTransitionCommandKind::ActorShadow, v.transition,
                        v.actor, 1}))
            return false;
          if (v.leader && !command({FieldTransitionCommandKind::JumpFinished,
                                    v.transition, v.actor}))
            return false;
        }
      }
    }
  for (size_t i = 0; i < tasks_.size(); ++i) {
    auto &t = tasks_[i];
    if ((source && t.transition != source) || t.done || !t.started ||
        t.waiting_signal)
      continue;
    t.time += dt;
    const auto total = p(FieldTransitionParameter::PathDelay) +
                       p(FieldTransitionParameter::PathSeconds);
    const float factor =
        std::clamp(float((t.time - p(FieldTransitionParameter::PathDelay)) /
                         p(FieldTransitionParameter::PathSeconds)),
                   0.f, 1.f);
    if (!command({FieldTransitionCommandKind::ActorPosition, t.transition,
                  t.actor, 0, lerp(t.from, t.target, factor)}))
      return false;
    if (t.time >= total) {
      ++t.point;
      if (!prepare_point(t))
        return false;
    }
  }

  tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                              [source](const auto &t) {
                                return (!source || t.transition == source) &&
                                       t.done;
                              }),
               tasks_.end());
  starts_.erase(std::remove_if(starts_.begin(), starts_.end(),
                               [source](const auto &s) {
                                 return (!source || s.transition == source) &&
                                        s.next >= s.actors.size();
                               }),
                starts_.end());
  visuals_.erase(std::remove_if(visuals_.begin(), visuals_.end(),
                                [source](const auto &v) {
                                  return (!source || v.transition == source) &&
                                         v.done;
                                }),
                 visuals_.end());
  return true;
}
} // namespace encore::upstream
