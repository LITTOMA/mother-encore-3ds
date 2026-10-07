#include "encore/field_geometry_space.hpp"
#include "encore/field_dialogue_visual.hpp"
#include "encore/grass_native.hpp"
#include "encore/field_npc.hpp"
#include "encore/field_global_registry.hpp"
#include "encore/field_scene_actions.hpp"
#include "encore/player_initialization.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace encore::upstream {
namespace {
constexpr uint32_t none = 0xffffffffu;
Vec2 add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
Vec2 sub(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
Vec2 mul(Vec2 a, float s) { return {a.x * s, a.y * s}; }
float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
float length(Vec2 a) { return std::sqrt(dot(a, a)); }
Vec2 normalized(Vec2 a) {
  float n = length(a);
  return n ? mul(a, 1 / n) : Vec2{};
}
Vec2 basis(FieldGeometryTransform t, Vec2 p) {
  return add(mul(t.x, p.x), mul(t.y, p.y));
}
Vec2 xf(FieldGeometryTransform t, Vec2 p) { return add(basis(t, p), t.origin); }
FieldGeometryTransform compose(FieldGeometryTransform a,
                               FieldGeometryTransform b) {
  return {basis(a, b.x), basis(a, b.y), xf(a, b.origin)};
}
Vec2 inverse(FieldGeometryTransform t, Vec2 p) {
  p = sub(p, t.origin);
  const float d = cross(t.x, t.y);
  return {(t.y.y * p.x - t.y.x * p.y) / d, (t.x.x * p.y - t.x.y * p.x) / d};
}
bool finite(Vec2 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) < 1000000 &&
         std::abs(p.y) < 1000000;
}
bool finite(FieldGeometryTransform t) {
  return finite(t.x) && finite(t.y) && finite(t.origin) && cross(t.x, t.y) != 0;
}
bool bounds_ok(FieldGeometryBounds b) {
  return finite(b.minimum) && finite(b.maximum) && b.minimum.x <= b.maximum.x &&
         b.minimum.y <= b.maximum.y;
}
bool intersects(FieldGeometryBounds a, FieldGeometryBounds b) {
  return a.minimum.x <= b.maximum.x && a.maximum.x >= b.minimum.x &&
         a.minimum.y <= b.maximum.y && a.maximum.y >= b.minimum.y;
}
bool actor_ok(const FieldGeometryActor &a) {
  if (!finite(a.transform))
    return false;
  if (a.kind == FieldGeometryKind::Circle ||
      a.kind == FieldGeometryKind::Capsule) {
    const float x = dot(a.transform.x, a.transform.x),
                y = dot(a.transform.y, a.transform.y);
    return std::isfinite(a.radius) && a.radius > 0 && x > 0 && x == y &&
           dot(a.transform.x, a.transform.y) == 0 &&
           (a.kind != FieldGeometryKind::Capsule ||
            (std::isfinite(a.extents.y) && a.extents.y >= 0));
  }
  if (a.kind == FieldGeometryKind::Rectangle)
    return finite(a.extents) && a.extents.x > 0 && a.extents.y > 0 &&
           dot(a.transform.x, a.transform.y) == 0;
  if (a.kind != FieldGeometryKind::Convex || a.points.size() < 3 ||
      a.points.size() > 4096)
    return false;
  int winding = 0;
  for (size_t i = 0; i < a.points.size(); ++i) {
    auto p = a.points[i], q = a.points[(i + 1) % a.points.size()],
         r = a.points[(i + 2) % a.points.size()];
    if (!finite(p))
      return false;
    float turn = cross(sub(q, p), sub(r, q));
    if (!turn)
      return false;
    int sign = turn > 0 ? 1 : -1;
    if (winding && sign != winding)
      return false;
    winding = sign;
  }
  return true;
}
std::vector<Vec2> polygon(const FieldGeometryActor &a) {
  std::vector<Vec2> p = a.points;
  if (a.kind == FieldGeometryKind::Rectangle)
    p = {{-a.extents.x, -a.extents.y},
         {a.extents.x, -a.extents.y},
         {a.extents.x, a.extents.y},
         {-a.extents.x, a.extents.y}};
  for (auto &v : p)
    v = xf(a.transform, v);
  return p;
}
// Godot 3 Capsule height is the straight middle segment. This exact union of
// its rectangle and circular caps preserves analytic curves, not tessellation.
std::vector<FieldGeometryActor> capsule_parts(const FieldGeometryActor &a) {
  std::vector<FieldGeometryActor> parts;
  if (a.extents.y > 0) {
    auto rectangle = a;
    rectangle.kind = FieldGeometryKind::Rectangle;
    rectangle.extents = {a.radius, a.extents.y};
    parts.push_back(std::move(rectangle));
  }
  for (float sign : {-1.0f, 1.0f}) {
    auto cap = a;
    cap.kind = FieldGeometryKind::Circle;
    cap.transform.origin = xf(a.transform, {0, sign * a.extents.y});
    parts.push_back(std::move(cap));
  }
  return parts;
}
FieldGeometryBounds actor_bounds(const FieldGeometryActor &a) {
  if (a.kind == FieldGeometryKind::Capsule) {
    const auto p = capsule_parts(a);
    auto b = actor_bounds(p.front());
    for (size_t i = 1; i < p.size(); ++i) {
      const auto c = actor_bounds(p[i]);
      b.minimum = {std::min(b.minimum.x, c.minimum.x),
                   std::min(b.minimum.y, c.minimum.y)};
      b.maximum = {std::max(b.maximum.x, c.maximum.x),
                   std::max(b.maximum.y, c.maximum.y)};
    }
    return b;
  }
  if (a.kind == FieldGeometryKind::Circle) {
    const float r = a.radius * length(a.transform.x);
    return {sub(a.transform.origin, {r, r}), add(a.transform.origin, {r, r})};
  }
  auto p = polygon(a);
  FieldGeometryBounds b{p[0], p[0]};
  for (auto v : p) {
    b.minimum.x = std::min(b.minimum.x, v.x);
    b.minimum.y = std::min(b.minimum.y, v.y);
    b.maximum.x = std::max(b.maximum.x, v.x);
    b.maximum.y = std::max(b.maximum.y, v.y);
  }
  return b;
}
bool overlap(const FieldGeometryActor &a, const FieldGeometryActor &b) {
  if (a.kind == FieldGeometryKind::Capsule) {
    for (const auto &part : capsule_parts(a))
      if (overlap(part, b))
        return true;
    return false;
  }
  if (b.kind == FieldGeometryKind::Capsule)
    return overlap(b, a);
  const bool ca = a.kind == FieldGeometryKind::Circle,
             cb = b.kind == FieldGeometryKind::Circle;
  auto pa = ca ? std::vector<Vec2>{} : polygon(a),
       pb = cb ? std::vector<Vec2>{} : polygon(b);
  auto projection = [](const FieldGeometryActor &shape,
                       const std::vector<Vec2> &p, Vec2 axis) {
    if (shape.kind == FieldGeometryKind::Circle) {
      const float center = dot(shape.transform.origin, axis),
                  r = shape.radius * length(Vec2{dot(shape.transform.x, axis),
                                                 dot(shape.transform.y, axis)});
      return std::array<float, 2>{center - r, center + r};
    }
    std::array<float, 2> result{dot(p[0], axis), dot(p[0], axis)};
    for (auto v : p) {
      float d = dot(v, axis);
      result[0] = std::min(result[0], d);
      result[1] = std::max(result[1], d);
    }
    return result;
  };
  auto axis_test = [&](Vec2 axis) {
    if (std::abs(axis.x) < 1e-5f && std::abs(axis.y) < 1e-5f)
      axis = {0, 1};
    auto aa = projection(a, pa, axis), bb = projection(b, pb, axis);
    bb[0] -= (aa[1] - aa[0]) * .5f;
    bb[1] += (aa[1] - aa[0]) * .5f;
    float dmin = bb[0] - (aa[0] + aa[1]) * .5f,
          dmax = bb[1] - (aa[0] + aa[1]) * .5f;
    return !(dmin > 0 || dmax < 0);
  };
  if (ca && cb)
    return axis_test(normalized(sub(a.transform.origin, b.transform.origin)));
  auto faces = [&](const std::vector<Vec2> &p) {
    for (size_t i = 0; i < p.size(); ++i) {
      auto edge = normalized(sub(p[(i + 1) % p.size()], p[i]));
      if (!axis_test({-edge.y, edge.x}))
        return false;
    }
    return true;
  };
  if (!ca && !faces(pa))
    return false;
  if (!cb && !faces(pb))
    return false;
  if (ca) {
    for (auto v : pb)
      if (!axis_test(normalized(sub(a.transform.origin, v))))
        return false;
  }
  if (cb) {
    for (auto v : pa)
      if (!axis_test(normalized(sub(b.transform.origin, v))))
        return false;
  }
  return true;
}
bool ray_shape(const FieldGeometryActor &a, Vec2 from, Vec2 to, float &fraction,
               Vec2 &normal) {
  if (a.kind == FieldGeometryKind::Capsule) {
    bool found = false;
    float best = std::numeric_limits<float>::infinity();
    Vec2 chosen{};
    for (const auto &part : capsule_parts(a)) {
      float t;
      Vec2 n;
      if (ray_shape(part, from, to, t, n) && t < best) {
        best = t;
        chosen = n;
        found = true;
      }
    }
    if (found) {
      fraction = best;
      normal = chosen;
    }
    return found;
  }
  const auto begin = inverse(a.transform, from), end = inverse(a.transform, to),
             direction = sub(end, begin);
  float t = std::numeric_limits<float>::infinity();
  Vec2 local_normal{};
  if (a.kind == FieldGeometryKind::Circle) {
    const float aa = dot(direction, direction), bb = 2 * dot(begin, direction),
                cc = dot(begin, begin) - a.radius * a.radius;
    float disc = bb * bb - 4 * aa * cc;
    if (disc < 0 || aa == 0)
      return false;
    t = (-bb - std::sqrt(disc)) / (2 * aa);
    if (t < 0 || t > 1 + 1e-5f)
      return false;
    local_normal = normalized(add(begin, mul(direction, t)));
  } else if (a.kind == FieldGeometryKind::Rectangle) {
    float minimum = 0, maximum = 1;
    int axis = -1;
    float sign = 0;
    for (unsigned i = 0; i < 2; ++i) {
      float b = i ? begin.y : begin.x, e = i ? end.y : end.x,
            extent = i ? a.extents.y : a.extents.x, d = e - b;
      if (d == 0) {
        if (b < -extent || b > extent)
          return false;
        continue;
      }
      float low = (-extent - b) / d, high = (extent - b) / d, s = -1;
      if (low > high) {
        std::swap(low, high);
        s = 1;
      }
      if (low > minimum) {
        minimum = low;
        axis = int(i);
        sign = s;
      }
      maximum = std::min(maximum, high);
      if (minimum > maximum)
        return false;
    }
    t = minimum;
    if (axis == 0)
      local_normal = {sign, 0};
    else if (axis == 1)
      local_normal = {0, sign};
  } else {
    // Native convex intersects each original edge, including a ray starting
    // inside; equal fractions retain the first source edge.
    for (size_t i = 0; i < a.points.size(); ++i) {
      auto p = a.points[i], q = a.points[(i + 1) % a.points.size()],
           edge = sub(q, p);
      float den = cross(direction, edge);
      if (den == 0)
        continue;
      auto delta = sub(p, begin);
      float hit = cross(delta, edge) / den, u = cross(delta, direction) / den;
      if (hit < 0 || hit > 1 || u < 0 || u > 1 || hit >= t)
        continue;
      t = hit;
      auto n = normalized(Vec2{edge.y, -edge.x});
      if (dot(direction, n) > 0)
        n = mul(n, -1);
      local_normal = n;
    }
    if (!std::isfinite(t))
      return false;
  }
  fraction = t;
  const float det = cross(a.transform.x, a.transform.y);
  normal = normalized(Vec2{
      (a.transform.y.y * local_normal.x - a.transform.x.y * local_normal.y) /
          det,
      (-a.transform.y.x * local_normal.x + a.transform.x.x * local_normal.y) /
          det});
  return true;
}
} // namespace
bool FieldGeometrySpace::keys(FieldGeometryBounds b,
                              std::vector<int64_t> &result,
                              std::string &error) const {
  if (!bounds_ok(b) || cell_size_ < 1) {
    error = "Field geometry spatial bounds rejected";
    return false;
  }
  const int32_t x0 = int32_t(std::floor(b.minimum.x / cell_size_)),
                y0 = int32_t(std::floor(b.minimum.y / cell_size_)),
                x1 = int32_t(std::floor(b.maximum.x / cell_size_)),
                y1 = int32_t(std::floor(b.maximum.y / cell_size_));
  if (uint64_t(int64_t(x1) - x0 + 1) * uint64_t(int64_t(y1) - y0 + 1) > 65536) {
    error = "Field geometry spatial query capacity exceeded";
    return false;
  }
  result.clear();
  for (int64_t y = y0; y <= y1; ++y)
    for (int64_t x = x0; x <= x1; ++x)
      result.push_back(int64_t((uint64_t(uint32_t(x)) << 32) | uint32_t(y)));
  return true;
}
bool FieldGeometrySpace::configure(const FieldGeometryView &source, float cell,
                                   std::string &error) {
  if (!dynamic_owners_.empty()) {
    error = "Field geometry reconfigure while actual dynamic owners remain";
    return false;
  }
  if (!source.valid() || !std::isfinite(cell) || cell < 1 || cell > 4096) {
    error = "Field geometry space source/grid rejected";
    return false;
  }
  FieldGeometrySpace c;
  c.next_rid_ = next_rid_;
  c.source_ = &source;
  c.cell_size_ = cell;
  c.node_instances_.resize(source.node_count());
  c.children_.resize(source.node_count());
  c.owner_instances_.resize(source.owner_count());
  c.owner_shapes_.resize(source.owner_count());
  for (uint32_t i = 0; i < source.node_count(); ++i) {
    auto n = source.node(i);
    c.ids_[n.stable_id] = i;
    c.nodes_.push_back(
        {n.local, n.world, false, source.string(n.script).empty(), 0, 0});
    if (n.parent != none)
      c.children_[n.parent].push_back(i);
  }
  for (uint32_t i = 0; i < source.owner_count(); ++i) {
    auto o = source.owner(i);
    c.owners_.push_back(
        {o.layer, o.mask, (o.flags & 2) != 0, (o.flags & 4) != 0});
    if (c.next_rid_ == UINT64_MAX) {
      error = "Field native Physics RID exhausted";
      return false;
    }
    c.static_rids_.emplace(i, ++c.next_rid_);
  }
  for (uint32_t s = 0; s < source.shape_count(); ++s) {
    auto shape = source.shape(s);
    c.owner_shapes_[shape.owner].push_back(s);
    c.disabled_.push_back((shape.flags & 1) != 0);
    for (uint32_t part = 0; part < shape.part_count; ++part) {
      uint32_t i = uint32_t(c.instances_.size());
      auto o = source.owner(shape.owner);
      auto n = source.node(o.node);
      uint32_t native_index = part;
      for (uint32_t previous = o.shape_first; previous < s; ++previous)
        native_index += source.shape(previous).part_count;
      Instance item;
      item.contact = {shape.owner, s, part, n.stable_id, native_index};
      item.geometry = shape.part_first + part;
      item.shape_order = source.node(shape.node).order;
      c.instances_.push_back(std::move(item));
      c.owner_instances_[shape.owner].push_back(i);
      uint32_t node = shape.node;
      while (node != none) {
        c.node_instances_[node].push_back(i);
        node = source.node(node).parent;
      }
      if (!c.rebuild_instance(i, error))
        return false;
      auto &entry = c.instances_[i];
      if (!entry.resolved)
        c.unresolved_.insert(i);
      for (auto key : entry.cells)
        c.grid_[key].push_back(i);
    }
  }
  *this = std::move(c);
  error.clear();
  return true;
}
bool FieldGeometrySpace::rebuild_instance(uint32_t i, std::string &error) {
  auto &entry = instances_[i];
  auto shape = source_->shape(entry.contact.shape);
  auto primitive = source_->geometry(entry.geometry);
  entry.actor = {};
  entry.actor.kind = primitive.kind;
  entry.actor.transform = nodes_[shape.node].world;
  if (entry.ownership_override && entry.contact.owner != none)
    entry.actor.transform =
        compose(nodes_[source_->owner(entry.contact.owner).node].world,
                nodes_[shape.node].local);
  entry.actor.radius = primitive.parameters[0];
  entry.actor.extents = {primitive.parameters[0], primitive.parameters[1]};
  auto npc_extents = npc_rectangle_extents_.find(entry.contact.shape);
  if (npc_extents != npc_rectangle_extents_.end())
    entry.actor.extents = npc_extents->second;
  entry.actor.stable_id = entry.contact.stable_id;
  if (primitive.kind == FieldGeometryKind::Convex)
    for (uint32_t j = 0; j < primitive.point_count; ++j)
      entry.actor.points.push_back(source_->point(primitive.point_first + j));
  if (!actor_ok(entry.actor)) {
    error = "Field geometry live kind/transform unsupported";
    return false;
  }
  entry.bounds = actor_bounds(entry.actor);
  entry.disabled = disabled_[entry.contact.shape];
  entry.deleted = entry.ownership_override && entry.contact.owner == none;
  entry.resolved = true;
  uint32_t node = entry.ownership_override
                      ? (entry.contact.owner == none
                             ? none
                             : source_->owner(entry.contact.owner).node)
                      : shape.node;
  if (entry.ownership_override && nodes_[shape.node].deleted)
    entry.deleted = true;
  while (node != none) {
    entry.deleted = entry.deleted || nodes_[node].deleted;
    entry.resolved = entry.resolved && nodes_[node].bound;
    node = source_->node(node).parent;
  }
  return keys(entry.bounds, entry.cells, error);
}
bool FieldGeometrySpace::refresh(const std::vector<uint32_t> &changed,
                                 std::string &error) {
  std::unordered_set<uint32_t> affected;
  for (auto n : changed)
    for (auto i : node_instances_[n])
      affected.insert(i);
  std::vector<uint32_t> indices(affected.begin(), affected.end());
  std::sort(indices.begin(), indices.end());
  std::vector<Instance> old;
  old.reserve(indices.size());
  for (auto i : indices) {
    old.push_back(instances_[i]);
    if (!rebuild_instance(i, error)) {
      for (size_t j = 0; j < old.size(); ++j)
        instances_[indices[j]] = std::move(old[j]);
      return false;
    }
  }
  for (size_t j = 0; j < indices.size(); ++j) {
    auto i = indices[j];
    for (auto key : old[j].cells) {
      auto found = grid_.find(key);
      if (found != grid_.end()) {
        auto &list = found->second;
        list.erase(std::remove(list.begin(), list.end(), i), list.end());
        if (list.empty())
          grid_.erase(found);
      }
    }
    for (auto key : instances_[i].cells)
      grid_[key].push_back(i);
    if (instances_[i].resolved)
      unresolved_.erase(i);
    else
      unresolved_.insert(i);
  }
  return true;
}
bool FieldGeometrySpace::bind_script(uint32_t id,
                                     const std::array<uint8_t, 32> &hash,
                                     uint32_t family, uint32_t capability,
                                     std::string &error) {
  auto it = ids_.find(id);
  if (!source_ || it == ids_.end() || !family || !capability) {
    error = "Field geometry script adapter identity rejected";
    return false;
  }
  uint32_t i = it->second;
  auto n = source_->node(i);
  if (source_->string(n.script).empty() || n.script_sha256 != hash) {
    error = "Field geometry script source rejected";
    return false;
  }
  auto old = nodes_[i];
  if (old.bound && (old.family != family || old.capability != capability)) {
    error = "Field geometry script adapter rebind rejected";
    return false;
  }
  nodes_[i].bound = true;
  nodes_[i].family = family;
  nodes_[i].capability = capability;
  if (!refresh({i}, error)) {
    nodes_[i] = old;
    return false;
  }
  error.clear();
  return true;
}
bool FieldGeometrySpace::apply_updates(
    const std::vector<FieldGeometryNodeUpdate> &updates, std::string &error) {
  if (!source_) {
    error = "Field geometry space unavailable";
    return false;
  }
  auto old_nodes = nodes_;
  auto old_owners = owners_;
  auto old_disabled = disabled_;
  std::vector<uint32_t> changed;
  std::unordered_set<uint32_t> unique;
  auto fail = [&](const char *text) {
    nodes_ = std::move(old_nodes);
    owners_ = std::move(old_owners);
    disabled_ = std::move(old_disabled);
    error = text;
    return false;
  };
  for (const auto &u : updates) {
    auto it = ids_.find(u.stable_id);
    if (it == ids_.end() || !u.fields || u.fields > 31 ||
        !unique.insert(it->second).second)
      return fail("Field geometry dirty node update rejected");
    uint32_t i = it->second, p = i;
    while (p != none) {
      if (!nodes_[p].bound)
        return fail("Field geometry unbound scripted ancestor update rejected");
      if (nodes_[p].deleted)
        return fail("Field geometry freed source node update rejected");
      p = source_->node(p).parent;
    }
    changed.push_back(i);
    if (u.fields & 1) {
      if (!finite(u.local))
        return fail("Field geometry live local matrix rejected");
      nodes_[i].local = u.local;
    }
    if (u.fields & 2) {
      if (nodes_[i].deleted && !u.deleted)
        return fail("Field geometry deleted source node resurrection rejected");
      nodes_[i].deleted = u.deleted;
    }
    if (u.fields & 4) {
      auto cls = source_->string(source_->node(i).class_name);
      if (cls != "CollisionShape2D" && cls != "CollisionPolygon2D")
        return fail("Field geometry disabled update target rejected");
      bool found = false;
      for (auto instance : node_instances_[i]) {
        auto shape = source_->shape(instances_[instance].contact.shape);
        if (shape.node == i) {
          disabled_[instances_[instance].contact.shape] = u.disabled;
          found = true;
        }
      }
      if (!found && cls == "CollisionPolygon2D")
        return fail("Field geometry disabled target has no native parts");
    }
    if (u.fields & 24) {
      bool found = false;
      for (uint32_t owner = 0; owner < source_->owner_count(); ++owner)
        if (source_->owner(owner).node == i) {
          found = true;
          if (u.fields & 8) {
            owners_[owner].layer = u.layer;
            owners_[owner].mask = u.mask;
          }
          if (u.fields & 16) {
            if (source_->owner(owner).kind != 4)
              return fail("Field geometry monitoring update target rejected");
            owners_[owner].monitoring = u.monitoring;
            owners_[owner].monitorable = u.monitorable;
          }
        }
      if (!found)
        return fail("Field geometry owner state target rejected");
    }
  }
  std::unordered_set<uint32_t> affected_nodes;
  std::vector<uint32_t> stack = changed;
  while (!stack.empty()) {
    auto i = stack.back();
    stack.pop_back();
    if (!affected_nodes.insert(i).second)
      continue;
    for (auto c : children_[i])
      stack.push_back(c);
  }
  std::vector<uint32_t> ordered(affected_nodes.begin(), affected_nodes.end());
  std::sort(ordered.begin(), ordered.end());
  for (auto i : ordered) {
    auto parent = source_->node(i).parent;
    nodes_[i].world = parent == none
                          ? nodes_[i].local
                          : compose(nodes_[parent].world, nodes_[i].local);
    if (!finite(nodes_[i].world))
      return fail("Field geometry live world matrix rejected");
  }
  if (!refresh(changed, error)) {
    nodes_ = std::move(old_nodes);
    owners_ = std::move(old_owners);
    disabled_ = std::move(old_disabled);
    return false;
  }
  error.clear();
  return true;
}
bool FieldGeometrySpace::filter_owner(uint32_t i,
                                      const FieldGeometryFilter &f) const {
  auto o = source_->owner(i);
  auto state = owners_[i];
  if (o.kind == 4 ? !f.areas : !f.bodies)
    return false;
  if (f.exclude_stable_id &&
      source_->node(o.node).stable_id == f.exclude_stable_id)
    return false;
  if (f.monitoring_only && (o.kind != 4 || !state.monitoring))
    return false;
  if (f.bilateral_mask) {
    if (!(state.layer & f.layer_mask) && !(state.mask & f.reciprocal_layer))
      return false;
  } else {
    if (!f.monitoring_only && !(state.layer & f.layer_mask))
      return false;
    if (f.require_reciprocal_mask && !(state.mask & f.reciprocal_layer))
      return false;
  }
  return true;
}
bool FieldGeometrySpace::query_instances(FieldGeometryBounds b,
                                         const FieldGeometryFilter &filter,
                                         size_t capacity,
                                         std::vector<uint32_t> &output,
                                         std::string &error) const {
  if (!source_) {
    error = "Field geometry space unavailable";
    return false;
  }
  // Unknown scripted motion cannot be discarded merely because its initial
  // source AABB lies outside this query. Resolve eligible owners first.
  for (auto i : unresolved_) {
    const auto &e = instances_[i];
    if (e.contact.owner == none)
      continue;
    const auto owner = source_->owner(e.contact.owner);
    const bool type = owner.kind == 4 ? filter.areas : filter.bodies;
    const bool excluded = filter.exclude_stable_id &&
                          e.contact.stable_id == filter.exclude_stable_id;
    if (type && !excluded && !e.deleted) {
      error = "Field geometry query requires actual scripted ancestor adapter "
              "before layer/disabled/monitoring filtering";
      return false;
    }
  }
  std::vector<int64_t> cells;
  if (!keys(b, cells, error))
    return false;
  std::unordered_set<uint32_t> seen;
  std::vector<uint32_t> hits;
  for (auto key : cells) {
    auto it = grid_.find(key);
    if (it == grid_.end())
      continue;
    for (auto i : it->second) {
      if (!seen.insert(i).second)
        continue;
      const auto &e = instances_[i];
      if (e.disabled || e.deleted || !filter_owner(e.contact.owner, filter) ||
          !intersects(b, e.bounds))
        continue;
      if (hits.size() >= capacity) {
        error = "Field geometry query result capacity exceeded";
        return false;
      }
      hits.push_back(i);
    }
  }
  std::sort(hits.begin(), hits.end(), [&](uint32_t a, uint32_t c) {
    auto aa = instances_[a].contact, bb = instances_[c].contact;
    auto ao = source_->node(source_->owner(aa.owner).node).order,
         bo = source_->node(source_->owner(bb.owner).node).order;
    if (ao != bo)
      return ao < bo;
    auto as = instances_[a].shape_order, bs = instances_[c].shape_order;
    return as != bs ? as < bs : aa.part < bb.part;
  });
  output = std::move(hits);
  error.clear();
  return true;
}
bool FieldGeometrySpace::live_geometry(const FieldGeometryContact &contact,
                                       FieldGeometryActor &actor,
                                       FieldGeometryOwner &owner,
                                       FieldGeometryShape &shape,
                                       std::string &error) const {
  if (contact.actual_owner || contact.actual_shape) {
    for (const auto &d : dynamic_) {
      const auto &c = d.contact;
      if (c.actual_owner == contact.actual_owner &&
          c.actual_shape == contact.actual_shape && c.part == contact.part &&
          c.stable_id == contact.stable_id &&
          c.native_shape_index == contact.native_shape_index) {
        if (d.disabled || !dynamic_actor(d, actor, error))
          return false;
        owner = d.owner;
        shape = d.shape;
        return true;
      }
    }
    error = "Field dynamic geometry stale actual ObjectID contact";
    return false;
  }
  if (!source_) {
    error = "Field live geometry source unavailable";
    return false;
  }
  for (const auto &entry : instances_) {
    const auto &c = entry.contact;
    if (c.owner != contact.owner || c.shape != contact.shape ||
        c.part != contact.part || c.stable_id != contact.stable_id ||
        c.native_shape_index != contact.native_shape_index)
      continue;
    if (entry.deleted || entry.disabled || !entry.resolved || c.owner == none ||
        c.owner >= owners_.size()) {
      error = "Field live geometry disabled/deleted/unadmitted";
      return false;
    }
    auto o = source_->owner(c.owner);
    o.layer = owners_[c.owner].layer;
    o.mask = owners_[c.owner].mask;
    o.flags = (o.flags & ~6u) | (owners_[c.owner].monitoring ? 2u : 0u) |
              (owners_[c.owner].monitorable ? 4u : 0u);
    actor = entry.actor;
    owner = o;
    shape = source_->shape(c.shape);
    error.clear();
    return true;
  }
  error = "Field live geometry stale/unknown contact";
  return false;
}
bool FieldGeometrySpace::candidates(FieldGeometryBounds b,
                                    const FieldGeometryFilter &filter,
                                    size_t capacity,
                                    std::vector<FieldGeometryContact> &output,
                                    std::string &error) const {
  std::vector<uint32_t> indices;
  if (!query_instances(b, filter, capacity, indices, error))
    return false;
  std::vector<FieldGeometryContact> result;
  for (auto i : indices)
    result.push_back(instances_[i].contact);
  std::vector<size_t> dyn;
  if (!query_dynamic(b, filter, capacity - result.size(), dyn, error))
    return false;
  for (auto i : dyn)
    result.push_back(dynamic_[i].contact);
  output = std::move(result);
  return true;
}
bool FieldGeometrySpace::overlap_actor(
    const FieldGeometryActor &actor, const FieldGeometryFilter &filter,
    size_t capacity, std::vector<FieldGeometryContact> &output,
    std::string &error) const {
  if (!actor_ok(actor)) {
    error = "Field geometry actor kind/transform rejected";
    return false;
  }
  std::vector<uint32_t> indices;
  if (!query_instances(actor_bounds(actor), filter, instances_.size(), indices,
                       error))
    return false;
  std::vector<FieldGeometryContact> result;
  for (auto i : indices)
    if (overlap(actor, instances_[i].actor)) {
      if (result.size() >= capacity) {
        error = "Field geometry overlap capacity exceeded";
        return false;
      }
      result.push_back(instances_[i].contact);
    }
  std::vector<size_t> dyn;
  if (!query_dynamic(actor_bounds(actor), filter, dynamic_.size(), dyn, error))
    return false;
  for (auto i : dyn) {
    FieldGeometryActor current;
    if (!dynamic_actor(dynamic_[i], current, error))
      return false;
    if (overlap(actor, current)) {
      if (result.size() >= capacity) {
        error = "Field dynamic overlap capacity exceeded";
        return false;
      }
      result.push_back(dynamic_[i].contact);
    }
  }
  output = std::move(result);
  error.clear();
  return true;
}
bool FieldGeometrySpace::monitoring_areas(
    const FieldGeometryActor &actor, size_t capacity,
    std::vector<FieldGeometryContact> &output, std::string &error) const {
  if (!source_ || !actor_ok(actor)) {
    error = "Field geometry monitored actor/source rejected";
    return false;
  }
  if (actor.area && !actor.monitorable) {
    output.clear();
    error.clear();
    return true;
  }
  FieldGeometryFilter filter;
  filter.bodies = false;
  filter.areas = true;
  filter.monitoring_only = true;
  filter.bilateral_mask = true;
  filter.layer_mask = actor.mask;
  filter.reciprocal_layer = actor.layer;
  filter.exclude_stable_id = actor.stable_id;
  return overlap_actor(actor, filter, capacity, output, error);
}
bool FieldGeometrySpace::ray(Vec2 from, Vec2 to,
                             const FieldGeometryFilter &filter, size_t capacity,
                             bool &collided, FieldGeometryRayHit &output,
                             std::string &error) const {
  if (!finite(from) || !finite(to) || dot(sub(to, from), sub(to, from)) == 0) {
    error = "Field geometry ray endpoints rejected";
    return false;
  }
  FieldGeometryBounds b{{std::min(from.x, to.x), std::min(from.y, to.y)},
                        {std::max(from.x, to.x), std::max(from.y, to.y)}};
  std::vector<uint32_t> indices;
  if (!query_instances(b, filter, capacity, indices, error))
    return false;
  bool found = false;
  FieldGeometryRayHit result;
  float best = std::numeric_limits<float>::infinity();
  for (auto i : indices) {
    float t;
    Vec2 normal;
    if (ray_shape(instances_[i].actor, from, to, t, normal) && t < best) {
      best = t;
      found = true;
      static_cast<FieldGeometryContact &>(result) = instances_[i].contact;
      result.position = add(from, mul(sub(to, from), t));
      result.normal = normal;
      result.fraction = t;
    }
  }
  std::vector<size_t> dyn;
  if (!query_dynamic(b, filter, capacity - indices.size(), dyn, error))
    return false;
  for (auto i : dyn) {
    FieldGeometryActor current;
    if (!dynamic_actor(dynamic_[i], current, error))
      return false;
    float t;
    Vec2 normal;
    if (ray_shape(current, from, to, t, normal) && t < best) {
      best = t;
      found = true;
      static_cast<FieldGeometryContact &>(result) = dynamic_[i].contact;
      result.position = add(from, mul(sub(to, from), t));
      result.normal = normal;
      result.fraction = t;
    }
  }
  collided = found;
  if (found)
    output = result;
  error.clear();
  return true;
}
bool FieldGeometrySpace::admit_leaf_polygon(const FieldSceneActionsData &a,
                                            uint32_t id, uint32_t &node,
                                            uint32_t &shape,
                                            std::string &e) const {
  std::array<uint8_t, 32> hash{};
  const auto *r = a.reference(id);
  auto n = ids_.find(id);
  if (!source_ || !a.valid() ||
      a.source_pin() != source_->identity().upstream_commit ||
      a.scene() != source_->source_scene() ||
      !a.source_hash(source_->source_scene(), hash) ||
      hash != source_->identity().source_sha256 || !r || r->kind != 2 ||
      r->descendants || !r->script.empty() || n == ids_.end()) {
    e = "Field live polygon source identity/leaf rejected";
    return false;
  }
  node = n->second;
  if (source_->string(source_->node(node).class_name) != "CollisionPolygon2D" ||
      !source_->string(source_->node(node).script).empty() ||
      nodes_[node].deleted) {
    e = "Field live polygon actual class/lifetime rejected";
    return false;
  }
  bool moving = false;
  for (const auto &b : a.bindings())
    if (b.kind == 1)
      for (const auto &o : b.objects)
        if (o.source_id == id)
          moving = true;
  if (!moving) {
    e = "Field live polygon source reparent binding missing";
    return false;
  }
  shape = none;
  for (uint32_t i = 0; i < source_->shape_count(); ++i)
    if (source_->shape(i).node == node) {
      if (shape != none) {
        e = "Field live polygon duplicate native shape owner";
        return false;
      }
      shape = i;
    }
  if (shape == none || !source_->shape(shape).part_count) {
    e = "Field live polygon native parts missing";
    return false;
  }
  e.clear();
  return true;
}
void FieldGeometrySpace::rebuild_dependencies() {
  node_instances_.assign(source_->node_count(), {});
  owner_instances_.assign(source_->owner_count(), {});
  for (uint32_t i = 0; i < instances_.size(); ++i) {
    const auto &entry = instances_[i];
    const auto shape = source_->shape(entry.contact.shape);
    if (entry.contact.owner != none)
      owner_instances_[entry.contact.owner].push_back(i);
    uint32_t node = shape.node;
    if (entry.ownership_override) {
      node_instances_[node].push_back(i);
      node = entry.contact.owner == none
                 ? none
                 : source_->owner(entry.contact.owner).node;
    }
    while (node != none) {
      node_instances_[node].push_back(i);
      node = source_->node(node).parent;
    }
  }
}
bool FieldGeometrySpace::change_leaf_owner(uint32_t shape, uint32_t owner,
                                           uint32_t order, std::string &e) {
  FieldGeometrySpace candidate = *this;
  std::unordered_set<uint32_t> affected;
  for (auto &list : candidate.owner_shapes_)
    list.erase(std::remove(list.begin(), list.end(), shape), list.end());
  if (owner != none)
    candidate.owner_shapes_[owner].push_back(shape);
  for (uint32_t i = 0; i < candidate.instances_.size(); ++i) {
    auto &entry = candidate.instances_[i];
    if (entry.contact.shape != shape)
      continue;
    entry.ownership_override = true;
    entry.contact.owner = owner;
    entry.shape_order = order;
    if (owner != none)
      entry.contact.stable_id =
          source_->node(source_->owner(owner).node).stable_id;
    affected.insert(i);
  }
  // Native PhysicsServer shape indices compact on removal and append on add.
  // Keep the stable source ShapeID separately from that transient native index.
  for (uint32_t o = 0; o < candidate.owner_shapes_.size(); ++o) {
    uint32_t index = 0;
    for (auto s : candidate.owner_shapes_[o]) {
      for (auto &entry : candidate.instances_)
        if (entry.contact.shape == s && entry.contact.owner == o)
          entry.contact.native_shape_index = index + entry.contact.part;
      index += source_->shape(s).part_count;
    }
  }
  candidate.rebuild_dependencies();
  if (!candidate.refresh({source_->shape(shape).node}, e))
    return false;
  *this = std::move(candidate);
  e.clear();
  return true;
}
bool FieldGeometrySpace::detach_leaf_polygon(const FieldSceneActionsData &a,
                                             uint32_t id, std::string &e) {
  uint32_t node = 0, shape = 0;
  if (!admit_leaf_polygon(a, id, node, shape, e))
    return false;
  // There is no CollisionObject2D owner while outside the tree. The native
  // Polygon node remains live; subsequent queue work uses its cached identity.
  return change_leaf_owner(
      shape, none, nodes_[node].deleted ? 0 : source_->node(node).order, e);
}
bool FieldGeometrySpace::attach_leaf_polygon(const FieldSceneActionsData &a,
                                             uint32_t id, uint32_t parent,
                                             uint32_t order, std::string &e) {
  uint32_t node = 0, shape = 0;
  if (!admit_leaf_polygon(a, id, node, shape, e))
    return false;
  const auto *p = a.reference(parent);
  bool allowed = false;
  for (const auto &b : a.bindings())
    if (b.kind == 1 && b.parent_id == parent)
      for (const auto &o : b.objects)
        if (o.source_id == id)
          allowed = true;
  if (!p || !allowed || (p->kind != 1 && p->kind != 3)) {
    e = "Field live polygon destination outside source binding";
    return false;
  }
  uint32_t owner = none;
  if (p->kind == 3) {
    auto n = ids_.find(parent);
    if (n == ids_.end() || nodes_[n->second].deleted) {
      e = "Field live polygon parent Area unavailable";
      return false;
    }
    for (uint32_t i = 0; i < source_->owner_count(); ++i)
      if (source_->owner(i).node == n->second) {
        if (source_->owner(i).kind != 4 || owner != none) {
          e = "Field live polygon Area shape owner rejected";
          return false;
        }
        owner = i;
      }
    if (owner == none) {
      e = "Field live polygon parent lacks physical owner";
      return false;
    }
  }
  // A TileMap has collision properties but is not a CollisionObject2D. Source
  // CollisionPolygon2D::_notification consequently registers no shape owner.
  return change_leaf_owner(shape, owner, order, e);
}
bool FieldGeometrySpace::live_polygon_parts(uint32_t owner_id,
                                            uint32_t shape_id, bool &attached,
                                            bool &disabled,
                                            std::vector<std::vector<Vec2>> &out,
                                            std::string &e) const {
  if (!source_) {
    e = "Field live polygon source unavailable";
    return false;
  }
  auto node = ids_.find(shape_id);
  if (node == ids_.end() ||
      source_->string(source_->node(node->second).class_name) !=
          "CollisionPolygon2D") {
    e = "Field live polygon shape identity rejected";
    return false;
  }
  bool present = false, blocked = false, found = false;
  std::vector<std::vector<Vec2>> parts;
  for (const auto &entry : instances_)
    if (source_->shape(entry.contact.shape).node == node->second) {
      found = true;
      if (entry.deleted || entry.contact.owner == none)
        continue;
      if (entry.contact.stable_id != owner_id) {
        e = "Field live polygon belongs to another actual owner";
        return false;
      }
      if (!entry.resolved || entry.actor.kind != FieldGeometryKind::Convex) {
        e = "Field live polygon native geometry/adapters pending";
        return false;
      }
      present = true;
      blocked = blocked || entry.disabled;
      parts.push_back(polygon(entry.actor));
    }
  if (!found) {
    e = "Field live polygon has no native parts";
    return false;
  }
  attached = present;
  disabled = blocked;
  out = std::move(parts);
  e.clear();
  return true;
}

