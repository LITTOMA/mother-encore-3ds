#include "encore/field_prompts.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
bool finite(Vec2 v) { return std::isfinite(v.x) && std::isfinite(v.y); }
float ease(float x, float c) {
  x = std::clamp(x, 0.f, 1.f);
  if (c > 0)
    return c < 1 ? 1 - std::pow(1 - x, 1 / c) : std::pow(x, c);
  if (c < 0)
    return x < .5f ? std::pow(x * 2, -c) * .5f
                   : (1 - std::pow(1 - (x - .5f) * 2, -c)) * .5f + .5f;
  return 0;
}
} // namespace
bool FieldPromptRuntime::fail(const char *t) {
  error_ = t;
  return false;
}
const FieldPromptInstance *FieldPromptRuntime::instance(uint32_t id) const {
  auto i = instances_.find(id);
  return i == instances_.end() ? nullptr : &i->second;
}
FieldPromptInstance *FieldPromptRuntime::get(uint32_t id) {
  auto i = instances_.find(id);
  if (i == instances_.end() || !i->second.ready) {
    fail("Field prompt instance not Ready");
    return nullptr;
  }
  return &i->second;
}
bool FieldPromptRuntime::initialize(const FieldPromptData &d, FieldPromptHost h,
                                    std::string &e) {
  if (!d.valid() || !h.observe || !h.key_name || !h.connect || !h.publish ||
      !h.visibility || !h.hide_signal) {
    e = "Field prompt actual source hosts absent";
    return false;
  }
  data_ = &d;
  host_ = std::move(h);
  instances_.clear();
  had_ready_ = false;
  last_ready_ = 0;
  error_.clear();
  e.clear();
  return true;
}
bool FieldPromptRuntime::create(uint32_t id) {
  if (!data_ || !data_->record(id) || instances_.count(id))
    return fail("Field prompt source creation rejected");
  FieldPromptInstance s;
  s.id = id;
  s.enabled = data_->record(id)->enabled;
  s.properties = data_->initial();
  instances_.emplace(id, std::move(s));
  return true;
}
bool FieldPromptRuntime::assign_source_material(
    uint32_t id, const std::array<float, 4> &flash, float fm, float gm,
    std::string &e) {
  auto i = instances_.find(id);
  if (!data_ || !error_.empty() || i == instances_.end() || i->second.ready ||
      i->second.material_assigned || !std::isfinite(fm) || !std::isfinite(gm) ||
      fm < 0 || fm > 1 || gm < 0) {
    e = "Prompt actual source material constructor phase rejected";
    return false;
  }
  for (float v : flash)
    if (!std::isfinite(v) || v < 0 || v > 1) {
      e = "Prompt source Flash color rejected";
      return false;
    }
  i->second.properties[8] = flash;
  i->second.properties[9][0] = fm;
  i->second.properties[7][0] = gm;
  i->second.material_assigned = true;
  e.clear();
  return true;
}
bool FieldPromptRuntime::observe(uint32_t id, FieldPromptObservation &o) {
  const auto *d = data_->record(id);
  if (!d || !host_.observe(d->parent_id, o, error_))
    return false;
  if (o.settings_choice >= 4 || !finite(o.parent_scale) ||
      o.parent_scale.x == 0 || o.parent_scale.y == 0)
    return fail("Field prompt live parent/settings rejected");
  return true;
}
bool FieldPromptRuntime::reset_scale(FieldPromptInstance &s,
                                     const FieldPromptObservation &o) {
  const auto &v = data_->record(s.id)->offset;
  s.position = {v.x / o.parent_scale.x, v.y / o.parent_scale.y};
  s.scale = {1 / o.parent_scale.x, 1 / o.parent_scale.y};
  if (!finite(s.position) || !finite(s.scale))
    return fail("Field prompt inverse parent scale rejected");
  return true;
}
bool FieldPromptRuntime::key_name(FieldPromptInstance &s) {
  std::string label;
  if (!host_.key_name(data_->record(s.id)->key, label, error_))
    return false;
  if (label.size() > 4096 || label.find('\0') != label.npos)
    return fail("Field prompt key label rejected");
  s.label = std::move(label);
  return true;
}
bool FieldPromptRuntime::publish(FieldPromptInstance &s) {
  return host_.publish(s.id, s, error_);
}
bool FieldPromptRuntime::ready(uint32_t id) {
  auto i = instances_.find(id);
  const auto *d = data_ ? data_->record(id) : nullptr;
  if (!d || i == instances_.end() || i->second.ready ||
      (had_ready_ && d->ready_ordinal <= last_ready_))
    return fail("Field prompt source Ready order rejected");
  auto &s = i->second;
  FieldPromptObservation o;
  if (!observe(id, o) || !reset_scale(s, o))
    return false;
  s.properties[6][0] = 0;
  if (!host_.visibility(id, false, error_))
    return false;
  s.hidden = true;
  s.process = false;
  if (!key_name(s))
    return false;
  s.ready = true;
  if (!host_.connect(
          id,
          [this, id](uint32_t object, bool near) {
            return nearby(id, object, near);
          },
          [this, id]() { return pause_changed(id); },
          [this, id]() { return inputs_or_locale_changed(id); }, error_) ||
      !publish(s))
    return false;
  had_ready_ = true;
  last_ready_ = d->ready_ordinal;
  return true;
}
bool FieldPromptRuntime::play(FieldPromptInstance &s,
                              FieldPromptClipRole role) {
  const auto *c = data_->clip(role);
  if (!c)
    return fail("Field prompt animation absent");
  if (s.clip != role || s.elapsed == c->length)
    s.elapsed = 0;
  s.clip = role;
  s.playing = true;
  s.started = true;
  return !host_.native_animation ||
         host_.native_animation(s.id, role, 1, error_);
}
bool FieldPromptRuntime::refresh(FieldPromptInstance &s, bool quick) {
  if (s.pressing)
    return true;
  FieldPromptObservation o;
  if (!observe(s.id, o))
    return false;
  const auto selected = data_->choices()[o.settings_choice];
  const auto category = data_->choices()[data_->record(s.id)->category];
  bool show = (s.enabled && (selected == category || selected == "Both") &&
               s.nearby && !o.paused) ||
              s.force_show;
  show = show && !s.force_hide;
  if (s.hidden && show) {
    s.hidden = false;
    if (!reset_scale(s, o) || !key_name(s))
      return false;
    if (!quick) {
      if (!play(s, FieldPromptClipRole::Show))
        return false;
    } else {
      s.properties[6][0] = 1;
      if (!host_.visibility(s.id, true, error_))
        return false;
      if (!play(s, FieldPromptClipRole::Float))
        return false;
    }
  } else if (!s.hidden && !show) {
    s.hidden = true;
    if (!quick) {
      if (!play(s, FieldPromptClipRole::Hide))
        return false;
    } else {
      s.playing = false;
      if (host_.native_animation &&
          !host_.native_animation(s.id, s.clip, 3, error_))
        return false;
      s.elapsed = 0;
      s.properties[6][0] = 0;
      if (!host_.visibility(s.id, false, error_))
        return false;
    }
  }
  return publish(s);
}
bool FieldPromptRuntime::nearby(uint32_t id, uint32_t object, bool near) {
  auto *s = get(id);
  if (!s)
    return false;
  if (object != data_->record(id)->parent_id)
    return true;
  s->nearby = near;
  return refresh(*s, false);
}
bool FieldPromptRuntime::pause_changed(uint32_t id) {
  auto *s = get(id);
  return s && refresh(*s, true);
}
bool FieldPromptRuntime::inputs_or_locale_changed(uint32_t id) {
  auto *s = get(id);
  return s && key_name(*s) && publish(*s);
}
bool FieldPromptRuntime::set_enabled(uint32_t id, bool enabled, bool quick) {
  auto *s = get(id);
  if (!s)
    return false;
  s->enabled = enabled;
  return refresh(*s, quick);
}
bool FieldPromptRuntime::force(uint32_t id, int mode, bool quick) {
  auto *s = get(id);
  if (!s || mode < -1 || mode > 1)
    return fail("Field prompt force mode rejected");
  s->force_show = mode > 0;
  s->force_hide = mode < 0;
  return refresh(*s, quick);
}
bool FieldPromptRuntime::press(uint32_t id) {
  auto *s = get(id);
  if (!s)
    return false;
  FieldPromptObservation o;
  if (!observe(id, o))
    return false;
  const auto selected = data_->choices()[o.settings_choice],
             category = data_->choices()[data_->record(id)->category];
  if (!s->enabled || (selected != category && selected != "Both"))
    return true;
  s->hidden = true;
  s->pressing = true;
  return play(*s, FieldPromptClipRole::Press) && publish(*s);
}
bool FieldPromptRuntime::apply(FieldPromptInstance &s, FieldPromptProperty p,
                               const std::array<float, 4> &v) {
  const auto role = uint32_t(p);
  if (role == 11) {
    s.properties[6][0] = 0;
    return host_.visibility(s.id, false, error_);
  }
  if (role < 1 || role > 10)
    return fail("Field prompt unknown animation property");
  s.properties[role - 1] = v;
  if (role == 7)
    return host_.visibility(s.id, v[0] != 0, error_);
  return true;
}
bool FieldPromptRuntime::animate(FieldPromptInstance &s,
                                 const FieldPromptClip &c, float from, float to,
                                 bool initial) {
  for (const auto &t : c.tracks) {
    const auto &keys = t.keys;
    if (t.update == 1) {
      if (to != from) {
        const float end = to == c.length && !c.loop ? c.length * 1.001f : to;
        for (const auto &k : keys)
          if (k.time >= from && k.time < end)
            if (!apply(s, t.property, k.value))
              return false;
      }
    } else {
      if (keys.size() == 1) {
        if (!apply(s, t.property, keys.front().value))
          return false;
        continue;
      }
      size_t index = 0;
      float value_time = to;
      if (value_time < keys.front().time) {
        if (!c.loop)
          continue;
        index = keys.size() - 1;
      } else
        while (index + 1 < keys.size() && keys[index + 1].time <= value_time)
          ++index;
      auto value = keys[index].value;
      size_t next = (index + 1) % keys.size();
      if (next > index || c.loop) {
        float left = keys[index].time, right = keys[next].time;
        if (next == 0)
          right += c.length;
        if (value_time < left)
          value_time += c.length;
        const float ratio =
            right > left
                ? ease((value_time - left) / (right - left), keys[index].ease)
                : 0;
        for (size_t k = 0; k < 4; ++k)
          value[k] += (keys[next].value[k] - value[k]) * ratio;
      }
      if (!apply(s, t.property, value))
        return false;
    }
  }
  (void)initial;
  return true;
}
bool FieldPromptRuntime::idle_frame(uint32_t id, float dt) {
  auto *s = get(id);
  if (!s)
    return false;
  if (!std::isfinite(dt) || dt < 0 || dt > 60)
    return fail("Field prompt idle delta rejected");
  if (s->process) {
    FieldPromptObservation o;
    if (!observe(id, o) || !reset_scale(*s, o))
      return false;
  }
  if (!s->playing)
    return true;
  const auto *c = data_->clip(s->clip);
  if (!c)
    return fail("Field prompt playing animation absent");
  const float from = s->elapsed;
  float to = from + dt;
  if (c->loop) {
    while (to >= c->length) {
      if (!animate(*s, *c, s->elapsed, c->length, s->started))
        return false;
      to -= c->length;
      s->elapsed = 0;
      s->started = false;
    }
    if (!animate(*s, *c, s->elapsed, to, s->started))
      return false;
    s->elapsed = to;
    s->started = false;
    return publish(*s);
  }
  to = std::min(to, c->length);
  if (!animate(*s, *c, from, to, s->started))
    return false;
  s->elapsed = to;
  s->started = false;
  if (from < c->length && to == c->length) {
    s->playing = false;
    const auto role = c->role;
    if (host_.native_animation &&
        !host_.native_animation(s->id, role, 2, error_))
      return false;
    if (role == FieldPromptClipRole::Show && s->visible()) {
      if (!play(*s, FieldPromptClipRole::Float))
        return false;
    } else if (role == FieldPromptClipRole::Press) {
      s->pressing = false;
      if (!host_.hide_signal(id, error_))
        return false;
    }
  }
  return publish(*s);
}
bool FieldPromptRuntime::destroy(uint32_t id) {
  auto *s = get(id);
  if (!s)
    return false;
  instances_.erase(id);
  return true;
}
} // namespace encore::upstream

namespace encore::upstream {
// Source plain property assignment differs from set_enabled's refresh method.
bool FieldPromptRuntime::assign_enabled(uint32_t id, bool value) {
  auto *s = get(id);
  if (!s)
    return false;
  s->enabled = value;
  return publish(*s);
}
// Canvas.hide changes only local Canvas visibility; AnimationPlayer, source
// _hidden and _pressing_button are separate state and deliberately retained.
bool FieldPromptRuntime::canvas_hide(uint32_t id) {
  auto *s = get(id);
  if (!s)
    return false;
  s->properties[6][0] = 0;
  return host_.visibility(id, false, error_) && publish(*s);
}
} // namespace encore::upstream
