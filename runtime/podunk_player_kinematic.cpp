#include "encore/player_tree_rebind.hpp"
#include "encore/podunk_player_kinematic.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
namespace encore::upstream {
namespace {
using V = GlobalYamlValue;
using P = std::shared_ptr<const V>;
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
P get(P p, std::string_view k) { return p ? p->get(k) : P{}; }
bool text(P p, std::string &v) {
  if (!p || p->kind != 4)
    return false;
  v = p->string;
  return true;
}
bool number(P p, double &v) {
  if (!p)
    return false;
  if (p->kind == 2) {
    v = double(p->integer);
    return true;
  }
  if (p->kind == 3) {
    v = p->real;
    return std::isfinite(v);
  }
  std::string type, raw;
  if (!text(get(p, "type"), type) || !text(get(p, "value"), raw) ||
      (type != "int64" && type != "real"))
    return false;
  char *end = nullptr;
  v = std::strtod(raw.c_str(), &end);
  return end == raw.c_str() + raw.size() && std::isfinite(v);
}
bool integer(P p, uint32_t &v) {
  double n;
  if (!number(p, n) || n < 0 || n > double(UINT32_MAX) || std::floor(n) != n)
    return false;
  v = uint32_t(n);
  return true;
}
bool boolean(P p, bool &v) {
  if (!p || p->kind != 1)
    return false;
  v = p->boolean;
  return true;
}
bool vector(P p, Vec2 &v) {
  std::string type;
  double x, y;
  if (!text(get(p, "type"), type) || type != "Vector2" ||
      !number(get(p, "x"), x) || !number(get(p, "y"), y) ||
      std::abs(x) > 1000000 || std::abs(y) > 1000000)
    return false;
  v = {float(x), float(y)};
  return true;
}
bool transform(P p, FieldGeometryTransform &v) {
  std::string type;
  return text(get(p, "type"), type) && type == "Transform2D" &&
         vector(get(p, "x"), v.x) && vector(get(p, "y"), v.y) &&
         vector(get(p, "origin"), v.origin);
}
bool finite(Vec2 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::abs(v.x) <= 1000000 &&
         std::abs(v.y) <= 1000000;
}
Vec2 add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
Vec2 sub(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
Vec2 mul(Vec2 a, float b) { return {a.x * b, a.y * b}; }
float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
Vec2 xf(FieldGeometryTransform t, Vec2 p) {
  return add(t.origin, add(mul(t.x, p.x), mul(t.y, p.y)));
}
FieldGeometryTransform geom(FieldTransform t) { return {t[0], t[1], t[2]}; }
bool inverse(FieldGeometryTransform t, Vec2 p, Vec2 &out) {
  float det = cross(t.x, t.y);
  if (!std::isfinite(det) || det == 0)
    return false;
  auto d = sub(p, t.origin);
  out = {(t.y.y * d.x - t.y.x * d.y) / det, (-t.x.y * d.x + t.x.x * d.y) / det};
  return finite(out);
}
P source_node(P native, std::string_view path) {
  auto nodes = get(native, "nodes");
  if (!nodes || nodes->kind != 5)
    return {};
  for (auto node : nodes->array) {
    std::string actual;
    if (text(get(node, "path"), actual) && actual == path)
      return node;
  }
  return {};
}
bool contains(FieldGeometryBounds a, MotionQueryBounds b) {
  return b.initialized && a.minimum.x <= b.minimum.x &&
         a.minimum.y <= b.minimum.y && a.maximum.x >= b.maximum.x &&
         a.maximum.y >= b.maximum.y;
}
FieldGeometryBounds grow(FieldGeometryBounds b, MotionQueryBounds q) {
  return {
      {std::min(b.minimum.x, q.minimum.x), std::min(b.minimum.y, q.minimum.y)},
      {std::max(b.maximum.x, q.maximum.x), std::max(b.maximum.y, q.maximum.y)}};
}
} // namespace
bool PodunkPlayerKinematic::initialize(
    const PlayerInitializationData &d, const PlayerMotionData &motion,
    PlayerInitializationBody &body, FieldNodeTreeRuntime &t,
    FieldGlobalRegistry &registry, FieldMapSpace &map,
    FieldGeometrySpace &space, PodunkPlayerKinematicHost host, std::string &e) {
  if (data_ || !d.valid() || !motion.valid() || body.data() != &d ||
      body.tree() != &t || !body.constructed() ||
      registry.tree_owner(body.object()).get() != &t || !map.source() ||
      !space.source() || !host.source_object ||
      map.source()->identity().upstream_commit !=
          d.identity().upstream_commit ||
      space.source()->identity().upstream_commit !=
          d.identity().upstream_commit ||
      map.source()->source_scene() != space.source()->source_scene() ||
      map.source()->identity().scene_id !=
          space.source()->identity().scene_id ||
      map.source()->identity().source_sha256 !=
          space.source()->identity().source_sha256)
    return fail(
        e, "Player Kinematic checked source/actual scene owner unavailable");
  auto native = d.native_source();
  auto root = source_node(native, ".");
  auto props = get(root, "properties");
  auto owners = get(root, "physics_shape_owners");
  if (!root || !owners || owners->kind != 5 || owners->array.size() != 1)
    return fail(e, "Player native collider owner closure unsupported");
  auto owner = owners->array[0];
  std::string path, type;
  if (!text(get(get(owner, "owner"), "path"), path) ||
      !text(get(get(owner, "owner"), "type"), type) || type != "NodeReference")
    return fail(e, "Player native collider NodeReference rejected");
  auto shapes = get(owner, "shapes");
  if (!shapes || shapes->kind != 5 || shapes->array.size() != 1)
    return fail(e, "Player native collider part count unsupported");
  uint32_t resource;
  if (!integer(get(shapes->array[0], "id"), resource))
    return fail(e, "Player native collider resource rejected");
  auto resources = get(native, "resources");
  P shape_resource;
  if (!resources || resources->kind != 5)
    return fail(e, "Player native resource closure absent");
  for (auto row : resources->array) {
    uint32_t id;
    if (integer(get(row, "id"), id) && id == resource)
      shape_resource = row;
  }
  std::string cls;
  if (!text(get(shape_resource, "class"), cls) || cls != "ConvexPolygonShape2D")
    return fail(e, "Player source polygon resource unsupported");
  auto points = get(get(get(shape_resource, "properties"), "points"), "value");
  if (!points || points->kind != 5 || points->array.size() < 3)
    return fail(e, "Player source polygon points absent");
  std::vector<Vec2> hull;
  for (auto point : points->array) {
    Vec2 v;
    if (!vector(point, v))
      return fail(e, "Player source polygon point rejected");
    hull.push_back(v);
  }
  FieldGeometryTransform source_shape;
  if (!transform(get(owner, "transform"), source_shape))
    return fail(e, "Player source polygon transform rejected");
  bool disabled = false, one_way = false;
  if (!boolean(get(owner, "disabled"), disabled) ||
      !boolean(get(owner, "one_way"), one_way) || one_way)
    return fail(e, "Player one-way native collider unsupported");
  FieldObjectId actual_collider, actual_ray;
  if (!t.get_node(body.object(), path, actual_collider, e) ||
      !t.get_node(body.object(), motion.node(PlayerMotionNode::EventRay),
                  actual_ray, e))
    return false;
  auto collision_desc = t.descriptor(actual_collider);
  auto ray_desc = t.descriptor(actual_ray);
  if (!collision_desc || collision_desc->native_class != "CollisionPolygon2D" ||
      !ray_desc || ray_desc->native_class != "RayCast2D")
    return fail(e, "Player collider/ray actual native class rejected");
  auto ray_props =
      get(source_node(native, motion.node(PlayerMotionNode::EventRay)),
          "properties");
  uint32_t layer, mask, ray_mask;
  double margin;
  Vec2 cast;
  bool enabled = false, bodies = false, areas = false, exclude = false;
  if (!integer(get(props, "collision_layer"), layer) ||
      !integer(get(props, "collision_mask"), mask) ||
      !number(get(props, "collision/safe_margin"), margin) || margin < 0 ||
      margin > 100 || !integer(get(ray_props, "collision_mask"), ray_mask) ||
      !vector(get(ray_props, "cast_to"), cast) ||
      !boolean(get(ray_props, "enabled"), enabled) ||
      !boolean(get(ray_props, "collide_with_bodies"), bodies) ||
      !boolean(get(ray_props, "collide_with_areas"), areas) ||
      !boolean(get(ray_props, "exclude_parent"), exclude))
    return fail(e, "Player Kinematic/ray source properties rejected");
  // Cross-check the true native shape owner's transform and Scene recipe.
  const auto &local = collision_desc->local;
  if (!finite(source_shape.origin) || local[0].x != source_shape.x.x ||
      local[0].y != source_shape.x.y || local[1].x != source_shape.y.x ||
      local[1].y != source_shape.y.y || local[2].x != source_shape.origin.x ||
      local[2].y != source_shape.origin.y)
    return fail(e, "Player native shape transform/recipe differs");
  data_ = &d;
  motion_ = &motion;
  body_ = &body;
  tree_ = &t;
  registry_ = &registry;
  map_ = &map;
  space_ = &space;
  host_ = std::move(host);
  collider_ = actual_collider;
  ray_ = actual_ray;
  hull_ = std::move(hull);
  disabled_ = disabled;
  margin_ = float(margin);
  layer_ = layer;
  mask_ = mask;
  ray_mask_ = ray_mask;
  cast_ = cast;
  ray_enabled_ = enabled;
  ray_bodies_ = bodies;
  ray_areas_ = areas;
  exclude_parent_ = exclude;
  return true;
}
bool PodunkPlayerKinematic::live(std::string &e) const {
  auto root = body_ && tree_ ? tree_->state(body_->object()) : nullptr;
  auto collider = tree_ ? tree_->state(collider_) : nullptr;
  auto ray = tree_ ? tree_->state(ray_) : nullptr;
  return data_ && !poisoned_ && body_->constructed() && root && root->alive &&
                 root->inside && root->bound && collider && collider->alive &&
                 collider->inside && collider->bound && ray && ray->alive &&
                 ray->inside && ray->bound &&
                 registry_->tree_owner(body_->object()).get() == tree_
             ? true
             : fail(e, "Player native Kinematic/shape/ray lifecycle pending");
}
bool PodunkPlayerKinematic::begin_physics(uint64_t epoch, float delta,
                                          std::string &e) {
  if (!live(e) || epoch == 0 || epoch <= epoch_ || !std::isfinite(delta) ||
      delta < 0)
    return fail(e, "Player actual physics epoch/delta rejected");
  epoch_ = epoch;
  delta_ = delta;
  return true;
}
bool PodunkPlayerKinematic::move_and_slide(FieldObjectId player, Vec2 velocity,
                                           Vec2 &returned, std::string &e) {
  if (!live(e) || player != body_->object() || !epoch_ || !finite(velocity))
    return fail(e, "Player source Kinematic call outside actual physics owner");
  FieldTransform root_world, shape_world;
  if (!tree_->world_transform(player, root_world, e) ||
      !tree_->world_transform(collider_, shape_world, e))
    return false;
  auto position = root_world[2];
  SlideResult solved;
  if (disabled_) {
    solved = {add(position, mul(velocity, delta_)), velocity};
    if (!finite(solved.position))
      return fail(e, "Player disabled collider motion overflow");
  } else {
    FieldGeometryActor actor;
    actor.kind = FieldGeometryKind::Convex;
    actor.transform = geom(shape_world);
    actor.transform.origin = sub(actor.transform.origin, position);
    actor.points = hull_;
    PodunkSolidShape actor_shape;
    if (!podunk_prepare_shape(actor, actor_shape, e))
      return false;
    Vec2 travel = mul(velocity, delta_);
    FieldGeometryBounds area{{position.x + actor_shape.minimum.x +
                                  std::min(0.0f, travel.x) - margin_,
                              position.y + actor_shape.minimum.y +
                                  std::min(0.0f, travel.y) - margin_},
                             {position.x + actor_shape.maximum.x +
                                  std::max(0.0f, travel.x) + margin_,
                              position.y + actor_shape.maximum.y +
                                  std::max(0.0f, travel.y) + margin_}};
    bool closed = false;
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
      std::vector<PodunkSolidShape> solids;
      std::vector<uint32_t> polygons;
      FieldMapRect query{area.minimum, area.maximum};
      if (!map_->collision_polygons(query, mask_, host_.map_gates, 4096,
                                    polygons, e))
        return false;
      for (auto index : polygons) {
        auto p = map_->polygon(index);
        if (p.kind == 2) {
          FieldMapRect local;
          auto tr = map_->shape_transform(index);
          FieldGeometryTransform transform{tr.x, tr.y, tr.origin};
          Vec2 first;
          bool initial = true;
          for (float x : {query.minimum.x, query.maximum.x})
            for (float y : {query.minimum.y, query.maximum.y}) {
              if (!inverse(transform, {x, y}, first))
                return fail(e, "Player map segment query transform rejected");
              if (initial) {
                local = {first, first};
                initial = false;
              } else {
                local.minimum.x = std::min(local.minimum.x, first.x);
                local.minimum.y = std::min(local.minimum.y, first.y);
                local.maximum.x = std::max(local.maximum.x, first.x);
                local.maximum.y = std::max(local.maximum.y, first.y);
              }
            }
          std::vector<uint32_t> pairs;
          if (!map_->source()->concave_segments(index, local,
                                                4096 - solids.size(), pairs, e))
            return false;
          auto g = map_->source()->local_geometry(p.geometry);
          for (auto pair : pairs) {
            FieldGeometryActor shape;
            shape.kind = FieldGeometryKind::Segment;
            shape.transform = transform;
            shape.points = {
                map_->source()->local_point(g.point_first + pair * 2),
                map_->source()->local_point(g.point_first + pair * 2 + 1)};
            PodunkSolidShape s;
            if (!podunk_prepare_shape(shape, s, e))
              return false;
            solids.push_back(std::move(s));
          }
        } else {
          FieldGeometryActor shape;
          shape.kind = FieldGeometryKind::Convex;
          for (uint32_t i = 0; i < p.point_count; ++i)
            shape.points.push_back(map_->point(index, i));
          PodunkSolidShape s;
          if (!podunk_prepare_shape(shape, s, e))
            return false;
          solids.push_back(std::move(s));
        }
      }
      FieldGeometryFilter filter;
      filter.layer_mask = mask_;
      filter.reciprocal_layer = layer_;
      filter.bilateral_mask = true;
      filter.areas = false;
      std::vector<FieldGeometryContact> contacts;
      if (!space_->candidates(area, filter, 4096 - solids.size(), contacts, e))
        return false;
      for (const auto &contact : contacts) {
        FieldGeometryActor shape;
        FieldGeometryOwner owner;
        FieldGeometryShape source;
        if (!space_->live_geometry(contact, shape, owner, source, e))
          return false;
        FieldObjectId object;
        if (!host_.source_object(contact.stable_id, object, e))
          return false;
        auto t = registry_->tree_owner(object);
        auto state = t ? t->state(object) : nullptr;
        auto desc = t ? t->descriptor(object) : nullptr;
        if (!state || !state->inside || !state->alive || !state->bound ||
            !desc || desc->id != contact.stable_id)
          return fail(e, "Player solid actual collision owner not admitted");
        if (object == player)
          continue;
        if (owner.kind == 3 || (source.flags & 2) ||
            owner.constant_angular_velocity != 0 ||
            owner.constant_linear_velocity.x != 0 ||
            owner.constant_linear_velocity.y != 0)
          return fail(e,
                      "Player solid rigid/oneway/platform motion unsupported");
        PodunkSolidShape s;
        if (!podunk_prepare_shape(shape, s, e))
          return false;
        solids.push_back(std::move(s));
      }
      MotionQueryBounds visited;
      SlideResult candidate;
      if (!podunk_solve_motion(actor_shape, solids, position, velocity, delta_,
                               margin_, candidate, visited, e))
        return false;
      if (contains(area, visited)) {
        solved = candidate;
        closed = true;
        break;
      }
      area = grow(area, visited);
    }
    if (!closed)
      return fail(e, "Player solid collision query closure exceeded capacity");
  }
  auto state = tree_->state(player);
  auto local = state->local;
  if (state->canvas_parent) {
    FieldTransform parent;
    if (!tree_->world_transform(state->canvas_parent, parent, e) ||
        !inverse(geom(parent), solved.position, local[2]))
      return fail(e, "Player actual parent motion transform rejected");
  } else
    local[2] = solved.position;
  if (!tree_->set_local(player, local, e))
    return false;
  returned = solved.velocity;
  return true;
}
bool PodunkPlayerKinematic::ray_rotation(FieldObjectId ray, float rotation,
                                         std::string &e) {
  if (!live(e) || ray != ray_ || !std::isfinite(rotation))
    return fail(e, "Player native ray rotation rejected");
  auto local = tree_->state(ray)->local;
  float sx = std::hypot(local[0].x, local[0].y),
        sy = std::hypot(local[1].x, local[1].y);
  if (cross(local[0], local[1]) < 0)
    sy = -sy;
  local[0] = {std::cos(rotation) * sx, std::sin(rotation) * sx};
  local[1] = {-std::sin(rotation) * sy, std::cos(rotation) * sy};
  return tree_->set_local(ray, local, e);
}
bool PodunkPlayerKinematic::set_collision_mask(uint32_t bit, bool enabled,
                                               std::string &e) {
  if (!live(e) || bit >= 32)
    return fail(e, "Player source collision mask bit rejected");
  if (enabled)
    mask_ |= uint32_t(1) << bit;
  else
    mask_ &= ~(uint32_t(1) << bit);
  return true;
}
bool PodunkPlayerKinematic::set_collider_disabled(bool value, std::string &e) {
  if (!live(e))
    return false;
  disabled_ = value;
  return true;
}
bool PodunkPlayerKinematic::cached_ray(FieldObjectId ray, FieldObjectId &out,
                                       std::string &e) const {
  if (!live(e) || ray != ray_ || !ray_cached_)
    return fail(e, "Player actual native physics ray cache unavailable");
  out = ray_hit_;
  return true;
}
bool PodunkPlayerKinematic::ray_physics(FieldObjectId ray, uint64_t epoch,
                                        std::string &e) {
  if (!live(e) || ray != ray_ || epoch != epoch_ || !epoch ||
      epoch <= ray_epoch_)
    return fail(e, "Player RayCast actual physics-internal cursor rejected");
  FieldObjectId hit = 0;
  if (ray_enabled_) {
    FieldTransform world;
    if (!tree_->world_transform(ray, world, e))
      return false;
    Vec2 from = world[2], to = xf(geom(world), cast_);
    FieldGeometryFilter filter;
    filter.layer_mask = ray_mask_;
    filter.bodies = ray_bodies_;
    filter.areas = ray_areas_;
    if (exclude_parent_)
      filter.exclude_stable_id = tree_->descriptor(body_->object())->id;
    bool collided = false;
    FieldGeometryRayHit native_hit;
    if (!space_->ray(from, to, filter, 4096, collided, native_hit, e))
      return false;
    float best =
        collided ? native_hit.fraction : std::numeric_limits<float>::infinity();
    if (collided && !host_.source_object(native_hit.stable_id, hit, e))
      return false;
    if (exclude_parent_ && hit == body_->object())
      return fail(e, "Player native ray parent exclusion needs same-space "
                     "player owner filtering");
    if (ray_bodies_) {
      FieldMapRect area{{std::min(from.x, to.x), std::min(from.y, to.y)},
                        {std::max(from.x, to.x), std::max(from.y, to.y)}};
      std::vector<uint32_t> polygons;
      if (!map_->collision_polygons(area, ray_mask_, host_.map_gates, 4096,
                                    polygons, e))
        return false;
      auto direction = sub(to, from);
      for (auto index : polygons) {
        auto p = map_->polygon(index);
        for (uint32_t i = 0; i < p.point_count; ++i) {
          Vec2 a = map_->point(index, i),
               b = map_->point(index, p.kind == 2 ? (i ^ 1u)
                                                  : ((i + 1) % p.point_count));
          auto edge = sub(b, a);
          float den = cross(direction, edge);
          if (!den)
            continue;
          auto delta = sub(a, from);
          float fraction = cross(delta, edge) / den,
                u = cross(delta, direction) / den;
          if (fraction < 0 || fraction > 1 || u < 0 || u > 1 ||
              fraction >= best)
            continue;
          auto layer = map_->map(p.map);
          FieldObjectId object;
          if (!host_.source_object(layer.stable_id, object, e))
            return false;
          best = fraction;
          hit = object;
        }
      }
    }
    if (hit) {
      auto owner = registry_->tree_owner(hit);
      auto state = owner ? owner->state(hit) : nullptr;
      if (!state || !state->alive || !state->inside || !state->bound)
        return fail(e, "Player native ray cached hit actual owner unavailable");
    }
  }
  ray_hit_ = hit;
  ray_epoch_ = epoch;
  ray_cached_ = true;
  return true;
}
bool PodunkPlayerKinematic::rebind_tree(FieldNodeTreeRuntime &next, std::string &e) {
  if (!admitted() || !body_ || body_->tree() != &next || !registry_)
    return fail(e, "Player Kinematic rebind requires existing body/world owner");
  for (auto id : {body_->object(), collider_, ray_}) {
    auto *descriptor = next.descriptor(id);
    if (!descriptor || !player_rebind_node(*data_, *registry_, next, id, descriptor->id, e))
      return false;
  }
  // Preserve mask, velocity-independent solver state and the actual RayCast
  // cache until its next source physics-internal notification.
  tree_ = &next;
  e.clear();
  return true;
}
} // namespace encore::upstream