namespace {
using NativeValue = std::shared_ptr<const GlobalYamlValue>;
NativeValue native_get(NativeValue p, std::string_view k) {
  return p ? p->get(k) : NativeValue{};
}
bool native_text(NativeValue p, std::string &s) {
  if (!p || p->kind != 4)
    return false;
  s = p->string;
  return true;
}
bool native_number(NativeValue p, double &n) {
  if (!p)
    return false;
  if (p->kind == 2) {
    n = double(p->integer);
    return true;
  }
  if (p->kind == 3) {
    n = p->real;
    return std::isfinite(n);
  }
  std::string t, v;
  if (!native_text(native_get(p, "type"), t) || (t != "real" && t != "int64") ||
      !native_text(native_get(p, "value"), v))
    return false;
  char *end = nullptr;
  n = std::strtod(v.c_str(), &end);
  return end == v.c_str() + v.size() && std::isfinite(n);
}
bool native_integer(NativeValue p, uint32_t &v) {
  double n;
  if (!native_number(p, n) || n < 0 || n > double(UINT32_MAX) ||
      std::floor(n) != n)
    return false;
  v = uint32_t(n);
  return true;
}
bool native_bool(NativeValue p, bool &v) {
  if (!p || p->kind != 1)
    return false;
  v = p->boolean;
  return true;
}
bool native_vector(NativeValue p, Vec2 &v) {
  std::string t;
  double x, y;
  if (!native_text(native_get(p, "type"), t) || t != "Vector2" ||
      !native_number(native_get(p, "x"), x) ||
      !native_number(native_get(p, "y"), y) || std::abs(x) >= 1000000 ||
      std::abs(y) >= 1000000)
    return false;
  v = {float(x), float(y)};
  return true;
}
bool source_player_node(const PlayerInitializationData &d,
                        FieldNodeTreeRuntime &t, FieldGlobalRegistry &r,
                        FieldObjectId id, std::string &e) {
  const auto *n = t.descriptor(id);
  const auto *s = t.state(id);
  FieldIdentity identity;
  const auto *source = n ? d.recipe().record(n->id) : nullptr;
  if (!d.valid() || !n || !s || !s->alive || !source ||
      source->path != n->path || source->native_class != n->native_class ||
      source->script_sha != n->script_sha || r.tree_owner(id).get() != &t ||
      !r.object_exists(id) || !t.object_identity(id, identity) ||
      identity.scene_id != d.recipe().identity().scene_id ||
      identity.source_sha256 != d.recipe().identity().source_sha256 ||
      identity.upstream_commit != d.identity().upstream_commit) {
    e = "Field Player physics actual source node/Registry identity rejected";
    return false;
  }
  return true;
}
} // namespace
namespace {
bool dialogue_node(const FieldDialogueVisualData &d,const FieldNodeRecipeData &recipe,
 FieldNodeTreeRuntime &tree,FieldGlobalRegistry &registry,FieldObjectId object,std::string &e){
 const auto *state=tree.state(object);const auto *node=tree.descriptor(object);
 const auto *source=node?recipe.record(node->id):nullptr;FieldIdentity identity;
 if(!d.valid()||!recipe.valid()||d.recipe_sha()!=recipe.ir_sha256()||
    !state||!state->alive||!node||!source||!d.node(node->id)||
    source->path!=node->path||source->native_class!=node->native_class||source->script_sha!=node->script_sha||
    registry.tree_owner(object).get()!=&tree||!registry.object_exists(object)||
    !tree.object_identity(object,identity)||identity.scene_id!=recipe.identity().scene_id||
    identity.source_sha256!=recipe.identity().source_sha256||identity.upstream_commit!=recipe.identity().upstream_commit||
    d.identity().scene_id!=identity.scene_id||d.identity().source_sha256!=identity.source_sha256||
    d.identity().upstream_commit!=identity.upstream_commit){e="Dialogue Camera shape actual source/Registry rejected";return false;}
 return true;
}
}
bool FieldGeometrySpace::reserve_dialogue_camera_owner(const FieldDialogueVisualData &d,
 const FieldNodeRecipeData &recipe,FieldNodeTreeRuntime &tree,FieldGlobalRegistry &registry,
 FieldObjectId object,std::string &e){
 if(!source_||source_->identity().upstream_commit!=d.identity().upstream_commit||
    !dialogue_node(d,recipe,tree,registry,object,e)||dynamic_owners_.count(object)||next_rid_==UINT64_MAX)return false;
 bool found=false;for(const auto &camera:d.camera().records())found=found||tree.descriptor(object)->id==camera.area_id;
 if(!found||tree.descriptor(object)->native_class!="Area2D"){e="Dialogue Camera native Area identity rejected";return false;}
 DynamicOwner owner;owner.tree=&tree;owner.registry=&registry;owner.object=object;owner.rid=++next_rid_;owner.dialogue=&d;owner.recipe=&recipe;
 dynamic_owners_.emplace(object,owner);return true;
}
bool FieldGeometrySpace::register_dialogue_camera_shape(const FieldDialogueVisualData &d,
 const FieldNodeRecipeData &recipe,FieldNodeTreeRuntime &tree,FieldGlobalRegistry &registry,
 FieldObjectId area,FieldObjectId shape,std::string &e){
 if(!dialogue_node(d,recipe,tree,registry,area,e)||!dialogue_node(d,recipe,tree,registry,shape,e))return false;
 auto owner=dynamic_owners_.find(area);const auto *state=tree.state(shape);const auto *node=tree.descriptor(shape);
 const FieldGameCameraDescriptor *camera=nullptr;
 for(const auto &c:d.camera().records())if(c.area_id==tree.descriptor(area)->id&&c.shape_id==node->id)camera=&c;
 if(owner==dynamic_owners_.end()||owner->second.dialogue!=&d||owner->second.recipe!=&recipe||
    !camera||!tree.state(area)->inside||!state->inside||!state->bound||!tree.state(area)->bound||
    state->parent!=area||node->native_class!="CollisionShape2D"||(camera->area_flags&8)){
   e="Dialogue Camera exact Rectangle native parent/Enter rejected";return false;}
 for(const auto &v:dynamic_)if(v.contact.actual_shape==shape){e="Dialogue native Camera shape duplicate registration";return false;}
 FieldTransform world;if(!tree.world_transform(shape,world,e))return false;
 DynamicInstance instance;instance.tree=&tree;instance.registry=&registry;instance.dialogue=&d;instance.recipe=&recipe;
 instance.player=area;instance.disabled=camera->area_flags&4;
 instance.contact={none,none,0,camera->area_id,0,area,shape};
 instance.actor.kind=FieldGeometryKind::Rectangle;instance.actor.transform={world[0],world[1],world[2]};instance.actor.extents=camera->shape_extents;
 instance.actor.stable_id=camera->area_id;instance.actor.layer=camera->area_layer;instance.actor.mask=camera->area_mask;
 instance.actor.area=true;instance.actor.monitorable=camera->area_flags&2;
 instance.owner.kind=4;instance.owner.layer=camera->area_layer;instance.owner.mask=camera->area_mask;
 instance.owner.flags=(camera->area_flags&1?2u:0u)|(camera->area_flags&2?4u:0u);
 instance.shape.kind=uint32_t(FieldGeometryKind::Rectangle);instance.shape.flags=instance.disabled?1u:0u;instance.shape.part_count=1;
 if(!actor_ok(instance.actor)){e="Dialogue Camera Rectangle geometry invalid";return false;}
 dynamic_.push_back(instance);return true;
}
bool FieldGeometrySpace::reserve_grass_owner(const GrassNativeData&d,FieldNodeTreeRuntime&t,FieldGlobalRegistry&r,FieldObjectId object,std::string&e){
 const auto*n=t.descriptor(object);const auto*s=t.state(object);FieldIdentity id;
 if(!source_||!d.valid()||!n||!s||!s->alive||n->id!=d.node(GrassNativeRole::Area)||!d.native_matches(*n)||r.tree_owner(object).get()!=&t||!r.object_exists(object)||!t.object_identity(object,id)||id.scene_id!=d.identity().scene_id||id.source_sha256!=d.identity().source_sha256||id.upstream_commit!=d.identity().upstream_commit||source_->identity().upstream_commit!=id.upstream_commit||dynamic_owners_.count(object)||next_rid_==UINT64_MAX){e="Grass actual native Area/RID source allocation rejected";return false;}
 DynamicOwner o;o.tree=&t;o.registry=&r;o.object=object;o.rid=++next_rid_;o.grass=&d;dynamic_owners_.emplace(object,o);return true;
}
bool FieldGeometrySpace::register_grass_shape(const GrassNativeData&d,FieldNodeTreeRuntime&t,FieldGlobalRegistry&r,FieldObjectId area,FieldObjectId shape,std::string&e){
 const auto*o=t.state(area);const auto*s=t.state(shape);const auto*n=t.descriptor(shape);auto owner=dynamic_owners_.find(area);
 if(!d.valid()||owner==dynamic_owners_.end()||owner->second.grass!=&d||owner->second.tree!=&t||owner->second.registry!=&r||!o||!s||!n||!o->inside||!s->inside||!o->bound||!s->bound||!o->ready_notified||!s->ready_notified||s->parent!=area||n->id!=d.node(GrassNativeRole::Shape)||!d.native_matches(*n)||r.tree_owner(shape).get()!=&t){e="Grass actual native shape/Ready/parent rejected";return false;}
 for(const auto&v:dynamic_)if(v.contact.actual_shape==shape){e="Grass source shape registered twice";return false;}
 FieldTransform world;if(!t.world_transform(shape,world,e))return false;DynamicInstance v;v.grass=&d;v.tree=&t;v.registry=&r;v.player=area;v.disabled=d.disabled();v.contact={none,none,0,d.node(GrassNativeRole::Area),0,area,shape};v.actor.kind=FieldGeometryKind::Rectangle;v.actor.transform={world[0],world[1],world[2]};v.actor.extents=d.profile().collision_extents;v.actor.stable_id=v.contact.stable_id;v.actor.layer=d.profile().collision_layer;v.actor.mask=d.profile().collision_mask;v.actor.area=true;v.actor.monitorable=d.monitorable();v.owner.kind=4;v.owner.layer=v.actor.layer;v.owner.mask=v.actor.mask;v.owner.flags=(d.monitoring()?2u:0u)|(d.monitorable()?4u:0u);v.shape.kind=uint32_t(v.actor.kind);v.shape.flags=v.disabled?1u:0u;v.shape.part_count=1;if(!actor_ok(v.actor)){e="Grass exact Rectangle native matrix invalid";return false;}dynamic_.push_back(v);return true;
}
bool FieldGeometrySpace::reserve_player_owner(const PlayerInitializationData &d,
                                              FieldNodeTreeRuntime &t,
                                              FieldGlobalRegistry &r,
                                              FieldObjectId object,
                                              std::string &e) {
  if (!source_ ||
      source_->identity().upstream_commit != d.identity().upstream_commit ||
      !source_player_node(d, t, r, object, e))
    return false;
  const auto *n = t.descriptor(object);
  if (n->native_class != "Area2D" && n->native_class != "KinematicBody2D") {
    e = "Field dynamic collision native owner class rejected";
    return false;
  }
  auto existing = dynamic_owners_.find(object);
  if (existing != dynamic_owners_.end()) {
    if (existing->second.data != &d || existing->second.tree != &t ||
        existing->second.registry != &r) {
      e = "Field dynamic collision owner changed";
      return false;
    }
    return true;
  }
  if (next_rid_ == UINT64_MAX) {
    e = "Field native Physics RID allocation exhausted";
    return false;
  }
  dynamic_owners_.emplace(object,
                          DynamicOwner{&d, &t, &r, object, ++next_rid_});
  return true;
}
bool FieldGeometrySpace::register_player_shape(
    const PlayerInitializationData &d, FieldNodeTreeRuntime &t,
    FieldGlobalRegistry &r, FieldObjectId player, FieldObjectId shape,
    std::string &e) {
  if (!source_player_node(d, t, r, player, e) ||
      !source_player_node(d, t, r, shape, e))
    return false;
  const auto *root = t.descriptor(player);
  const auto *sn = t.descriptor(shape);
  const auto *ss = t.state(shape);
  if (root->id != d.recipe().identity().scene_id || !ss->inside || !ss->bound ||
      !sn->script.empty() ||
      (sn->native_class != "CollisionShape2D" &&
       sn->native_class != "CollisionPolygon2D")) {
    e = "Field Player native shape enter lifecycle rejected";
    return false;
  }
  for (const auto &old : dynamic_)
    if (old.contact.actual_shape == shape) {
      e = "Field Player shape already registered";
      return false;
    }
  auto nodes = native_get(d.native_source(), "nodes");
  auto resources = native_get(d.native_source(), "resources");
  if (!nodes || nodes->kind != 5 || !resources || resources->kind != 5) {
    e = "Field Player source geometry closure unavailable";
    return false;
  }
  std::vector<DynamicInstance> candidate;
  for (auto ownerNode : nodes->array) {
    std::string path, cls;
    if (!native_text(native_get(ownerNode, "path"), path) ||
        !native_text(native_get(ownerNode, "class"), cls))
      return false;
    if (cls != "Area2D" && cls != "KinematicBody2D")
      continue;
    auto sourceOwners = native_get(ownerNode, "physics_shape_owners");
    if (!sourceOwners || sourceOwners->kind != 5) {
      e = "Field Player native shape ownership missing";
      return false;
    }
    uint32_t nativeIndex = 0;
    for (auto so : sourceOwners->array) {
      std::string shapePath, refType;
      auto refs = native_get(so, "shapes");
      if (!native_text(native_get(native_get(so, "owner"), "type"), refType) ||
          refType != "NodeReference" ||
          !native_text(native_get(native_get(so, "owner"), "path"),
                       shapePath) ||
          !refs || refs->kind != 5) {
        e = "Field Player source shape owner reference rejected";
        return false;
      }
      if (shapePath != sn->path) {
        nativeIndex += uint32_t(refs->array.size());
        continue;
      }
      FieldObjectId actualOwner;
      if (!t.get_node(player, path, actualOwner, e) ||
          !source_player_node(d, t, r, actualOwner, e))
        return false;
      const auto *ownerState = t.state(actualOwner);
      if (!ownerState->inside || !ownerState->bound ||
          ss->parent != actualOwner) {
        e = "Field Player collision owner actual parent/enter mismatch";
        return false;
      }
      auto props = native_get(ownerNode, "properties");
      uint32_t layer, mask;
      bool monitoring = false, monitorable = true, disabled, oneWay;
      double oneWayMargin;
      if (!native_integer(native_get(props, "collision_layer"), layer) ||
          !native_integer(native_get(props, "collision_mask"), mask) ||
          !native_bool(native_get(so, "disabled"), disabled) ||
          !native_bool(native_get(so, "one_way"), oneWay) || oneWay ||
          !native_number(native_get(so, "one_way_margin"), oneWayMargin) ||
          oneWayMargin < 0 || oneWayMargin >= 1000000) {
        e = "Field Player collision masks/one-way properties rejected";
        return false;
      }
      if (cls == "Area2D") {
        uint32_t overrideMode;
        bool audioOverride;
        if (!native_bool(native_get(props, "monitoring"), monitoring) ||
            !native_bool(native_get(props, "monitorable"), monitorable) ||
            !native_integer(native_get(props, "space_override"),
                            overrideMode) ||
            overrideMode ||
            !native_bool(native_get(props, "audio_bus_override"),
                         audioOverride) ||
            audioOverride) {
          e = "Field Player Area override/monitoring mechanism unsupported";
          return false;
        }
      }
      FieldTransform world;
      if (!t.world_transform(shape, world, e))
        return false;
      for (uint32_t part = 0; part < refs->array.size(); ++part) {
        uint32_t resourceId;
        if (!native_integer(native_get(refs->array[part], "id"), resourceId)) {
          e = "Field Player source shape resource rejected";
          return false;
        }
        NativeValue resource;
        for (auto v : resources->array) {
          uint32_t id;
          if (native_integer(native_get(v, "id"), id) && id == resourceId)
            resource = v;
        }
        std::string resourceClass;
        if (!native_text(native_get(resource, "class"), resourceClass)) {
          e = "Field Player shape resource closure missing";
          return false;
        }
        DynamicInstance x;
        x.data = &d;
        x.tree = &t;
        x.registry = &r;
        x.player = player;
        x.disabled = disabled;
        x.contact = {none,
                     none,
                     part,
                     t.descriptor(actualOwner)->id,
                     nativeIndex + part,
                     actualOwner,
                     shape};
        x.actor.transform = {world[0], world[1], world[2]};
        x.actor.stable_id = x.contact.stable_id;
        x.actor.layer = layer;
        x.actor.mask = mask;
        x.actor.area = cls == "Area2D";
        x.actor.monitorable = monitorable;
        auto p = native_get(resource, "properties");
        double radius, height;
        if (resourceClass == "RectangleShape2D") {
          x.actor.kind = FieldGeometryKind::Rectangle;
          if (!native_vector(native_get(p, "extents"), x.actor.extents))
            return false;
        } else if (resourceClass == "CircleShape2D" ||
                   resourceClass == "CapsuleShape2D") {
          if (!native_number(native_get(p, "radius"), radius) || radius <= 0 ||
              radius >= 1000000)
            return false;
          x.actor.radius = float(radius);
          x.actor.kind = FieldGeometryKind::Circle;
          if (resourceClass == "CapsuleShape2D") {
            if (!native_number(native_get(p, "height"), height) || height < 0 ||
                height >= 1000000)
              return false;
            x.actor.kind = FieldGeometryKind::Capsule;
            x.actor.extents.y = float(height * .5);
          }
        } else if (resourceClass == "ConvexPolygonShape2D") {
          x.actor.kind = FieldGeometryKind::Convex;
          auto points = native_get(native_get(p, "points"), "value");
          if (!points || points->kind != 5)
            return false;
          for (auto v : points->array) {
            Vec2 q;
            if (!native_vector(v, q))
              return false;
            x.actor.points.push_back(q);
          }
        } else {
          e = "Field Player native source shape kind unsupported";
          return false;
        }
        if (!actor_ok(x.actor)) {
          e = "Field Player native shape/transform invalid";
          return false;
        }
        x.owner.kind = x.actor.area ? 4 : 2;
        x.owner.layer = layer;
        x.owner.mask = mask;
        x.owner.flags = (monitoring ? 2u : 0u) | (monitorable ? 4u : 0u);
        x.shape.kind = uint32_t(x.actor.kind);
        x.shape.flags = disabled ? 1 : 0;
        x.shape.part_count = 1;
        x.shape.owner_margin = float(oneWayMargin);
        candidate.push_back(std::move(x));
      }
      if (!dynamic_owners_.count(actualOwner)) {
        e = "Field Player collision owner native constructor/RID not executed";
        return false;
      }
      nativeIndex += uint32_t(refs->array.size());
    }
  }
  if (candidate.empty()) {
    e = "Field Player native shape ownership not found";
    return false;
  }
  dynamic_.insert(dynamic_.end(), candidate.begin(), candidate.end());
  return true;
}
bool FieldGeometrySpace::dynamic_actor(const DynamicInstance &d,
                                       FieldGeometryActor &a,
                                       std::string &e) const {
  if (d.dialogue) {
    if(!d.recipe||!dialogue_node(*d.dialogue,*d.recipe,*d.tree,*d.registry,d.contact.actual_owner,e)||
       !dialogue_node(*d.dialogue,*d.recipe,*d.tree,*d.registry,d.contact.actual_shape,e))return false;
  } else if (d.grass) {
    FieldIdentity identity;
    const auto *owner=d.tree->descriptor(d.contact.actual_owner);
    const auto *shape=d.tree->descriptor(d.contact.actual_shape);
    if(!d.grass->valid()||!d.registry||d.registry->tree_owner(d.contact.actual_owner).get()!=d.tree||d.registry->tree_owner(d.contact.actual_shape).get()!=d.tree||!owner||!shape||!d.grass->native_matches(*owner)||!d.grass->native_matches(*shape)||!d.tree->object_identity(d.contact.actual_owner,identity)||identity.scene_id!=d.grass->identity().scene_id||identity.source_sha256!=d.grass->identity().source_sha256||identity.upstream_commit!=d.grass->identity().upstream_commit){e="Grass dynamic native collision identity differs";return false;}
  } else if (!d.data ||
      !source_player_node(*d.data, *d.tree, *d.registry, d.contact.actual_owner,
                          e) ||
      !source_player_node(*d.data, *d.tree, *d.registry, d.contact.actual_shape,
                          e))
    return false;
  const auto *o = d.tree->state(d.contact.actual_owner);
  const auto *s = d.tree->state(d.contact.actual_shape);
  if (!o->inside || !s->inside || !o->bound || !s->bound ||
      s->parent != o->object) {
    e = "Field dynamic collision owner left actual world";
    return false;
  }
  FieldTransform world;
  if (!d.tree->world_transform(s->object, world, e))
    return false;
  a = d.actor;
  a.transform = {world[0], world[1], world[2]};
  if (!actor_ok(a)) {
    e = "Field dynamic collision live transform unsupported";
    return false;
  }
  return true;
}
bool FieldGeometrySpace::dynamic_filter(const DynamicInstance &d,
                                        const FieldGeometryFilter &f) const {
  if (d.owner.kind == 4 ? !f.areas : !f.bodies)
    return false;
  if (f.exclude_stable_id && d.contact.stable_id == f.exclude_stable_id)
    return false;
  if (f.monitoring_only && (d.owner.kind != 4 || !(d.owner.flags & 2)))
    return false;
  if (f.bilateral_mask)
    return (d.owner.layer & f.layer_mask) ||
           (d.owner.mask & f.reciprocal_layer);
  return (f.monitoring_only || (d.owner.layer & f.layer_mask)) &&
         (!f.require_reciprocal_mask || (d.owner.mask & f.reciprocal_layer));
}
bool FieldGeometrySpace::query_dynamic(FieldGeometryBounds b,
                                       const FieldGeometryFilter &f, size_t cap,
                                       std::vector<size_t> &out,
                                       std::string &e) const {
  std::vector<size_t> result;
  for (size_t i = 0; i < dynamic_.size(); ++i) {
    const auto &d = dynamic_[i];
    if (d.disabled || !dynamic_filter(d, f))
      continue;
    FieldGeometryActor a;
    if (!dynamic_actor(d, a, e))
      return false;
    if (!intersects(b, actor_bounds(a)))
      continue;
    if (result.size() >= cap) {
      e = "Field actual dynamic geometry capacity exceeded";
      return false;
    }
    result.push_back(i);
  }
  std::sort(result.begin(), result.end(), [&](size_t a, size_t c) {
    const auto &x = dynamic_[a].contact, &y = dynamic_[c].contact;
    return x.actual_owner != y.actual_owner
               ? x.actual_owner < y.actual_owner
               : x.native_shape_index < y.native_shape_index;
  });
  out = std::move(result);
  return true;
}
bool FieldGeometrySpace::remove_player_shape(FieldObjectId shape,
                                             std::string &e) {
  auto old = dynamic_.size();
  dynamic_.erase(std::remove_if(dynamic_.begin(), dynamic_.end(),
                                [&](const DynamicInstance &d) {
                                  return d.contact.actual_shape == shape;
                                }),
                 dynamic_.end());
  if (old == dynamic_.size()) {
    e = "Field Player shape removal lacks actual registration";
    return false;
  }
  return true;
}
bool FieldGeometrySpace::set_player_shape_disabled(FieldObjectId shape,
                                                   bool value, std::string &e) {
  bool found = false;
  for (auto &d : dynamic_)
    if (d.contact.actual_shape == shape) {
      d.disabled = value;
      d.shape.flags = value ? 1 : 0;
      found = true;
    }
  if (!found) {
    e = "Field Player disabled setter outside actual registered shape";
    return false;
  }
  return true;
}
bool FieldGeometrySpace::set_player_collision_mask(FieldObjectId owner,
                                                   uint32_t bit, bool value,
                                                   std::string &e) {
  if (bit >= 32 || !dynamic_owners_.count(owner)) {
    e = "Field Player native collision mask target rejected";
    return false;
  }
  bool found = false;
  for (auto &d : dynamic_)
    if (d.contact.actual_owner == owner && !d.disabled) {
      if (value)
        d.owner.mask |= uint32_t(1) << bit;
      else
        d.owner.mask &= ~(uint32_t(1) << bit);
      d.actor.mask = d.owner.mask;
      found = true;
    }
  if (!found) {
    e = "Field Player native collision mask shape absent";
    return false;
  }
  return true;
}
bool FieldGeometrySpace::player_shape_snapshot(const FieldGeometryContact &contact,
    FieldGeometryActor &actor,FieldGeometryOwner &owner,FieldGeometryShape &shape,
    bool &disabled,std::string &e)const{
  if(!contact.actual_owner||!contact.actual_shape){e="Player shape snapshot requires actual ObjectIDs";return false;}
  for(const auto &d:dynamic_){const auto &c=d.contact;
    if(c.actual_owner==contact.actual_owner&&c.actual_shape==contact.actual_shape&&
       c.part==contact.part&&c.stable_id==contact.stable_id&&c.native_shape_index==contact.native_shape_index){
      if(d.grass||!d.data||!dynamic_actor(d,actor,e))return false;
      owner=d.owner;shape=d.shape;disabled=d.disabled;e.clear();return true;
    }
  }
  e="Player shape snapshot has a stale actual shape contact";return false;
}
bool FieldGeometrySpace::player_shapes(FieldObjectId owner,
                                       std::vector<FieldGeometryContact> &out,
                                       std::string &e) const {
  std::vector<FieldGeometryContact> result;
  if (!dynamic_owners_.count(owner)) {
    e = "Field Player native collision owner unavailable";
    return false;
  }
  for (const auto &d : dynamic_)
    if (d.contact.actual_owner == owner) {
      FieldGeometryActor a;
      if (!dynamic_actor(d, a, e))
        return false;
      result.push_back(d.contact);
    }
  out = std::move(result);
  return true;
}
bool FieldGeometrySpace::player_owner_rid(FieldObjectId owner,
                                          FieldPhysicsRid &out,
                                          std::string &e) const {
  auto it = dynamic_owners_.find(owner);
  if (it == dynamic_owners_.end()) {
    e = "Field Player Physics RID owner absent";
    return false;
  }
  const auto &d = it->second;
  if (d.dialogue) {
    if(!d.recipe||!dialogue_node(*d.dialogue,*d.recipe,*d.tree,*d.registry,owner,e))return false;
  } else if (d.grass) {
    const auto*n=d.tree->descriptor(owner);FieldIdentity identity;
    if(!n||n->id!=d.grass->node(GrassNativeRole::Area)||!d.grass->native_matches(*n)||d.registry->tree_owner(owner).get()!=d.tree||!d.registry->object_exists(owner)||!d.tree->object_identity(owner,identity)||identity.scene_id!=d.grass->identity().scene_id||identity.source_sha256!=d.grass->identity().source_sha256||identity.upstream_commit!=d.grass->identity().upstream_commit){e="Grass native Physics RID same source owner unavailable";return false;}
  } else if (!d.data || !source_player_node(*d.data, *d.tree, *d.registry, owner, e))
    return false;
  out = {this, d.rid};
  return true;
}
bool FieldGeometrySpace::physics_rid(const FieldGeometryContact &c,
                                     FieldPhysicsRid &out,
                                     std::string &e) const {
  if (c.actual_owner)
    return player_owner_rid(c.actual_owner, out, e);
  FieldGeometryActor a;
  FieldGeometryOwner o;
  FieldGeometryShape s;
  if (!live_geometry(c, a, o, s, e))
    return false;
  auto it = static_rids_.find(c.owner);
  if (it == static_rids_.end()) {
    if (next_rid_ == UINT64_MAX) {
      e = "Field Physics RID exhausted";
      return false;
    }
    it = static_rids_.emplace(c.owner, ++next_rid_).first;
  }
  out = {this, it->second};
  return true;
}
bool FieldGeometrySpace::rid_alive(FieldPhysicsRid rid) const {
  if (rid.space != this || !rid.handle)
    return false;
  for (const auto &x : static_rids_)
    if (x.second == rid.handle)
      return true;
  for (const auto &x : dynamic_owners_)
    if (x.second.rid == rid.handle)
      return true;
  return false;
}
bool FieldGeometrySpace::retire_player_owner(FieldObjectId owner,
                                             std::string &e) {
  if (!dynamic_owners_.count(owner)) {
    e = "Field Physics native retirement lacks live owner";
    return false;
  }
  for (const auto &x : dynamic_)
    if (x.contact.actual_owner == owner) {
      e = "Field Physics owner retirement still has registered shapes";
      return false;
    }
  dynamic_owners_.erase(owner);
  return true;
}

