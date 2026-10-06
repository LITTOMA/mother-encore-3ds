#include "encore/field_vending_machine.hpp"
#include <algorithm>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool FieldVendingRuntime::initialize(const FieldVendingData &d,
                                     const FieldShopData &s,
                                     const FieldGeometryView &g,
                                     FieldVendingHost h, std::string &e) {
  data_ = nullptr;
  active_ = false;
  if (!d.valid() || !s.valid() || d.source_pin() != s.source_pin() ||
      d.descriptor().shop != s.name() || !h.bind || !h.player ||
      !h.set_current_shop || !h.open_dialogue || !h.unpause)
    return fail(e, "Vending typed source/Shop host unbound");
  if (!d.bind_geometry(g, e) || !h.bind(d, s, e))
    return false;
  data_ = &d;
  host_ = std::move(h);
  return true;
}
bool FieldVendingRuntime::interact(uint32_t id, std::string &e) {
  if (!data_ || active_)
    return fail(e, "Vending source interaction state rejected");
  FieldVendingPlayer p;
  if (!host_.player(p, e))
    return false;
  if (!p.id || p.id != id || p.kind != 1 || !p.controlled)
    return fail(e, "Vending requires actual controlled source Player");
  const auto &d = data_->descriptor();
  if (!host_.set_current_shop(d.shop, e) ||
      !host_.open_dialogue(d.id, d.program, e))
    return false;
  player_ = p.id;
  active_ = true;
  return true;
}
bool FieldVendingRuntime::programme_end(uint32_t id, const std::string &program,
                                        std::string &e) {
  if (!data_ || !active_ || id != data_->descriptor().id ||
      program != data_->descriptor().program)
    return fail(e, "Vending dialogue end binding rejected");
  FieldVendingPlayer p;
  if (!host_.player(p, e))
    return false;
  if (p.id != player_ || p.kind != 1 || !p.controlled)
    return fail(e, "Vending source Player identity changed");
  if (!host_.unpause(p.id, e))
    return false;
  active_ = false;
  player_ = 0;
  return true;
}
bool FieldVendingData::bind_geometry(const FieldGeometryView &g,
                                     std::string &e) const {
  if (!valid_ || !g.valid() || g.identity().upstream_commit != pin_ ||
      g.identity().source_sha256 != scene_ ||
      g.identity().scene_id != scene_id_ ||
      g.source_scene() != descriptor_.scene)
    return fail(e, "Vending source geometry identity rejected");
  bool root = false, area = false, body = false, shape = false;
  for (uint32_t i = 0; i < g.node_count(); ++i) {
    auto n = g.node(i);
    if (n.stable_id == descriptor_.id) {
      root = g.string(n.path) == descriptor_.node &&
             n.script_sha256 == script_ && n.ready == descriptor_.ready &&
             n.world.origin.x == descriptor_.root_position.x &&
             n.world.origin.y == descriptor_.root_position.y;
    }
    if (n.stable_id == descriptor_.area)
      area = g.string(n.path) == descriptor_.area_path &&
             g.string(n.script).empty() &&
             std::all_of(n.script_sha256.begin(), n.script_sha256.end(),
                         [](uint8_t v) { return v == 0; });
    if (n.stable_id == descriptor_.body)
      body = g.string(n.path) == descriptor_.body_path;
    if (n.stable_id == descriptor_.shape)
      shape = g.string(n.path) == descriptor_.shape_path;
  }
  if (!root || !area || !body || !shape)
    return fail(e, "Vending source root/Area/body binding rejected");
  bool area_owner = false, body_owner = false, owned_shape = false;
  for (uint32_t i = 0; i < g.owner_count(); ++i) {
    auto owner = g.owner(i);
    auto node = g.node(owner.node);
    if (node.stable_id == descriptor_.area) {
      area_owner = owner.kind == 4 && owner.layer == descriptor_.layer &&
                   owner.mask == descriptor_.mask;
      for (uint32_t j = 0; j < owner.shape_count; ++j) {
        auto s = g.shape(owner.shape_first + j);
        if (g.node(s.node).stable_id == descriptor_.shape && s.owner == i)
          owned_shape = true;
      }
    }
    if (node.stable_id == descriptor_.body)
      body_owner = owner.kind == 1;
  }
  if (!area_owner || !body_owner || !owned_shape)
    return fail(e, "Vending actual Area filter/body/shape owner rejected");
  return true;
}
} // namespace encore::upstream
