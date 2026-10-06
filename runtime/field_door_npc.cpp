#include "encore/field_door_npc.hpp"
#include <algorithm>
namespace encore::upstream {
const FieldDoorState *FieldDoorNpcRuntime::state(uint32_t id) const {
  for (const auto &s : states_)
    if (s.id == id)
      return &s;
  return nullptr;
}
FieldDoorState *FieldDoorNpcRuntime::active(uint32_t id, std::string &e) {
  for (auto &s : states_)
    if (s.id == id) {
      if (s.ready && s.alive)
        return &s;
      e = "DoorNPC source instance inactive";
      return nullptr;
    }
  e = "DoorNPC source identity missing";
  return nullptr;
}
bool FieldDoorNpcRuntime::initialize(const FieldDoorNpcData &d,
                                     FieldDoorNpcHost h, std::string &e) {
  if (!d.valid() || !h.admit_ready || !h.body_is_current_player ||
      !h.query_ui || !h.read_flag || !h.read_seen || !h.mark_seen ||
      !h.node_path || !h.admit_start || !h.turn_player || !h.pause_player ||
      !h.set_cutscene || !h.black_bars || !h.play_knock || !h.create_timer ||
      !h.admit_dialogue || !h.open_room_and_unpause) {
    e = "DoorNPC actual source host incomplete";
    return false;
  }
  data_ = &d;
  host_ = std::move(h);
  states_.clear();
  waits_.clear();
  ready_index_ = 0;
  last_token_ = 0;
  for (const auto &b : d.bindings()) {
    FieldDoorState s;
    s.id = b.id;
    s.groups = b.groups;
    states_.push_back(std::move(s));
  }
  e.clear();
  return true;
}
bool FieldDoorNpcRuntime::ready(uint32_t id, std::string &e) {
  if (!data_ || ready_index_ >= states_.size() ||
      states_[ready_index_].id != id) {
    e = "DoorNPC source Ready order";
    return false;
  }
  auto &s = states_[ready_index_];
  const auto &b = *data_->binding(id);
  if (!host_.admit_ready(b, data_->policy(), e))
    return false;
  if (!b.dialog.empty() && (s.groups.empty() || !s.groups[0][0].empty()))
    s.groups.insert(s.groups.begin(), {"", b.dialog});
  s.processing = false;
  s.ready = true;
  ++ready_index_;
  e.clear();
  return true;
}
bool FieldDoorNpcRuntime::flags(const FieldDoorBinding &b, bool &out,
                                std::string &e) {
  out = true;
  if (!b.appear.empty() && !host_.read_flag(b.appear, out, e))
    return false;
  if (out && !b.disappear.empty()) {
    bool value;
    if (!host_.read_flag(b.disappear, value, e))
      return false;
    out = !value;
  }
  e.clear();
  return true;
}
bool FieldDoorNpcRuntime::select_dialogue(uint32_t id, FieldDoorSelection &out,
                                          std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  std::string path;
  if (!host_.node_path(id, path, e))
    return false;
  if (path.find('\0') != path.npos || (!path.empty() && path.front() != '/')) {
    e = "DoorNPC actual NodePath invalid";
    return false;
  }
  out = {};
  for (const auto &g : s->groups) {
    bool enabled = true;
    if (!g[0].empty() && !host_.read_flag(g[0], enabled, e))
      return false;
    if (!enabled)
      continue;
    for (size_t j = 1; j < g.size(); ++j) {
      auto key = path + ":" + g[0] + ":" + std::to_string(j) + ":" + g[j];
      bool seen = false;
      if (!host_.read_seen(key, seen, e))
        return false;
      if (!seen || j == g.size() - 1) {
        out = {g[j], std::move(key)};
        break;
      }
    }
  }
  e.clear();
  return true;
}
bool FieldDoorNpcRuntime::body_enter(uint32_t id, uint32_t body,
                                     std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  if (!s->attached) {
    e = "DoorNPC detached body signal";
    return false;
  }
  bool player;
  if (!host_.body_is_current_player(body, player, e))
    return false;
  if (!player) {
    e.clear();
    return true;
  }
  FieldDoorSelection chosen;
  if (!select_dialogue(id, chosen, e))
    return false;
  if (!chosen.dialog.empty()) {
    bool valid;
    if (!flags(*data_->binding(id), valid, e))
      return false;
    s->processing = true;
  }
  e.clear();
  return true;
}
bool FieldDoorNpcRuntime::start(uint32_t id, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  const auto &b = *data_->binding(id);
  const auto &p = data_->policy();
  if (!host_.admit_start(b, p, e))
    return false;
  if (!host_.turn_player(id, p.turn, e) || !host_.pause_player(p.pause, e) ||
      !host_.set_cutscene(true, e) || !host_.black_bars(true, e) ||
      !host_.play_knock(b, e))
    return false;
  uint64_t token = 0;
  if (!host_.create_timer(id, p, token, e))
    return false;
  if (!token || token <= last_token_) {
    e = "DoorNPC global timer identity/order invalid";
    return false;
  }
  last_token_ = token;
  waits_.push_back({token, id});
  s->processing = false;
  e.clear();
  return true;
}
bool FieldDoorNpcRuntime::idle_process(uint32_t id, bool can_process,
                                       std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  if (!can_process || !s->attached || !s->processing) {
    e.clear();
    return true;
  }
  FieldDoorUi ui;
  if (!host_.query_ui(ui, e))
    return false;
  if (ui.cutscene || ui.battle || ui.pause) {
    e.clear();
    return true;
  }
  bool allowed;
  if (!flags(*data_->binding(id), allowed, e))
    return false;
  if (allowed)
    return start(id, e);
  s->processing = false;
  e.clear();
  return true;
}
bool FieldDoorNpcRuntime::timer_timeout(uint64_t token, std::string &e) {
  if (waits_.empty() || waits_.front().token != token) {
    e = "DoorNPC actual timer source FIFO mismatch";
    return false;
  }
  const auto wait = waits_.front();
  auto si = std::find_if(states_.begin(), states_.end(),
                         [&](const auto &s) { return s.id == wait.owner; });
  if (si == states_.end()) {
    e = "DoorNPC coroutine source owner lost";
    return false;
  }
  if (!si->alive) {
    waits_.erase(waits_.begin());
    e.clear();
    return true;
  }
  FieldDoorSelection chosen;
  if (!select_dialogue(wait.owner, chosen, e))
    return false;
  const auto &b = *data_->binding(wait.owner);
  if (!host_.admit_dialogue(b, chosen, e))
    return false;
  if (!chosen.seen_key.empty() && !host_.mark_seen(chosen.seen_key, e))
    return false;
  if (!host_.open_room_and_unpause(b, chosen, data_->policy().completion, e))
    return false;
  if (!host_.set_cutscene(false, e))
    return false;
  waits_.erase(waits_.begin());
  e.clear();
  return true;
}
bool FieldDoorNpcRuntime::exit_tree(uint32_t id, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  s->attached = false;
  e.clear();
  return true;
}
bool FieldDoorNpcRuntime::enter_tree(uint32_t id, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  s->attached = true;
  e.clear();
  return true;
}
bool FieldDoorNpcRuntime::free_instance(uint32_t id, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  s->alive = false;
  s->attached = false;
  e.clear();
  return true;
}
} // namespace encore::upstream