bool FieldGeometrySpace::apply_npc_interaction(const FieldNpcRuntime &runtime,
                                               uint32_t id,
                                               FieldNodeTreeRuntime &tree,
                                               FieldGlobalRegistry &registry,
                                               std::string &e) {
  const auto *d = runtime.data();
  const FieldNpcDescriptor *desc = nullptr;
  const FieldNpcInstance *body = nullptr;
  if (d)
    for (const auto &v : d->npcs())
      if (v.id == id)
        desc = &v;
  for (const auto &v : runtime.npcs())
    if (v.id == id && !v.destroyed)
      body = &v;
  if (!source_ || !d || !desc || !body || !body->ready ||
      body->geometry.size() != 9 ||
      d->source_pin() != source_->identity().upstream_commit) {
    e = "NPC interaction shape actual runtime/source unavailable";
    return false;
  }
  auto actual = tree.source_object(id);
  auto state = tree.state(actual);
  auto node = tree.descriptor(actual);
  if (!state || !state->alive || !state->inside || !state->bound || !node ||
      node->path != desc->node || registry.tree_owner(actual).get() != &tree) {
    e = "NPC interaction shape actual ObjectDB owner unavailable";
    return false;
  }
  const auto &g = body->geometry[1];
  const auto &original = desc->geometry[1];
  if (g.role != 2 || g.kind != 1 || original.role != 2 || original.kind != 1 ||
      !std::isfinite(g.value.x) || !std::isfinite(g.value.y) ||
      g.value.x <= 0 || g.value.y <= 0 || g.value.x > 1000000 ||
      g.value.y > 1000000) {
    e = "NPC interaction rectangle source role/value rejected";
    return false;
  }
  uint32_t shape = none;
  for (uint32_t i = 0; i < source_->shape_count(); ++i) {
    auto sh = source_->shape(i);
    auto n = source_->node(sh.node);
    auto p = source_->geometry(sh.part_first);
    auto owner = source_->node(source_->owner(sh.owner).node);
    if (source_->string(n.path).compare(0, desc->node.size() + 1,
                                        desc->node + "/") != 0 ||
        p.kind != FieldGeometryKind::Rectangle || sh.part_count != 1)
      continue;
    auto candidate = tree.source_object(n.stable_id);
    auto actualShape = tree.state(candidate);
    if (!actualShape || !actualShape->alive || !actualShape->inside ||
        !actualShape->bound || registry.tree_owner(candidate).get() != &tree)
      continue;
    if (p.parameters[0] != original.value.x ||
        p.parameters[1] != original.value.y ||
        source_->owner(sh.owner).layer != original.layer ||
        source_->owner(sh.owner).mask != original.mask)
      continue;
    // Distinguish the interaction rectangle from other same-sized shapes by its
    // actual source world offset and owner relation, before any runtime resize.
    if (sh.world.origin.x - desc->position.x != original.offset.x ||
        sh.world.origin.y - desc->position.y != original.offset.y ||
        owner.parent == none || source_->node(owner.parent).stable_id != id)
      continue;
    if (shape != none) {
      e = "NPC interaction source shape identity ambiguous";
      return false;
    }
    shape = i;
  }
  if (shape == none) {
    e = "NPC interaction source rectangle absent";
    return false;
  }
  auto old = npc_rectangle_extents_.find(shape);
  const bool had = old != npc_rectangle_extents_.end();
  Vec2 previous = had ? old->second : Vec2{};
  npc_rectangle_extents_[shape] = g.value;
  if (!refresh({source_->shape(shape).node}, e)) {
    if (had)
      npc_rectangle_extents_[shape] = previous;
    else
      npc_rectangle_extents_.erase(shape);
    return false;
  }
  return true;
}
} // namespace encore::upstream
