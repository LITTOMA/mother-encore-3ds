#include "encore/field_melody_background.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
bool finite(Vec2 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::abs(v.x) <= 1e6f &&
         std::abs(v.y) <= 1e6f;
}
BattleValue mix(BattleValue a, BattleValue b, float t) {
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
          a.w + (b.w - a.w) * t};
}
} // namespace
const FieldMelodyState *FieldMelodyBackgroundRuntime::state(uint32_t id) const {
  for (const auto &s : states_)
    if (s.id == id)
      return &s;
  return nullptr;
}
FieldMelodyState *FieldMelodyBackgroundRuntime::active(uint32_t id,
                                                       std::string &e) {
  for (auto &s : states_)
    if (s.id == id) {
      if (s.ready && s.alive)
        return &s;
      e = "Melody source instance inactive";
      return nullptr;
    }
  e = "Melody source identity absent";
  return nullptr;
}
bool FieldMelodyBackgroundRuntime::initialize(
    const FieldMelodyBackgroundData &d, FieldMelodyBackgroundHost h,
    std::string &e) {
  if (!d.valid() || !h.admit_ready || !h.admit_call ||
      !h.camera_screen_center || !h.root_object || !h.set_global_position ||
      !h.local_position || !h.dialogue_actors || !h.talker ||
      !h.print_actor_key || !h.describe_actor || !h.set_actor_position ||
      !h.set_actor_active || !h.remove_child || !h.add_child ||
      !h.set_visible || !h.write_color || !h.animation_started ||
      !h.animation_stopped || !h.create_tween || !h.tween_signal) {
    e = "Melody actual scene/camera/actor/tween host incomplete";
    return false;
  }
  data_ = &d;
  host_ = std::move(h);
  states_.clear();
  tweens_.clear();
  last_token_ = 0;
  ready_index_ = 0;
  for (const auto &b : d.bindings()) {
    FieldMelodyState s;
    s.id = b.id;
    s.modulate = b.initial_modulate;
    s.self_modulate = b.initial_self;
    states_.push_back(std::move(s));
  }
  e.clear();
  return true;
}
bool FieldMelodyBackgroundRuntime::ready(uint32_t id, std::string &e) {
  if (!data_ || ready_index_ >= states_.size() ||
      states_[ready_index_].id != id) {
    e = "Melody source Ready order";
    return false;
  }
  auto &s = states_[ready_index_];
  const auto &b = *data_->binding(id);
  if (!host_.admit_ready(b, data_->policy(), data_->asset(), e) ||
      !host_.root_object(id, s.object, e))
    return false;
  if (!s.object) {
    e = "Melody actual root object missing";
    return false;
  }
  auto color = s.self_modulate;
  color.w = data_->policy().ready_alpha;
  if (!host_.write_color(b.bg_id, FieldMelodyColorRole::SelfModulate, color,
                         e) ||
      !host_.set_visible(id, true, e))
    return false;
  s.self_modulate = color;
  s.visible = true;
  s.ready = true;
  ++ready_index_;
  e.clear();
  return true;
}
bool FieldMelodyBackgroundRuntime::move_actor(FieldMelodyState &s,
                                              FieldMelodyObject object,
                                              bool required, std::string &e) {
  if (!object) {
    e = "Melody source actor reference invalid";
    return false;
  }
  FieldMelodyActor actor;
  if (!host_.describe_actor(object, actor, e))
    return false;
  if (!actor.alive) {
    e = "Melody source cached actor freed";
    return false;
  }
  if (!actor.has_position) {
    if (required) {
      e = "Melody source talker position missing";
      return false;
    }
    e.clear();
    return true;
  }
  if (!actor.parent) {
    e = "Melody source actor parent absent";
    return false;
  }
  s.moved.push_back({object, actor.parent});
  Vec2 local;
  if (!host_.local_position(s.id, local, e) || !finite(local) ||
      !finite(actor.position)) {
    e = "Melody source local transform invalid";
    return false;
  }
  Vec2 target{actor.position.x - local.x, actor.position.y - local.y};
  if (!finite(target)) {
    e = "Melody source actor translated position invalid";
    return false;
  }
  if (!host_.set_actor_position(object, target, e) ||
      !host_.remove_child(actor.parent, object, e) ||
      !host_.add_child(s.object, object, e))
    return false;
  e.clear();
  return true;
}
bool FieldMelodyBackgroundRuntime::add_tween(uint32_t id, uint32_t role,
                                             std::string &e) {
  const auto &b = *data_->binding(id);
  FieldMelodyTweenSpec spec;
  spec.owner = id;
  spec.bg = b.bg_id;
  spec.role = role;
  spec.fade = role == 1 ? data_->policy().fade_in : data_->policy().fade_out;
  uint64_t token = 0;
  if (!host_.create_tween(spec, token, e))
    return false;
  if (!token || token <= last_token_) {
    e = "Melody source global tween token/order invalid";
    return false;
  }
  last_token_ = token;
  FieldMelodyTween tween;
  tween.token = token;
  tween.owner = id;
  tween.role = role;
  tween.fade = spec.fade;
  tweens_.push_back(tween);
  e.clear();
  return true;
}
bool FieldMelodyBackgroundRuntime::appear(uint32_t id, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  const auto &b = *data_->binding(id);
  if (!host_.admit_call(b, true, e))
    return false;
  Vec2 center;
  if (!host_.camera_screen_center(center, e) || !finite(center)) {
    e = "Melody actual camera center invalid";
    return false;
  }
  if (!host_.set_global_position(id, center, e))
    return false;
  std::vector<FieldMelodyActorEntry> actors;
  if (!host_.dialogue_actors(actors, e))
    return false;
  for (const auto &entry : actors) {
    if (!host_.print_actor_key(entry.key, e) ||
        !move_actor(*s, entry.object, false, e))
      return false;
  }
  FieldMelodyObject talker = 0;
  if (!host_.talker(talker, e))
    return false;
  if (talker && !move_actor(*s, talker, true, e))
    return false;
  // Source play of the same assigned animation retains its current position.
  if (!s->assigned)
    s->animation_time = 0;
  s->assigned = true;
  s->playing = true;
  if (!host_.animation_started(b.animation_id, data_->policy().clip, e))
    return false;
  return add_tween(id, 1, e);
}
bool FieldMelodyBackgroundRuntime::disappear(uint32_t id, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  if (!host_.admit_call(*data_->binding(id), false, e))
    return false;
  return add_tween(id, 2, e);
}
bool FieldMelodyBackgroundRuntime::animation_idle(uint32_t id, float dt,
                                                  bool can_process,
                                                  std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  if (!std::isfinite(dt) || dt < 0 || dt > 1e6f) {
    e = "Melody source animation delta invalid";
    return false;
  }
  if (!can_process || !s->attached || !s->playing) {
    e.clear();
    return true;
  }
  const auto &p = data_->policy();
  float pos = std::fmod(s->animation_time + dt, p.length);
  if (!std::isfinite(pos)) {
    e = "Melody source animation clock invalid";
    return false;
  }
  const auto &keys = p.keys;
  size_t index = 0;
  while (index + 1 < keys.size() && keys[index + 1].time <= pos)
    ++index;
  const auto &a = keys[index];
  const auto &b = keys[(index + 1) % keys.size()];
  float end = index + 1 < keys.size() ? b.time : p.length;
  float t = (pos - a.time) / (end - a.time);
  auto color = mix(a.color, b.color, t);
  if (!host_.write_color(data_->binding(id)->bg_id,
                         FieldMelodyColorRole::Modulate, color, e))
    return false;
  s->animation_time = pos;
  s->modulate = color;
  e.clear();
  return true;
}
bool FieldMelodyBackgroundRuntime::restore(FieldMelodyState &s,
                                           std::string &e) {
  const auto &b = *data_->binding(s.id);
  if (!host_.animation_stopped(b.animation_id, e))
    return false;
  s.playing = false;
  s.animation_time = 0;
  for (const auto &entry : s.moved) {
    FieldMelodyActor a;
    if (!host_.describe_actor(entry.object, a, e))
      return false;
    if (!a.alive) {
      e = "Melody source restoration actor freed";
      return false;
    }
    if (a.has_active && !host_.set_actor_active(entry.object, true, e))
      return false;
    // Source position is read after the potentially scripted active setter.
    if (!host_.describe_actor(entry.object, a, e) || !a.alive ||
        !a.has_position) {
      e = "Melody source restoration position missing";
      return false;
    }
    Vec2 local;
    if (!host_.local_position(s.id, local, e) || !finite(local) ||
        !finite(a.position)) {
      e = "Melody source restoration transform invalid";
      return false;
    }
    Vec2 target{a.position.x + local.x, a.position.y + local.y};
    if (!finite(target)) {
      e = "Melody source restored position invalid";
      return false;
    }
    if (!host_.set_actor_position(entry.object, target, e) ||
        !host_.remove_child(s.object, entry.object, e) ||
        !host_.add_child(entry.parent, entry.object, e))
      return false;
  }
  s.moved.clear();
  e.clear();
  return true;
}
bool FieldMelodyBackgroundRuntime::tween_step(uint64_t token, float dt,
                                              uint64_t frame, bool can_process,
                                              std::string &e) {
  auto it = std::find_if(tweens_.begin(), tweens_.end(),
                         [&](const auto &t) { return t.token == token; });
  if (it == tweens_.end()) {
    e = "Melody source tween identity absent";
    return false;
  }
  if (!std::isfinite(dt) || dt < 0 || dt > 1e6f || !frame ||
      frame <= it->last_frame) {
    e = "Melody source tween clock/pass invalid";
    return false;
  }
  auto si = std::find_if(states_.begin(), states_.end(),
                         [&](const auto &s) { return s.id == it->owner; });
  if (si == states_.end()) {
    e = "Melody source bound owner missing";
    return false;
  }
  if (!si->alive) {
    tweens_.erase(it);
    e.clear();
    return true;
  }
  if (!can_process || !si->attached) {
    e.clear();
    return true;
  }
  it->last_frame = frame;
  if (!it->started) {
    it->from = it->fade.explicit_from ? it->fade.from : si->self_modulate;
    it->started = true;
  }
  if (dt == 0) {
    e.clear();
    return true;
  }
  const float elapsed = it->elapsed + dt;
  if (!std::isfinite(elapsed)) {
    e = "Melody source tween elapsed invalid";
    return false;
  }
  const bool finished = elapsed >= it->fade.duration;
  const auto color =
      finished ? it->fade.to
               : mix(it->from, it->fade.to, elapsed / it->fade.duration);
  if (!host_.write_color(data_->binding(si->id)->bg_id,
                         FieldMelodyColorRole::SelfModulate, color, e))
    return false;
  it->elapsed = elapsed;
  si->self_modulate = color;
  if (finished) {
    const auto role = it->role;
    if (!host_.tween_signal(token, FieldMelodyTweenSignal::PropertyFinished,
                            e) ||
        !host_.tween_signal(token, FieldMelodyTweenSignal::StepFinished, e) ||
        !host_.tween_signal(token, FieldMelodyTweenSignal::Finished, e))
      return false;
    if (role == 2 && !restore(*si, e))
      return false;
    tweens_.erase(it);
  }
  e.clear();
  return true;
}
bool FieldMelodyBackgroundRuntime::exit_tree(uint32_t id, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  s->attached = false;
  e.clear();
  return true;
}
bool FieldMelodyBackgroundRuntime::enter_tree(uint32_t id, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  s->attached = true;
  e.clear();
  return true;
}
bool FieldMelodyBackgroundRuntime::free_instance(uint32_t id, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  s->alive = false;
  s->attached = false;
  e.clear();
  return true;
}
} // namespace encore::upstream
