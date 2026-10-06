#include "encore/field_interact_dialog.hpp"
#include <cmath>
namespace encore::upstream {
bool FieldInteractRuntime::fail(std::string_view e) {
  error_ = e;
  poisoned_ = true;
  return false;
}
bool FieldInteractRuntime::callback(bool ok) {
  if (!ok) {
    poisoned_ = true;
    if (error_.empty())
      error_ = "InteractDialog actual Host failed";
  }
  return ok;
}
const FieldInteractState *FieldInteractRuntime::state(uint32_t id) const {
  auto it = states_.find(id);
  return it == states_.end() ? nullptr : &it->second;
}
FieldInteractState *FieldInteractRuntime::live(uint32_t id, bool ready) {
  if (poisoned_ || !data_) {
    fail("InteractDialog runtime unavailable");
    return nullptr;
  }
  auto it = states_.find(id);
  if (it == states_.end() || it->second.deleted ||
      (ready && !it->second.ready)) {
    fail("InteractDialog node/lifecycle rejected");
    return nullptr;
  }
  return &it->second;
}
bool FieldInteractRuntime::initialize(const FieldInteractData &d,
                                      FieldInteractHost h, std::string &e) {
  if (data_ || !d.valid() || !h.admit_ready || !h.connect_flags ||
      !h.read_flag || !h.visible || !h.queue_free ||
      !h.apply_serialized_offset || !h.admit_programme || !h.open_programme ||
      !h.telepathy_effect) {
    e = "InteractDialog actual source Host incomplete";
    return false;
  }
  data_ = &d;
  host_ = std::move(h);
  for (const auto &r : d.records())
    states_.emplace(r.id, FieldInteractState{r.id, false, false,
                                             bool(r.flags & 8), false, false});
  e.clear();
  return true;
}
bool FieldInteractRuntime::instantiate(uint32_t id,bool source_constructor) {
  auto *s = live(id, false);
  if (!s || s->instantiated || s->ready)
    return fail("InteractDialog duplicate/late instancing");
  if(source_constructor){s->instantiated=true;pending_source_constructor_.insert(id);return true;}
  const auto &d = *data_->record(id);
  if ((d.flags & 16) && !callback(host_.apply_serialized_offset(
                            d.prompt, d.button_offset, error_)))
    return false;
  s->instantiated = true;
  return true;
}
bool FieldInteractRuntime::complete_source_constructor(uint32_t id) {
  auto*s=live(id,false);
  if(!s||!s->instantiated||s->ready||!pending_source_constructor_.count(id))
    return fail("InteractDialog source constructor completion rejected");
  const auto&d=*data_->record(id);
  if((d.flags&16)&&!callback(host_.apply_serialized_offset(d.prompt,d.button_offset,error_)))return false;
  pending_source_constructor_.erase(id);
  return true;
}
bool FieldInteractRuntime::flags_updated(uint32_t id) {
  auto *s = live(id, true);
  if (!s)
    return false;
  const auto &d = *data_->record(id);
  bool on = true, v = false;
  if (!d.appear.empty()) {
    if (!callback(host_.read_flag(d.appear, v, error_)))
      return false;
    on = v;
  }
  if (!d.disappear.empty()) {
    if (!callback(host_.read_flag(d.disappear, v, error_)))
      return false;
    on = on && !v;
  }
  s->visible = on;
  if (!callback(host_.visible(id, on, error_)))
    return false;
  if (!on) {
    s->queued = true;
    if (!callback(host_.queue_free(id, error_)))
      return false;
  }
  return true;
}
bool FieldInteractRuntime::ready(uint32_t id) {
  auto *s = live(id, false);
  if (!s || !s->instantiated || s->ready || pending_source_constructor_.count(id))
    return fail("InteractDialog Ready ownership rejected");
  const auto &d = *data_->record(id);
  if (had_ready_ && d.ready <= last_ready_)
    return fail("InteractDialog source Ready order");
  if (!callback(host_.admit_ready(d, error_)))
    return false;
  s->ready = true;
  had_ready_ = true;
  last_ready_ = d.ready;
  if (!flags_updated(id))
    return false;
  return callback(host_.connect_flags(
      id,
      [this, id](std::string &e) {
        const auto ok = flags_updated(id);
        if (!ok)
          e = error_;
        return ok;
      },
      error_));
}
bool FieldInteractRuntime::selected(const FieldInteractDescriptor &d,
                                    std::string &p) {
  p = d.dialogue;
  for (const auto &c : d.choices) {
    if (c.flag.empty())
      continue;
    bool value = false;
    if (!callback(host_.read_flag(c.flag, value, error_)))
      return false;
    if (value)
      p = c.programme;
  }
  return true;
}
bool FieldInteractRuntime::interact(uint32_t id) {
  auto *s = live(id, true);
  if (!s)
    return false;
  std::string p;
  if (!selected(*data_->record(id), p) ||
      !callback(host_.admit_programme(p, error_)))
    return false;
  return callback(host_.open_programme(id, p, error_));
}
bool FieldInteractRuntime::interact_item(uint32_t id, std::string_view name) {
  auto *s = live(id, true);
  if (!s)
    return false;
  return name != data_->record(id)->key_item || interact(id);
}
bool FieldInteractRuntime::telepathy(uint32_t id) {
  auto *s = live(id, true);
  if (!s)
    return false;
  const auto &p = data_->record(id)->thoughts;
  if (!callback(host_.admit_programme(p, error_)))
    return false;
  if (!callback(host_.telepathy_effect(true, error_)))
    return false;
  return callback(host_.open_programme(id, p, error_));
}
bool FieldInteractRuntime::has_thoughts(uint32_t id, bool &value) const {
  if (!data_ || poisoned_ || !data_->record(id))
    return false;
  value = !data_->record(id)->thoughts.empty();
  return true;
}
bool FieldInteractRuntime::commit_deleted(uint32_t id) {
  auto *s = live(id, true);
  if (!s)
    return false;
  if (!s->queued)
    return fail("InteractDialog unqueued deletion");
  s->deleted = true;
  return true;
}
} // namespace encore::upstream