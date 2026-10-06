#include "encore/field_stepping_sounds.hpp"
#include <cmath>
namespace encore::upstream {
namespace {
bool convex(const std::vector<Vec2> &v) {
  if (v.size() < 3 || v.size() > 128)
    return false;
  bool pos = false, neg = false;
  for (size_t i = 0; i < v.size(); ++i) {
    auto a = v[i], b = v[(i + 1) % v.size()], c = v[(i + 2) % v.size()];
    if (!std::isfinite(a.x) || !std::isfinite(a.y) || std::abs(a.x) > 1e6f ||
        std::abs(a.y) > 1e6f)
      return false;
    double x = (double(b.x) - a.x) * (double(c.y) - b.y) -
               (double(b.y) - a.y) * (double(c.x) - b.x);
    pos |= x > 0;
    neg |= x < 0;
  }
  return pos != neg;
}
bool intersect(const std::vector<Vec2> &a, const std::vector<Vec2> &b) {
  auto separate = [](const auto &u, const auto &v) {
    for (size_t i = 0; i < u.size(); ++i) {
      double x = double(u[(i + 1) % u.size()].y) - u[i].y,
             y = double(u[i].x) - u[(i + 1) % u.size()].x;
      double lo = 1e30, hi = -1e30, vl = 1e30, vh = -1e30;
      for (auto p : u) {
        double z = p.x * x + p.y * y;
        if (z < lo)
          lo = z;
        if (z > hi)
          hi = z;
      }
      for (auto p : v) {
        double z = p.x * x + p.y * y;
        if (z < vl)
          vl = z;
        if (z > vh)
          vh = z;
      }
      if (hi < vl || vh < lo)
        return true;
    }
    return false;
  };
  return !separate(a, b) && !separate(b, a);
}
} // namespace
const FieldSteppingState *FieldSteppingSoundsRuntime::state(uint32_t id) const {
  for (const auto &s : states_)
    if (s.id == id)
      return &s;
  return nullptr;
}
FieldSteppingState *FieldSteppingSoundsRuntime::active(uint32_t id,
                                                       std::string &e) {
  for (auto &s : states_)
    if (s.id == id) {
      if (s.ready && s.alive)
        return &s;
      e = "Stepping source instance not active";
      return nullptr;
    }
  e = "Stepping source identity absent";
  return nullptr;
}
bool FieldSteppingSoundsRuntime::initialize(const FieldSteppingSoundsData &d,
                                            FieldSteppingSoundsHost h,
                                            std::string &e) {
  if (!d.valid() || !h.admit || !h.admit_ready || !h.describe_body ||
      !h.admit_dispatch || !h.set_player_run_sound || !h.set_party_shadow ||
      !h.geometry) {
    e = "Stepping actual player/party/physics host incomplete";
    return false;
  }
  if (!h.admit(d, e))
    return false;
  data_ = &d;
  host_ = std::move(h);
  states_.clear();
  ready_index_ = 0;
  for (const auto &b : d.bindings())
    states_.push_back({b.id, false, true, b.enabled});
  e.clear();
  return true;
}
bool FieldSteppingSoundsRuntime::ready(uint32_t id, std::string &e) {
  if (!data_ || ready_index_ >= states_.size() ||
      states_[ready_index_].id != id) {
    e = "Stepping source Ready order";
    return false;
  }
  auto &s = states_[ready_index_];
  if (!host_.admit_ready(*data_->binding(id), data_->connections(), e))
    return false;
  s.ready = true;
  ++ready_index_;
  e.clear();
  return true;
}
bool FieldSteppingSoundsRuntime::enable(uint32_t id, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  s->enabled = true;
  e.clear();
  return true;
}
bool FieldSteppingSoundsRuntime::disable(uint32_t id, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  s->enabled = false;
  e.clear();
  return true;
}
bool FieldSteppingSoundsRuntime::dispatch(uint32_t id, uint32_t body,
                                          bool entered, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  if (!body) {
    e = "Stepping source body identity absent";
    return false;
  }
  FieldSteppingBody b;
  if (!host_.describe_body(body, b, e))
    return false;
  const auto &a = *data_->binding(id);
  FieldSteppingDispatch d;
  d.area = id;
  d.body = body;
  if (b.is_player && s->enabled)
    d.sound = entered ? a.entering_sound : a.exiting_sound;
  if (b.is_party)
    d.effect = entered ? a.enter_shadow_effect : a.exit_shadow_effect;
  if (d.sound.empty() && d.effect.empty()) {
    e.clear();
    return true;
  }
  if (!host_.admit_dispatch(d, e))
    return false;
  if (!d.sound.empty() && !host_.set_player_run_sound(d.sound, e))
    return false;
  if (!d.effect.empty() && !host_.set_party_shadow(body, d.effect, e))
    return false;
  e.clear();
  return true;
}
bool FieldSteppingSoundsRuntime::body_enter(uint32_t id, uint32_t body,
                                            std::string &e) {
  return dispatch(id, body, true, e);
}
bool FieldSteppingSoundsRuntime::body_exit(uint32_t id, uint32_t body,
                                           std::string &e) {
  return dispatch(id, body, false, e);
}
bool FieldSteppingSoundsRuntime::overlaps(uint32_t id, uint32_t layer,
                                          uint32_t mask,
                                          const std::vector<Vec2> &body,
                                          bool &out, std::string &e) {
  if (!active(id, e))
    return false;
  if (!convex(body)) {
    e = "Stepping actual body geometry invalid";
    return false;
  }
  const auto &b = *data_->binding(id);
  out = false;
  if (!(b.flags & 1) || !((b.mask & layer) || (mask & b.layer))) {
    e.clear();
    return true;
  }
  for (const auto &s : b.shapes) {
    FieldSteppingGeometry g;
    if (!host_.geometry(id, s, g, e))
      return false;
    if (!g.attached || g.disabled)
      continue;
    if (g.parts.empty() || g.parts.size() > 1024) {
      e = "Stepping dynamic shape owner empty";
      return false;
    }
    for (const auto &p : g.parts) {
      if (!convex(p)) {
        e = "Stepping dynamic shape owner unsupported";
        return false;
      }
      out |= intersect(body, p);
    }
  }
  e.clear();
  return true;
}
bool FieldSteppingSoundsRuntime::exit_tree(uint32_t id, std::string &e) {
  auto *s = active(id, e);
  if (!s)
    return false;
  s->alive = false;
  e.clear();
  return true;
}
} // namespace encore::upstream
