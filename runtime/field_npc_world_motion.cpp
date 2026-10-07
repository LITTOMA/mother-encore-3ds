#include "encore/field_npc_world.hpp"
#include "encore/podunk_player_kinematic.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *m) {
  e = m;
  return false;
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
bool field_npc_world_move(const FieldNpcWorldBody &body, FieldObjectId player,
                          FieldObjectId collider, bool disabled, Vec2 velocity,
                          float delta, FieldNodeTreeRuntime &tree,
                          FieldGlobalRegistry &registry, FieldMapSpace &map,
                          FieldGeometrySpace &space,
                          const FieldMapGateQuery &gates, Vec2 &out_position,
                          Vec2 &returned, std::string &e) {
  if (!finite(velocity) || !std::isfinite(delta) || delta < 0 ||
      !space.source())
    return fail(e, "NPC native motion inputs");
  FieldTransform root_world, shape_world;
  if (!tree.world_transform(player, root_world, e) ||
      !tree.world_transform(collider, shape_world, e))
    return false;
  auto position = root_world[2];
  SlideResult solved;
  if (disabled) {
    solved = {add(position, mul(velocity, delta)), velocity};
    if (!finite(solved.position))
      return fail(e, "NPC disabled collider motion overflow");
  } else {
    FieldGeometryActor actor;
    FieldGeometryOwner actor_owner;
    FieldGeometryShape actor_source;
    FieldGeometryContact own;
    bool found = false;
    const auto *geometry = space.source();
    for (uint32_t i = 0; i < geometry->owner_count(); ++i) {
      auto owner = geometry->owner(i);
      if (geometry->node(owner.node).stable_id != body.id)
        continue;
      for (uint32_t j = 0; j < owner.shape_count; ++j) {
        auto sh = geometry->shape(owner.shape_first + j);
        if (geometry->node(sh.node).stable_id ==
            tree.descriptor(collider)->id) {
          if (sh.part_count != 1)
            return fail(e, "NPC native shape decomposition unsupported");
          own = {i, owner.shape_first + j, 0, body.id};
          found = true;
        }
      }
    }
    if (!found ||
        !space.live_geometry(own, actor, actor_owner, actor_source, e))
      return false;
    actor.transform = geom(shape_world);
    actor.transform.origin = sub(actor.transform.origin, position);
    PodunkSolidShape actor_shape;
    if (!podunk_prepare_shape(actor, actor_shape, e))
      return false;
    Vec2 travel = mul(velocity, delta);
    FieldGeometryBounds area{{position.x + actor_shape.minimum.x +
                                  std::min(0.0f, travel.x) - body.margin,
                              position.y + actor_shape.minimum.y +
                                  std::min(0.0f, travel.y) - body.margin},
                             {position.x + actor_shape.maximum.x +
                                  std::max(0.0f, travel.x) + body.margin,
                              position.y + actor_shape.maximum.y +
                                  std::max(0.0f, travel.y) + body.margin}};
    bool closed = false;
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
      std::vector<PodunkSolidShape> solids;
      std::vector<uint32_t> polygons;
      FieldMapRect query{area.minimum, area.maximum};
      if (!map.collision_polygons(query, body.mask, gates, 4096, polygons, e))
        return false;
      for (auto index : polygons) {
        auto p = map.polygon(index);
        if (p.kind == 2) {
          FieldMapRect local;
          auto tr = map.shape_transform(index);
          FieldGeometryTransform transform{tr.x, tr.y, tr.origin};
          Vec2 first;
          bool initial = true;
          for (float x : {query.minimum.x, query.maximum.x})
            for (float y : {query.minimum.y, query.maximum.y}) {
              if (!inverse(transform, {x, y}, first))
                return fail(e, "NPC map segment query transform rejected");
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
          if (!map.source()->concave_segments(index, local,
                                              4096 - solids.size(), pairs, e))
            return false;
          auto g = map.source()->local_geometry(p.geometry);
          for (auto pair : pairs) {
            FieldGeometryActor shape;
            shape.kind = FieldGeometryKind::Segment;
            shape.transform = transform;
            shape.points = {
                map.source()->local_point(g.point_first + pair * 2),
                map.source()->local_point(g.point_first + pair * 2 + 1)};
            PodunkSolidShape s;
            if (!podunk_prepare_shape(shape, s, e))
              return false;
            solids.push_back(std::move(s));
          }
        } else {
          FieldGeometryActor shape;
          shape.kind = FieldGeometryKind::Convex;
          for (uint32_t i = 0; i < p.point_count; ++i)
            shape.points.push_back(map.point(index, i));
          PodunkSolidShape s;
          if (!podunk_prepare_shape(shape, s, e))
            return false;
          solids.push_back(std::move(s));
        }
      }
      FieldGeometryFilter filter;
      filter.layer_mask = body.mask;
      filter.reciprocal_layer = body.layer;
      filter.bilateral_mask = true;
      filter.areas = false;
      std::vector<FieldGeometryContact> contacts;
      if (!space.candidates(area, filter, 4096 - solids.size(), contacts, e))
        return false;
      for (const auto &contact : contacts) {
        FieldGeometryActor shape;
        FieldGeometryOwner owner;
        FieldGeometryShape source;
        if (!space.live_geometry(contact, shape, owner, source, e))
          return false;
        FieldObjectId object;
        if (!(object = contact.actual_owner
                           ? contact.actual_owner
                           : tree.source_object(contact.stable_id)))
          return false;
        auto t = registry.tree_owner(object);
        auto state = t ? t->state(object) : nullptr;
        auto desc = t ? t->descriptor(object) : nullptr;
        if (!state || !state->inside || !state->alive || !state->bound ||
            !desc || desc->id != contact.stable_id)
          return fail(e, "NPC solid actual collision owner not admitted");
        if (object == player)
          continue;
        if (owner.kind == 3 || (source.flags & 2) ||
            owner.constant_angular_velocity != 0 ||
            owner.constant_linear_velocity.x != 0 ||
            owner.constant_linear_velocity.y != 0)
          return fail(e, "NPC solid rigid/oneway/platform motion unsupported");
        PodunkSolidShape s;
        if (!podunk_prepare_shape(shape, s, e))
          return false;
        solids.push_back(std::move(s));
      }
      MotionQueryBounds visited;
      SlideResult candidate;
      if (!podunk_solve_motion(actor_shape, solids, position, velocity, delta,
                               body.margin, candidate, visited, e))
        return false;
      if (contains(area, visited)) {
        solved = candidate;
        closed = true;
        break;
      }
      area = grow(area, visited);
    }
    if (!closed)
      return fail(e, "NPC solid collision query closure exceeded capacity");
  }
  auto state = tree.state(player);
  auto local = state->local;
  if (state->canvas_parent) {
    FieldTransform parent;
    if (!tree.world_transform(state->canvas_parent, parent, e) ||
        !inverse(geom(parent), solved.position, local[2]))
      return fail(e, "NPC actual parent motion transform rejected");
  } else
    local[2] = solved.position;
  if (!tree.set_local(player, local, e))
    return false;
  out_position = solved.position;
  returned = solved.velocity;
  return true;
}
bool field_npc_world_ray(const FieldNpcWorldRay &desc, FieldObjectId ray,
                         Vec2 cast, FieldNodeTreeRuntime &tree,
                         FieldGlobalRegistry &registry, FieldMapSpace &map,
                         FieldGeometrySpace &space,
                         const FieldMapGateQuery &gates, FieldObjectId &out,
                         std::string &e) {
  auto state = tree.state(ray);
  if (!state || !state->inside || !state->bound || !finite(cast))
    return fail(e, "NPC native ray lifecycle/cast unavailable");
  auto parent = state->parent;
  FieldObjectId hit = 0;
  if (desc.enabled) {
    FieldTransform world;
    if (!tree.world_transform(ray, world, e))
      return false;
    Vec2 from = world[2], to = xf(geom(world), cast);
    FieldGeometryFilter filter;
    filter.layer_mask = desc.mask;
    filter.bodies = desc.bodies;
    filter.areas = desc.areas;
    if (desc.exclude)
      filter.exclude_stable_id = desc.parent;
    bool collided = false;
    FieldGeometryRayHit native_hit;
    if (!space.ray(from, to, filter, 4096, collided, native_hit, e))
      return false;
    float best =
        collided ? native_hit.fraction : std::numeric_limits<float>::infinity();
    if (collided && !(hit = native_hit.actual_owner
                                ? native_hit.actual_owner
                                : tree.source_object(native_hit.stable_id)))
      return false;
    if (desc.exclude && hit == parent)
      return fail(e, "NPC native ray parent exclusion needs same-space "
                     "player owner filtering");
    if (desc.bodies) {
      FieldMapRect area{{std::min(from.x, to.x), std::min(from.y, to.y)},
                        {std::max(from.x, to.x), std::max(from.y, to.y)}};
      std::vector<uint32_t> polygons;
      if (!map.collision_polygons(area, desc.mask, gates, 4096, polygons, e))
        return false;
      auto direction = sub(to, from);
      for (auto index : polygons) {
        auto p = map.polygon(index);
        for (uint32_t i = 0; i < p.point_count; ++i) {
          Vec2 a = map.point(index, i),
               b = map.point(index, p.kind == 2 ? (i ^ 1u)
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
          auto layer = map.map(p.map);
          FieldObjectId object;
          if (!(object = tree.source_object(layer.stable_id)))
            return false;
          best = fraction;
          hit = object;
        }
      }
    }
    if (hit) {
      auto owner = registry.tree_owner(hit);
      auto state = owner ? owner->state(hit) : nullptr;
      if (!state || !state->alive || !state->inside || !state->bound)
        return fail(e, "NPC native ray cached hit actual owner unavailable");
    }
  }
  out = hit;
  return true;
}
} // namespace encore::upstream
