#include "encore/field_map_space.hpp"
#include "encore/field_node_tree.hpp"
#include "encore/house_reentry.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_set>
namespace encore::upstream {
namespace {
constexpr uint32_t none = 0xffffffffu;
Vec2 add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
Vec2 sub(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
bool finite(Vec2 a) {
  return std::isfinite(a.x) && std::isfinite(a.y) && std::abs(a.x) < 1e6f &&
         std::abs(a.y) < 1e6f;
}
bool translated(const FieldSceneActionReference &r) {
  return r.scale.x == 1 && r.scale.y == 1 && r.rotation == 0 &&
         r.world[0] == 1 && r.world[1] == 0 && r.world[2] == 0 &&
         r.world[3] == 1;
}
} // namespace
bool FieldMapSpace::configure(const FieldMapView &m,
                              const FieldSceneActionsData &a, std::string &e) {
  std::array<uint8_t, 32> hash{};
  if (!m.valid() || !a.valid() ||
      m.identity().upstream_commit != a.source_pin() ||
      m.source_scene() != a.scene() || !a.source_hash(m.source_scene(), hash) ||
      hash != m.identity().source_sha256) {
    e = "Field live map source/scene identity differs";
    return false;
  }
  FieldMapSpace candidate;
  candidate.source_ = &m;
  candidate.actions_ = &a;
  for (uint32_t i = 0; i < m.canvas_count(); ++i) {
    const auto c = m.canvas(i);
    CanvasState s;
    s.parent = c.parent;
    s.order = c.order;
    s.visible = (c.flags & 1) != 0;
    s.world = c.position;
    s.local = c.parent == none ? c.position
                               : sub(c.position, m.canvas(c.parent).position);
    candidate.canvases_.push_back(s);
    if (!candidate.canvas_ids_.emplace(c.stable_id, i).second) {
      e = "Field live map duplicate canvas";
      return false;
    }
  }
  for (uint32_t i = 0; i < m.map_count(); ++i) {
    const auto l = m.map(i);
    candidate.layers_.push_back({l.layer, l.mask, {}, false});
    if (!candidate.map_ids_.emplace(l.stable_id, i).second) {
      e = "Field live map duplicate layer";
      return false;
    }
  }
  for (const auto &r : a.references())
    if (r.kind == 1) {
      auto i = candidate.map_ids_.find(r.id);
      auto c = candidate.canvas_ids_.find(r.id);
      if (i == candidate.map_ids_.end() || c == candidate.canvas_ids_.end() ||
          !translated(r) || r.script.size() ||
          m.string(m.map(i->second).node) != r.node ||
          m.map(i->second).layer != r.layer ||
          m.map(i->second).mask != r.mask ||
          m.canvas(c->second).position.x != r.world[4] ||
          m.canvas(c->second).position.y != r.world[5]) {
        e = "Field live map source TileMap reference differs";
        return false;
      }
      if (!r.descendants) {
        const auto &s = candidate.canvases_[c->second];
        if (s.parent == none || m.canvas(s.parent).stable_id != r.parent ||
            s.local.x != r.position.x || s.local.y != r.position.y) {
          e = "Field live map native leaf local parent differs";
          return false;
        }
      }
    }
  *this = std::move(candidate);
  e.clear();
  return true;
}
bool FieldMapSpace::admit_reparent(uint32_t leaf, uint32_t parent,
                                   std::string &e) const {
  if (!source_ || !actions_) {
    e = "Field live map unavailable";
    return false;
  }
  auto i = canvas_ids_.find(leaf), p = canvas_ids_.find(parent);
  const auto *r = actions_->reference(leaf);
  const auto *to = actions_->reference(parent);
  if (i == canvas_ids_.end() || p == canvas_ids_.end() || !r || !to ||
      r->kind != 1 || to->kind != 1 || r->descendants || !r->script.empty() ||
      canvases_[i->second].deleted || canvases_[p->second].deleted ||
      !canvases_[p->second].attached) {
    e = "Field live map unsupported reparent target";
    return false;
  }
  bool bound = false;
  for (const auto &b : actions_->bindings())
    if (b.kind == 1 && b.parent_id == parent)
      for (const auto &o : b.objects)
        if (o.source_id == leaf)
          bound = true;
  if (!bound) {
    e = "Field live map reparent missing source binding";
    return false;
  }
  const auto world =
      add(canvases_[p->second].world, canvases_[i->second].local);
  if (!finite(world)) {
    e = "Field live map reparent world translation rejected";
    return false;
  }
  e.clear();
  return true;
}
bool FieldMapSpace::replace_house_tiles(const HouseReentryData &data,
    const FieldNodeTreeRuntime &old_tree, uint64_t old_root,
    FieldNodeTreeRuntime &house_tree, std::string &e) {
  if (!source_ || !data.valid() || source_->source_scene() != data.source_scene() ||
      source_->identity().upstream_commit != data.identity().upstream_commit ||
      !old_root || old_tree.root() != old_root || old_tree.state(old_root) ||
      old_tree.lifecycle_pending() || &old_tree == &house_tree ||
      !old_tree.object_domain() || old_tree.object_domain() != house_tree.object_domain() ||
      !house_tree.state(house_tree.root()) || house_tree.lifecycle_pending() ||
      data.tilemaps().size() != 3) {
    e = "House tiles require actual old scene retirement and full source proof";
    return false;
  }
  std::array<uint8_t, 32> hash{};
  if (!data.source_hash(data.source_scene(), hash) || hash != source_->identity().source_sha256) {
    e = "House tiles source map identity differs";
    return false;
  }
  for (uint32_t i = 0; i < source_->map_count(); ++i)
    if (old_tree.source_object(source_->map(i).stable_id)) {
      e = "House tiles retain an old source TileMap";
      return false;
    }
  std::vector<uint64_t> objects;
  for (const auto &tilemap : data.tilemaps()) {
    const auto object = house_tree.source_object(tilemap.id);
    const auto *state = house_tree.state(object);
    const auto *node = house_tree.descriptor(object);
    FieldIdentity identity;
    if (!object || !state || !state->bound || state->inside || !node ||
        node->id != tilemap.id || node->path != tilemap.node ||
        node->native_class != "TileMap" ||
        !house_tree.object_identity(object, identity) ||
        identity.upstream_commit != data.identity().upstream_commit ||
        identity.source_sha256 != data.identity().source_sha256) {
      e = "House tiles actual staged native owner differs";
      return false;
    }
    objects.push_back(object);
  }
  // Clear the retired source pointers atomically; no subsequent ray/movement
  // query can fall through to outdoor geometry.
  source_ = nullptr;
  actions_ = nullptr;
  house_ = &data;
  house_tree_ = &house_tree;
  house_tiles_ = std::move(objects);
  canvases_.clear(); layers_.clear(); canvas_ids_.clear(); map_ids_.clear();
  e.clear();
  return true;
}
bool FieldMapSpace::remove_leaf(uint32_t leaf, uint32_t actual_parent,
                                std::string &e) {
  auto i = canvas_ids_.find(leaf);
  const auto *r = actions_ ? actions_->reference(leaf) : nullptr;
  if (i == canvas_ids_.end() || !r || r->kind != 1 || r->descendants ||
      !r->script.empty()) {
    e = "Field live map remove target rejected";
    return false;
  }
  bool source_leaf = false;
  for (const auto &binding : actions_->bindings())
    if (binding.kind == 1)
      for (const auto &object : binding.objects)
        if (object.source_id == leaf)
          source_leaf = true;
  if (!source_leaf) {
    e = "Field live map removal missing actual source object";
    return false;
  }
  auto &s = canvases_[i->second];
  if (s.deleted || !s.attached || s.parent == none ||
      source_->canvas(s.parent).stable_id != actual_parent) {
    e = "Field live map remove actual parent differs";
    return false;
  }
  s.attached = false;
  s.parent = none;
  layers_[map_ids_.at(leaf)].moved = true;
  e.clear();
  return true;
}
bool FieldMapSpace::append_leaf(uint32_t leaf, uint32_t parent, uint32_t order,
                                std::string &e) {
  if (!admit_reparent(leaf, parent, e))
    return false;
  auto i = canvas_ids_.at(leaf), p = canvas_ids_.at(parent);
  auto &s = canvases_[i];
  if (s.attached) {
    e = "Field live map add_child target still has parent";
    return false;
  }
  const auto world = add(canvases_[p].world, s.local);
  auto &l = layers_[map_ids_.at(leaf)];
  const auto delta = sub(world, source_->map(map_ids_.at(leaf)).position);
  if (!finite(delta)) {
    e = "Field live map translated geometry range rejected";
    return false;
  }
  s.parent = p;
  s.attached = true;
  s.world = world;
  s.order = order;
  l.delta = delta;
  l.moved = true;
  e.clear();
  return true;
}
bool FieldMapSpace::set_collision(uint32_t leaf, uint32_t role, uint32_t value,
                                  std::string &e) {
  const auto *r = actions_ ? actions_->reference(leaf) : nullptr;
  auto i = map_ids_.find(leaf);
  if (!r || r->kind != 1 || r->descendants || i == map_ids_.end() ||
      (role != 1 && role != 2)) {
    e = "Field live map deferred collision target rejected";
    return false;
  }
  auto &l = layers_[i->second];
  if (role == 1)
    l.mask = value;
  else
    l.layer = value;
  e.clear();
  return true;
}
bool FieldMapSpace::set_canvas_visibility(uint32_t id, bool visible,
                                          std::string &e) {
  auto i = canvas_ids_.find(id);
  if (i == canvas_ids_.end() || canvases_[i->second].deleted) {
    e = "Field live map visibility identity rejected";
    return false;
  }
  canvases_[i->second].visible = visible;
  e.clear();
  return true;
}
bool FieldMapSpace::commit_deleted(uint32_t id, std::string &e) {
  auto i = canvas_ids_.find(id);
  if (i == canvas_ids_.end()) {
    e = "Field live map free identity rejected";
    return false;
  }
  canvases_[i->second].deleted = true;
  e.clear();
  return true;
}
bool FieldMapSpace::active(uint32_t index, const FieldMapGateQuery &g,
                           bool drawing, bool &enabled, std::string &e) const {
  enabled = true;
  uint32_t c = source_->map(index).canvas;
  size_t visited = 0;
  while (c != none) {
    if (c >= canvases_.size() || ++visited > canvases_.size()) {
      e = "Field live map canvas ancestry rejected";
      return false;
    }
    const auto &s = canvases_[c];
    if (s.deleted || !s.attached || (drawing && !s.visible)) {
      enabled = false;
      return true;
    }
    const auto native = source_->canvas(c);
    if (native.gate != none) {
      if (!g) {
        e = "Field live map source flag consumer missing";
        return false;
      }
      const auto state = g(source_->gate(native.gate).stable_id);
      if (state == FieldMapGateState::Pending) {
        e = "Field live map source flag Ready pending";
        return false;
      }
      if (state == FieldMapGateState::Deleted ||
          (drawing && state == FieldMapGateState::Hidden)) {
        enabled = false;
        return true;
      }
    }
    c = s.parent;
  }
  return true;
}
bool FieldMapSpace::query(FieldMapRect area, uint32_t mask,
                          const FieldMapGateQuery &g, bool drawing,
                          size_t capacity, std::vector<uint32_t> &out,
                          std::string &e) const {
  if (house_) {
    if (drawing || !house_->valid() || !house_tree_ ||
        house_tiles_.size() != house_->tilemaps().size() ||
        !finite(area.minimum) || !finite(area.maximum) ||
        area.minimum.x > area.maximum.x || area.minimum.y > area.maximum.y) {
      e = "House tile query requires source collision proof; drawing belongs to House renderer";
      return false;
    }
    for (size_t i = 0; i < house_tiles_.size(); ++i) {
      const auto *state = house_tree_->state(house_tiles_[i]);
      const auto *node = house_tree_->descriptor(house_tiles_[i]);
      FieldIdentity identity;
      if (!state || !node || !state->alive || !state->bound || !state->inside ||
          state->world_dirty || node->id != house_->tilemaps()[i].id ||
          node->path != house_->tilemaps()[i].node || node->native_class != "TileMap" ||
          !house_tree_->object_identity(house_tiles_[i], identity) ||
          identity.upstream_commit != house_->identity().upstream_commit ||
          identity.source_sha256 != house_->identity().source_sha256) {
        e = "House tile collision queried before actual native Enter or after owner retirement";
        return false;
      }
    }
    out.clear(); // All placed tiles were proved to have zero native parts.
    e.clear();
    return true;
  }
  if (!source_) {
    e = "Field live map query unavailable";
    return false;
  }
  std::vector<uint8_t> enabled(layers_.size());
  std::vector<uint32_t> shifted;
  for (uint32_t i = 0; i < layers_.size(); ++i) {
    bool on = false;
    if (!active(i, g, drawing, on, e))
      return false;
    on = on && (drawing || (layers_[i].layer & mask));
    if (!on)
      continue;
    if (layers_[i].delta.x || layers_[i].delta.y)
      shifted.push_back(i);
    else
      enabled[i] = 1;
  }
  std::vector<uint32_t> result;
  auto collect = [&](FieldMapRect box, const std::vector<uint8_t> &selected,
                     size_t cap, std::vector<uint32_t> &v) {
    return drawing
               ? source_->collect_draws_masked(box, selected, cap, v, e)
               : source_->collision_polygons_masked(box, selected, cap, v, e);
  };
  if (!collect(area, enabled, capacity, result))
    return false;
  std::fill(enabled.begin(), enabled.end(), 0);
  for (auto i : shifted) {
    const auto delta = layers_[i].delta;
    FieldMapRect local{sub(area.minimum, delta), sub(area.maximum, delta)};
    enabled[i] = 1;
    std::vector<uint32_t> items;
    if (!collect(local, enabled, capacity - result.size(), items))
      return false;
    enabled[i] = 0;
    result.insert(result.end(), items.begin(), items.end());
  }
  // The compositor uses live canvas ancestry/order and tile-local draw order.
  // Returning stable source indices preserves geometry identity after moves.
  std::sort(result.begin(), result.end());
  out = std::move(result);
  e.clear();
  return true;
}
bool FieldMapSpace::collect_draws(FieldMapRect a, const FieldMapGateQuery &g,
                                  size_t cap, std::vector<uint32_t> &o,
                                  std::string &e) const {
  return query(a, 0, g, true, cap, o, e);
}
bool FieldMapSpace::collision_polygons(FieldMapRect a, uint32_t mask,
                                       const FieldMapGateQuery &g, size_t cap,
                                       std::vector<uint32_t> &o,
                                       std::string &e) const {
  return query(a, mask, g, false, cap, o, e);
}
FieldMapCanvas FieldMapSpace::canvas(uint32_t i) const {
  if (!source_ || i >= canvases_.size())
    return {};
  auto v = source_->canvas(i);
  const auto &s = canvases_[i];
  v.parent = s.parent;
  v.order = s.order;
  v.position = s.world;
  v.flags = (v.flags & ~1u) | (s.visible ? 1u : 0u);
  return v;
}
FieldMapLayer FieldMapSpace::map(uint32_t i) const {
  if (!source_ || i >= layers_.size())
    return {};
  auto v = source_->map(i);
  v.layer = layers_[i].layer;
  v.mask = layers_[i].mask;
  v.position = add(v.position, layers_[i].delta);
  return v;
}
FieldMapDraw FieldMapSpace::draw(uint32_t i) const {
  if (!source_ || i >= source_->draw_count())
    return {};
  auto v = source_->draw(i);
  v.position = add(v.position, layers_[v.map].delta);
  return v;
}
FieldMapPolygon FieldMapSpace::polygon(uint32_t i) const {
  if (!source_ || i >= source_->polygon_count())
    return {};
  auto v = source_->polygon(i);
  v.bounds.minimum = add(v.bounds.minimum, layers_[v.map].delta);
  v.bounds.maximum = add(v.bounds.maximum, layers_[v.map].delta);
  return v;
}
FieldMapShapeTransform FieldMapSpace::shape_transform(uint32_t i) const {
  if (!source_ || i >= source_->polygon_count())
    return {};
  auto p = source_->polygon(i);
  auto v = source_->shape_transform(p.transform);
  v.origin = add(v.origin, layers_[p.map].delta);
  return v;
}
Vec2 FieldMapSpace::point(uint32_t i, uint32_t j) const {
  if (!source_ || i >= source_->polygon_count())
    return {};
  auto p = source_->polygon(i);
  if (j >= p.point_count)
    return {};
  return add(source_->point(p.point_first + j), layers_[p.map].delta);
}
Vec2 FieldMapSpace::sort_anchor(uint32_t i) const {
  if (!source_ || i >= source_->draw_count())
    return {};
  return add(source_->sort_anchor(i), layers_[source_->draw(i).map].delta);
}
} // namespace encore::upstream
