#include "encore/field_present.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
const FieldPresentState *FieldPresentRuntime::state(uint32_t id) const {
  for (const auto &s : states_)
    if (s.id == id)
      return &s;
  return nullptr;
}
FieldPresentState *FieldPresentRuntime::mutable_state(uint32_t id) {
  for (auto &s : states_)
    if (s.id == id)
      return &s;
  return nullptr;
}
bool FieldPresentRuntime::initialize(const FieldPresentData &d,
                                     FieldPresentHost h, std::string &e) {
  if (!d.valid() || !h.admit_ready || !h.admit_interaction || !h.read_flag ||
      !h.write_flag || !h.inventory_space || !h.select_item ||
      !h.prompt_enabled || !h.sound || !h.dialogue) {
    e = "Present requires complete typed source Host";
    return false;
  }
  FieldPresentRuntime next;
  next.data_ = &d;
  next.host_ = std::move(h);
  for (const auto &b : d.bindings()) {
    FieldPresentState s;
    s.id = b.id;
    s.frame = b.initial_frame;
    s.visible = b.serialized_visible();
    next.states_.push_back(std::move(s));
  }
  *this = std::move(next);
  e.clear();
  return true;
}
bool FieldPresentRuntime::ready_sparkles(uint32_t id, SourceRandom &random,
                                         std::string &e) {
  if (!data_) {
    e = "Present unbound";
    return false;
  }
  for (const auto &b : data_->bindings())
    if (b.sparkles_id == id) {
      auto *s = mutable_state(b.id);
      if (!s || s->child_ready || s->parent_ready || !s->alive) {
        e = "Present Sparkles Ready lifecycle";
        return false;
      }
      const auto draw =
          random.rand_range(data_->random_low(), data_->random_high());
      if (!std::isfinite(draw) || draw < 0 || draw >= 65536) {
        e = "Present Sparkles RNG bounds";
        return false;
      }
      s->sparkle_frame = std::min(uint32_t(draw),
                                  uint32_t(data_->sparkle_frames().size() - 1));
      s->sparkle_timeout = float(
          1.0 / double(float(data_->sparkle_speed() * data_->sparkle_scale())));
      s->child_ready = true;
      e.clear();
      return true;
    }
  e = "Unknown Present Sparkles source instance";
  return false;
}
bool FieldPresentRuntime::update_state(FieldPresentState &s,
                                       const FieldPresentBinding &b,
                                       std::string &e) {
  bool opened = false;
  if (!host_.read_flag(b, opened, e))
    return false;
  s.opened = opened;
  s.sparkle_visible = !opened;
  const bool playing = !opened;
  if (s.sparkle_playing != playing) {
    s.sparkle_playing = playing;
    if (playing)
      s.sparkle_timeout = float(
          1.0 / double(float(data_->sparkle_speed() * data_->sparkle_scale())));
  }
  return true;
}
bool FieldPresentRuntime::ready_present(uint32_t id, std::string &e) {
  auto *s = mutable_state(id);
  auto *b = data_ ? data_->binding(id) : nullptr;
  if (!s || !b || !s->alive || !s->child_ready || s->parent_ready) {
    e = "Present parent Ready source order";
    return false;
  }
  if (!host_.admit_ready(*b, e))
    return false;
  if (!b->can_pickup() && !host_.prompt_enabled(b->prompt_id, false, e))
    return false;
  if (!update_state(*s, *b, e))
    return false;
  bool collected = false;
  if (!host_.read_flag(*b, collected, e))
    return false;
  if (collected) {
    s->frame = b->opened_frame;
    if (!update_state(*s, *b, e))
      return false;
  }
  s->parent_ready = true;
  e.clear();
  return true;
}
bool FieldPresentRuntime::play(FieldPresentState &s, bool wrapped,
                               std::string &e) {
  if (!data_ || !s.alive) {
    e = "Present animation lifecycle";
    return false;
  }
  s.wrapped = wrapped;
  s.animation_time = 0;
  s.next_key = 0;
  s.animation_playing = true;
  return true;
}
bool FieldPresentRuntime::interact(uint32_t id, std::string &e) {
  auto *s = mutable_state(id);
  auto *b = data_ ? data_->binding(id) : nullptr;
  if (!s || !b || !s->alive || !s->parent_ready) {
    e = "Present interact source instance not ready";
    return false;
  }
  if (!b->can_pickup()) {
    e.clear();
    return true;
  }
  if (!host_.admit_interaction(*b, e))
    return false;
  bool opened = false;
  if (!host_.read_flag(*b, opened, e))
    return false;
  if (opened) {
    if (!b->empty.empty())
      return host_.dialogue(data_->empty_message(), e);
    e.clear();
    return true;
  }
  if (!play(*s, false, e))
    return false;
  bool space = false;
  auto *item = b->item.empty() ? nullptr : data_->item(b->item);
  if (item && !host_.inventory_space(space, e))
    return false;
  if (item && (space || item->keyitem)) {
    if (!host_.select_item(*item, true, e) || !host_.write_flag(*b, true, e) ||
        !update_state(*s, *b, e) || !host_.dialogue(b->dialogue, e))
      return false;
  } else {
    if (s->revert_waiters.size() >= 4096 || s->wait_serial == UINT64_MAX) {
      e = "Present revert coroutine structural limit";
      return false;
    }
    s->revert_waiters.push_back(++s->wait_serial);
    if (!item) {
      if (!b->empty.empty() && !host_.dialogue(data_->empty_message(), e))
        return false;
    } else if (!host_.select_item(*item, false, e) ||
               !host_.dialogue(b->full, e))
      return false;
  }
  e.clear();
  return true;
}
bool FieldPresentRuntime::animate(FieldPresentState &s,
                                  const FieldPresentBinding &b, float delta,
                                  std::string &e) {
  if (!s.animation_playing)
    return true;
  const auto &clip = data_->clip(s.wrapped);
  const float end = std::min(clip.length, float(s.animation_time + delta));
  while (s.next_key < clip.keys.size() && clip.keys[s.next_key].time <= end) {
    const auto &key = clip.keys[s.next_key++];
    if (key.role == 1)
      s.frame = key.value;
    else if (key.role == 2) {
      s.audio_playing = key.value != 0;
      if (!host_.sound(b.id, s.audio_playing, e))
        return false;
    } else {
      e = "Present unknown source animation track";
      return false;
    }
  }
  s.animation_time = end;
  if (end >= clip.length) {
    s.animation_playing = false;
    auto waiters = std::move(s.revert_waiters);
    s.revert_waiters.clear();
    for (auto waiter : waiters) {
      (void)waiter;
      if (!play(s, true, e))
        return false;
    }
  }
  return true;
}
bool FieldPresentRuntime::idle_frame(double delta, bool processing,
                                     std::string &e) {
  if (!data_ || !std::isfinite(delta) || delta < 0 || delta > double(.1f)) {
    e = "Present source idle delta";
    return false;
  }
  if (!processing) {
    e.clear();
    return true;
  }
  for (auto &s : states_) {
    if (!s.alive)
      continue;
    if (s.child_ready && s.sparkle_playing) {
      float remaining = float(delta);
      while (remaining) {
        if (s.sparkle_timeout <= 0) {
          s.sparkle_timeout = float(
              1.0 /
              double(float(data_->sparkle_speed() * data_->sparkle_scale())));
          s.sparkle_frame =
              s.sparkle_frame >= data_->sparkle_frames().size() - 1
                  ? 0
                  : s.sparkle_frame + 1;
        }
        const float step = std::min(remaining, s.sparkle_timeout);
        remaining -= step;
        s.sparkle_timeout -= step;
      }
    }
    if (s.parent_ready && !animate(s, *data_->binding(s.id), float(delta), e))
      return false;
  }
  e.clear();
  return true;
}
bool FieldPresentRuntime::area_left(bool region_changed, std::string &e) {
  (void)region_changed;
  if (!data_) {
    e = "Present source area lifecycle";
    return false;
  }
  for (const auto &s : states_)
    if (s.alive && s.parent_ready) {
      const auto *b = data_->binding(s.id);
      if (b->reset_area() && !host_.write_flag(*b, false, e))
        return false;
    }
  e.clear();
  return true;
}
bool FieldPresentRuntime::exit_tree(uint32_t id, std::string &e) {
  auto *s = mutable_state(id);
  if (!s || !s->alive) {
    e = "Present source tree exit lifecycle";
    return false;
  }
  if (s->audio_playing && !host_.sound(id, false, e))
    return false;
  s->audio_playing = s->animation_playing = s->sparkle_playing = false;
  s->revert_waiters.clear();
  s->alive = false;
  e.clear();
  return true;
}
} // namespace encore::upstream
