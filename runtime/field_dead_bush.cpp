#include "encore/field_dead_bush.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
bool FieldBushRuntime::fail(const char *s) {
  error_ = s;
  return false;
}
bool FieldBushRuntime::callback(bool ok) {
  if (!ok)
    poisoned_ = true;
  return ok;
}
bool FieldBushRuntime::initialize(const FieldBushData &d, FieldBushHost h,
                                  std::string &e) {
  if (!d.valid() || !h.bind || !h.resolve_node || !h.read_flag ||
      !h.connect_viewport || !h.connect_hitbox || !h.publish ||
      !h.duplicate_sprite || !h.roots_frame || !h.roots_add_child ||
      !h.sprite_global_position || !h.roots_local_position || !h.audio_play ||
      !h.prompt_enabled || !h.boolean_setting || !h.vibrate ||
      !h.open_dialogue || !h.call_deferred || !h.queue_free) {
    e = "DeadBush data/source Host incomplete";
    return false;
  }
  if (!h.bind(d, e))
    return false;
  data_ = &d;
  host_ = std::move(h);
  instances_.clear();pending_source_constructor_.clear();
  last_ready_ = 0;
  had_ready_ = false;
  poisoned_ = false;
  error_.clear();
  e.clear();
  return true;
}
const FieldBushInstance *FieldBushRuntime::instance(uint32_t id) const {
  auto s = instances_.find(id);
  return s == instances_.end() ? nullptr : &s->second;
}
FieldBushInstance *FieldBushRuntime::get(uint32_t id, bool require_ready) {
  if (!data_ || poisoned_) {
    fail("DeadBush source runtime unavailable/partial callback failed");
    return nullptr;
  }
  auto s = instances_.find(id);
  if (s == instances_.end() || s->second.deleted ||
      (require_ready && !s->second.ready)) {
    fail("DeadBush instance missing/deleted/not Ready");
    return nullptr;
  }
  return &s->second;
}
bool FieldBushRuntime::publish(FieldBushInstance &s) {
  return callback(host_.publish(s.id, s, error_));
}
bool FieldBushRuntime::create(uint32_t id,bool source_constructor) {
  if (!data_ || poisoned_ || instances_.count(id))
    return fail("DeadBush create source missing/duplicate");
  auto d = data_->record(id);
  if (!d)
    return fail("DeadBush unknown source descriptor");
  FieldBushInstance s;
  s.id = id;
  s.frame = d->frame;
  s.visible = (d->flags & 1) != 0;
  s.sprite_visible = (d->flags & 2) != 0;
  s.body_disabled = (d->flags & 64) != 0;
  s.hit_disabled = (d->flags & 128) != 0;
  s.interact_disabled = (d->flags & 256) != 0;
  instances_.emplace(id, std::move(s));
 if(source_constructor){pending_source_constructor_.insert(id);return true;}
  return callback(host_.connect_hitbox(
      id, [this, id](uint32_t area) { return hitbox_entered(id, area); },
      error_));
}
bool FieldBushRuntime::ready(uint32_t id) {
  auto s = get(id, false);
  if (!s || s->ready)
    return fail("DeadBush Ready missing/duplicate");
  auto d = data_->record(id);
  if (had_ready_ && d->ready_ordinal <= last_ready_)
    return fail("DeadBush Ready violates actual postorder");
  if(pending_source_constructor_.count(id)){if(!callback(host_.connect_hitbox(id,[this,id](uint32_t area){return hitbox_entered(id,area);},error_)))return false;pending_source_constructor_.erase(id);}
  bool exists = false;
  // The onready variables resolve before source _ready. Null New_parent is
  // resolved only at the source fallback at the END of _ready.
  if (!(d->flags & 32)) {
    if (!callback(host_.resolve_node(d->new_parent_id, exists, error_)))
      return false;
    if (!exists)
      return fail("DeadBush actual New_parent absent");
    s->new_parent = d->new_parent_id;
  }
  if (d->called_id) {
    if (!callback(host_.resolve_node(d->called_id, exists, error_)))
      return false;
    if (exists)
      s->called = d->called_id;
  }
  if (!callback(host_.connect_viewport(
          id, [this, id](bool enter) { return viewport(id, enter); }, error_)))
    return false;
  s->visible = false;
  if (!publish(*s))
    return false;
  bool present = false, value = false;
  if (!d->flag.empty() &&
      !callback(host_.read_flag(d->flag, present, value, error_)))
    return false;
  if (!play(*s, d->flag.empty() || (present && value)
                    ? FieldBushClipRole::Idle
                    : FieldBushClipRole::Hidden))
    return false;
  if (d->flags & 32) {
    if (!callback(host_.resolve_node(d->parent_id, exists, error_)))
      return false;
    if (!exists)
      return fail("DeadBush source parent absent");
    s->new_parent = d->parent_id;
  }
  s->ready = true;
  last_ready_ = d->ready_ordinal;
  had_ready_ = true;
  return true;
}
bool FieldBushRuntime::play(FieldBushInstance &s, FieldBushClipRole role) {
  auto a = data_->clip(role);
  if (!a)
    return fail("DeadBush unbound source clip");
  if (s.clip != role || s.elapsed == a->length)
    s.elapsed = 0;
  s.clip = role;
  s.playing = true;
  return !host_.native_animation || callback(host_.native_animation(s.id,uint32_t(role),1,error_));
}
bool FieldBushRuntime::viewport(uint32_t id, bool enter) {
  auto s = get(id);
  if (!s)
    return false;
  s->visible = enter;
  return publish(*s);
}
bool FieldBushRuntime::grow(uint32_t id) {
  auto s = get(id);
  if (!s)
    return false;
  if (s->grow_waiters == UINT32_MAX)
    return fail("DeadBush grow coroutine bound exceeded");
  const auto &v = data_->vibration();
  bool enabled = false;
  if (!callback(
          host_.boolean_setting(data_->vibration_gate(), enabled, error_)))
    return false;
  if (enabled && !callback(host_.vibrate(data_->vibration_device(), v[0], v[1],
                                         v[2], error_)))
    return false;
  if (!play(*s, FieldBushClipRole::Grow))
    return false;
  ++s->grow_waiters;
  return true;
}
bool FieldBushRuntime::interact(uint32_t id) {
  auto s = get(id);
  if (!s)
    return false;
  if (!s->visible)
    return true;
  bool present = false, bat = false;
  if (!callback(host_.read_flag(data_->bat_flag(), present, bat, error_)))
    return false;
  if (!present)
    return fail("DeadBush source bat dictionary key absent");
  return callback(host_.open_dialogue(id, data_->dialogue(bat), error_));
}
bool FieldBushRuntime::hitbox_entered(uint32_t id, uint32_t area) {
  auto s = get(id);
  if (!s)
    return false;
  if (!area)
    return fail("DeadBush hitbox signal actual area absent");
  auto d = data_->record(id);
  uint64_t roots = 0;
  // Source deliberately has no bat/visibility guard here. Actual monitoring
  // geometry/mask owns the signal. Preserve duplicate/frame/add/local order.
  if (!callback(host_.duplicate_sprite(id, d->sprite_id, roots, error_)))
    return false;
  if (!roots)
    return fail("DeadBush Roots actual duplicate identity absent");
  if (!callback(host_.roots_frame(roots, data_->roots_frame(), error_)) ||
      !callback(host_.roots_add_child(roots, s->new_parent, error_)))
    return false;
  Vec2 global{};
  if (!callback(host_.sprite_global_position(d->sprite_id, global, error_)))
    return false;
  if (!std::isfinite(global.x) || !std::isfinite(global.y))
    return fail("DeadBush live Sprite global position invalid");
  if (!callback(host_.roots_local_position(roots, global, error_)))
    return false;
  s->roots.push_back(roots);
  if (!play(*s, FieldBushClipRole::Break) ||
      !callback(host_.audio_play(id, data_->sound(), error_)) ||
      !callback(host_.prompt_enabled(d->prompt_id, false, error_)))
    return false;
  return true;
}
bool FieldBushRuntime::apply(FieldBushInstance &s, FieldBushProperty p,
                             uint32_t v) {
  switch (p) {
  case FieldBushProperty::Frame: {
    auto d = data_->record(s.id);
    if (v >= d->columns * d->rows)
      return fail("DeadBush frame outside source grid");
    if(host_.native_frame&&!callback(host_.native_frame(s.id,v,error_)))return false;
    s.frame = v;
    break;
  }
  case FieldBushProperty::SpriteVisible:
    s.sprite_visible = v != 0;
    break;
  case FieldBushProperty::BodyDisabled:
    s.body_disabled = v != 0;
    break;
  case FieldBushProperty::HitDisabled:
    s.hit_disabled = v != 0;
    break;
  case FieldBushProperty::InteractDisabled:
    s.interact_disabled = v != 0;
    break;
  default:
    return fail("DeadBush unknown typed track");
  }
  return true;
}
bool FieldBushRuntime::finished(FieldBushInstance &s, FieldBushClipRole role) {
  const auto *d = data_->record(s.id);
  // Scene's existing animation_finished handler precedes grow's yield signal.
  if (role == FieldBushClipRole::Break) {
    bool present = false, value = false;
    if (!callback(host_.read_flag(d->disappear_flag, present, value, error_)))
      return false;
    if (present) {
      if (s.called && d->call_kind != FieldBushDeferredKind::None && !value &&
          !callback(host_.call_deferred(*d, d->call_kind, error_)))
        return false;
      if (!callback(host_.queue_free(s.id, error_)))
        return false;
      s.queued = true;
    }
  }
  const auto waiting = s.grow_waiters;
  s.grow_waiters = 0;
  for (uint32_t i = 0; i < waiting; ++i)
    if (!play(s, FieldBushClipRole::Idle))
      return false;
  return true;
}
bool FieldBushRuntime::idle_frame(uint32_t id, float dt) {
  auto s = get(id);
  if (!s)
    return false;
  if (!std::isfinite(dt) || dt < 0 || dt > 60)
    return fail("DeadBush source idle delta rejected");
  if (!s->playing)
    return true;
  auto a = data_->clip(s->clip);
  if (!a)
    return fail("DeadBush playing source clip absent");
  const auto role = s->clip;
  float from = s->elapsed, to = std::min(a->length, float(from + dt)),
        delta = to - from;
  for (const auto &t : a->tracks) {
    if (t.update == 1) {
      if (delta != 0) {
        float end =
            from != a->length && to == a->length ? a->length * 1.001f : to;
        for (const auto &k : t.keys)
          if (k.time >= from && k.time < end && !apply(*s, t.property, k.value))
            return false;
      }
    } else if (!apply(*s, t.property, t.keys.front().value))
      return false;
  }
  s->elapsed = to;
  if (!publish(*s))
    return false;
  if (to == a->length) {
    s->playing = false;
    if (from < a->length && host_.native_animation && !callback(host_.native_animation(id,uint32_t(role),2,error_)))return false;
    if (from < a->length && !finished(*s, role))
      return false;
  }
  return true;
}
bool FieldBushRuntime::seek(uint32_t id, float time, bool update) {
  auto s = get(id);
  if (!s)
    return false;
  auto a = data_->clip(s->clip);
  if (!a || !std::isfinite(time) || time < 0 || time > a->length)
    return fail("DeadBush source seek rejected");
  s->elapsed = time;
  if (update) {
    for (const auto &t : a->tracks) {
      const FieldBushKey *k = nullptr;
      for (const auto &q : t.keys)
        if (q.time <= time)
          k = &q;
      if (k && !apply(*s, t.property, k->value))
        return false;
    }
    return publish(*s);
  }
  return true;
}
bool FieldBushRuntime::commit_deleted(uint32_t id) {
  auto s = get(id);
  if (!s || !s->queued)
    return fail("DeadBush source deferred delete not queued");
  s->deleted = true;
  s->playing = false;
  return true; /* Roots survive under their actual new parent. */
}
} // namespace encore::upstream
