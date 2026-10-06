#include "podunk_scene_native.hpp"
#include "field_canvas_art_renderer.hpp"
#include "podunk_player_effect_owners.hpp"
// This existing primitive's compact statements predate this owner. Keep its
// diagnostics local rather than changing the frozen renderer's source bytes.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "field_map_renderer.hpp"
#pragma GCC diagnostic pop
#include <fstream>
#include <limits>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const std::string &s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool translation(const FieldTransform &t) {
  return t[0].x == 1 && t[0].y == 0 && t[1].x == 0 && t[1].y == 1;
}
} // namespace
// A closed, concrete ownership adapter for the existing two renderers. It is
// not an arbitrary callback that approves a foreign identity or empty draw.
class PodunkPlayerCanvasForeign final : public FieldCanvasForeignOwner {
public:
  PodunkPlayerCanvasForeign(PodunkPlayerHost &player,
                            const PlayerInitializationData &initial,
                            PodunkConcretePlayerEffectOwners &effects,
                            const PlayerEffectsData &effect_data,
                            FieldGlobalRegistry &registry,
                            FieldNodeTreeRuntime &tree)
      : player_(player), initial_(initial), effects_(effects),
        effect_data_(effect_data), registry_(registry), tree_(tree) {}
  bool sources(std::string &e) const {
    if (!initial_.valid() || !effect_data_.valid() ||
        effect_data_.player_ir_sha256() != initial_.ir_sha256() ||
        player_.registry() != &registry_ || player_.tree() != &tree_ ||
        player_.body().data() != &initial_ ||
        effects_.data() != &effect_data_ || effects_.registry() != &registry_)
      return fail(e,
                  "Canvas foreign actual source bodies/IR/Registry mismatch");
    return true;
  }
  bool admit(FieldObjectId id, const FieldNodeDescriptor &d,
             const FieldIdentity &identity, const FieldNodeTreeRuntime &tree,
             bool &drawable, std::string &e) const override {
    if (!sources(e) || &tree != &tree_ || !registry_.object_exists(id))
      return false;
    auto actual_owner = registry_.tree_owner(id);
    const auto *s = tree_.state(id);
    FieldIdentity actual_identity{};
    if (!actual_owner || actual_owner.get() != &tree_ || !s || !s->alive ||
        !s->inside || !s->bound ||
        !tree_.object_identity(id, actual_identity) ||
        !same(identity, actual_identity) ||
        !same(s->binding.identity, identity) || s->binding.stable_id != d.id ||
        s->binding.class_index != d.class_index ||
        s->binding.native_class != d.native_class ||
        s->binding.script_sha != d.script_sha || !s->binding.family ||
        !s->binding.capability)
      return fail(e, "Canvas foreign actual live source binding rejected");
    const FieldNodeRecipeData *recipe = nullptr;
    if (same(identity, initial_.recipe().identity())) {
      if (!player_.owns(id) || !player_.ready_complete() ||
          s->binding.family != 0x454e0056 || s->binding.capability != 1)
        return fail(e, "Canvas foreign Player actual owning Ready incomplete");
      recipe = &initial_.recipe();
    } else {
      if (!effects_.accepts(identity) || !effects_.owns(id) ||
          s->binding.family != 0x454e0059 || s->binding.capability != 1)
        return fail(e, "Canvas foreign effect actual owning instance absent");
      for (const auto &creator : effect_data_.creators()) {
        const auto *candidate = effect_data_.recipe(creator.kind);
        if (candidate && same(candidate->identity(), identity)) {
          recipe = candidate;
          break;
        }
      }
    }
    const auto *source = recipe ? recipe->record(d.id) : nullptr;
    if (!source || source->native_generated || source->path != d.path ||
        source->native_class != d.native_class || source->script != d.script ||
        source->script_sha != d.script_sha ||
        source->class_index != d.class_index)
      return fail(e, "Canvas foreign descriptor differs from actual checked "
                     "source recipe");
    drawable = d.native_class == "Sprite" || d.native_class == "AnimatedSprite";
    e.clear();
    return true;
  }
  bool draw(const FieldCanvasOrderSlot &slot, const FieldTransform &viewport,
            bool snap, std::string &e) {
    const auto *d = tree_.descriptor(slot.object);
    bool drawable = false;
    if (!d || !slot.foreign ||
        !admit(slot.object, *d, slot.identity, tree_, drawable, e) ||
        !drawable || !slot.foreign_drawable)
      return fail(e, "Canvas foreign GPU slot source ownership rejected");
    if (same(slot.identity, initial_.recipe().identity()))
      return player_.draw(slot.object, viewport, snap, e);
    if (!translation(viewport))
      return fail(e, "Canvas foreign effect viewport affine unsupported");
    return effects_.draw(slot.object, Vec2{-viewport[2].x, -viewport[2].y}, e);
  }

private:
  PodunkPlayerHost &player_;
  const PlayerInitializationData &initial_;
  PodunkConcretePlayerEffectOwners &effects_;
  const PlayerEffectsData &effect_data_;
  FieldGlobalRegistry &registry_;
  FieldNodeTreeRuntime &tree_;
};
PodunkSceneNative::PodunkSceneNative() = default;
PodunkSceneNative::~PodunkSceneNative() {
  if (map_gpu_)
    map_gpu_->free();
}
bool PodunkSceneNative::prepare(
    const FieldNodeTreeData &d, FieldNodeTreeRuntime &t, FieldGlobalRegistry &r,
    PodunkNativeRoot &root, FieldMapSpace &map, FieldGeometrySpace &geometry,
    const FieldCanvasArtData &art, const char *prefix, FieldCanvasArtHost host,
    PodunkSceneMaterialOwner *materials, std::string &e) {
  const auto *m = map.source();
  const auto *g = geometry.source();
  if (data_ || !d.valid() || !m || !g || !art.valid() || !prefix ||
      !same(d.identity(), m->identity()) ||
      !same(d.identity(), g->identity()) ||
      !same(d.identity(), art.identity()) ||
      d.source_scene() != m->source_scene() ||
      d.source_scene() != g->source_scene() ||
      d.source_scene() != art.source_scene() ||
      (materials && materials->registry() != &r))
    return fail(e, "Scene native source/owner identity mismatch");
  // Verify the actual converted map bytes before importing GPU textures.
  for (uint32_t i = 0; i < m->texture_count(); ++i) {
    const auto tex = m->texture(i);
    std::ifstream f(std::string(prefix) + std::string(m->string(tex.path)),
                    std::ios::binary);
    if (!f)
      return fail(e, "Scene native map texture unavailable");
    f.seekg(0, std::ios::end);
    auto n = f.tellg();
    if (n <= 0 || n > 16 * 1024 * 1024)
      return fail(e, "Scene native map texture extent rejected");
    std::vector<uint8_t> bytes(static_cast<size_t>(n));
    f.seekg(0);
    if (!f.read(reinterpret_cast<char *>(bytes.data()), n) ||
        field_canvas_art_renderer_detail::sha256(bytes.data(), bytes.size()) !=
            tex.output_sha256)
      return fail(e, "Scene native map texture source receipt rejected");
  }
  auto mg = std::make_unique<FieldMapRenderer>();
  auto ag = std::make_unique<FieldCanvasArtRenderer>();
  if (!mg->load(*m, prefix, e) || !ag->load(art, prefix, e)) {
    mg->free();
    return false;
  }
  std::map<uint32_t, FieldMapAnimationState> clocks;
  for (uint32_t i = 0; i < m->texture_count(); ++i) {
    const auto tex = m->texture(i);
    if (tex.frames > 1 && !clocks.count(tex.group)) {
      FieldMapAnimationState a;
      if (!m->start_animation(i, a, e)) {
        mg->free();
        return false;
      }
      clocks.emplace(tex.group, a);
    }
  }
  // Pack shape state remains source-native. This owner does not query the
  // preindexed space or pretend script ancestors are admitted during factory
  // construction. The real physics owner must check physics_admitted().
  data_ = &d;
  tree_ = &t;
  registry_ = &r;
  root_ = &root;
  map_ = &map;
  geometry_ = &geometry;
  art_ = &art;
  art_host_ = std::move(host);
  materials_ = materials;
  map_gpu_ = std::move(mg);
  art_gpu_ = std::move(ag);
  animations_ = std::move(clocks);
  e.clear();
  return true;
}
bool PodunkSceneNative::owns(const FieldNodeDescriptor &d) const {
  const auto &c = d.native_class;
  return c == "Node" || c == "Node2D" || c == "Position2D" || c == "YSort" ||
         c == "TileMap" || c == "StaticBody2D" || c == "Area2D" ||
         c == "CollisionShape2D" || c == "CollisionPolygon2D" || c == "Sprite";
}
bool PodunkSceneNative::owns(FieldObjectId id) const {
  return instances_.count(id) != 0;
}
bool PodunkSceneNative::actual(FieldObjectId id, const FieldNodeDescriptor *&d,
                               const FieldNodeState *&s, std::string &e) const {
  FieldIdentity identity;
  auto owner = registry_ ? registry_->tree_owner(id) : nullptr;
  d = tree_ ? tree_->descriptor(id) : nullptr;
  s = tree_ ? tree_->state(id) : nullptr;
  if (!data_ || !owner || owner.get() != tree_ ||
      !registry_->object_exists(id) || !d || !s || !s->alive ||
      !tree_->object_identity(id, identity) ||
      !same(identity, data_->identity()))
    return fail(e, "Scene native actual ObjectDB/tree identity rejected");
  const auto *source = data_->record(d->id);
  if (!source || source->path != d->path ||
      source->class_index != d->class_index ||
      source->script_sha != d->script_sha || source->script != d->script ||
      data_->classes().at(source->class_index) != d->native_class)
    return fail(e, "Scene native source descriptor binding rejected");
  return true;
}
bool PodunkSceneNative::material(FieldObjectId id, const FieldCanvasRecord &a,
                                 std::string &e) const {
  if (a.shader == FieldCanvasShader::Default)
    return true;
  PodunkSceneMaterialState state;
  if (!materials_ || !materials_->material(id, a, state, e))
    return fail(e, "Scene native material owner unavailable: " + a.node);
  const auto *resource = registry_->source_resource(state.material);
  std::array<uint8_t, 32> sha{};
  if (!resource ||
      std::string_view(resource->resource_class()) != "ShaderMaterial" ||
      !art_->source_hash(a.shader_source.substr(0, a.shader_source.find("::")),
                         sha) ||
      state.shader_source != a.shader_source || state.shader_source_sha != sha)
    return fail(e, "Scene native actual source ShaderMaterial rejected: " +
                       a.node);
  const auto b = resource->binding();
  std::array<uint8_t, 32> material_sha{};
  if (b.object != state.material ||
      !art_->source_hash(b.source.source.substr(0, b.source.source.find("::")),
                         material_sha) ||
      b.source.source_sha != material_sha ||
      b.source.identity.upstream_commit != data_->identity().upstream_commit)
    return fail(e,
                "Scene native ShaderMaterial source proof mismatch: " + a.node);
  return true;
}
bool PodunkSceneNative::construct(FieldObjectId id,
                                  const FieldNodeDescriptor &d,
                                  const FieldIdentity &identity,
                                  std::string &e) {
  const FieldNodeDescriptor *actual_d;
  const FieldNodeState *s;
  if (!same(identity, data_ ? data_->identity() : FieldIdentity{}) ||
      !owns(d) || instances_.count(id) || source_objects_.count(d.id) ||
      !actual(id, actual_d, s, e) || actual_d->id != d.id || s->inside)
    return fail(e, "Scene native constructor cursor/identity rejected");
  Instance n;
  n.source = d.id;
  if (d.native_class == "Node")
    n.kind = Kind::Node;
  else if (d.native_class == "TileMap")
    n.kind = Kind::Map;
  else if (d.native_class == "StaticBody2D")
    n.kind = Kind::Body;
  else if (d.native_class == "Area2D")
    n.kind = Kind::Area;
  else if (d.native_class == "CollisionShape2D" ||
           d.native_class == "CollisionPolygon2D")
    n.kind = Kind::Shape;
  else if (d.native_class == "Sprite")
    n.kind = Kind::Sprite;
  else
    n.kind = Kind::Canvas;
  auto *g = geometry_->source();
  for (uint32_t i = 0; i < g->node_count(); ++i)
    if (g->node(i).stable_id == d.id) {
      n.geometry_node = i;
      break;
    }
  if (n.kind == Kind::Body || n.kind == Kind::Area) {
    for (uint32_t i = 0; i < g->owner_count(); ++i)
      if (g->owner(i).node == n.geometry_node) {
        n.owner = i;
        break;
      }
    if (n.owner == UINT32_MAX)
      return fail(e, "Scene native collision owner missing: " + d.path);
    const auto o = g->owner(n.owner);
    if ((n.kind == Kind::Body &&
         (o.kind != 1 || o.constant_linear_velocity.x ||
          o.constant_linear_velocity.y || o.constant_angular_velocity)) ||
        (n.kind == Kind::Area &&
         (o.kind != 4 || o.space_override || (o.flags & 8))))
      return fail(e, "Scene native collision owner behavior unsupported: " +
                         d.path);
  }
  if (n.kind == Kind::Shape) {
    for (uint32_t i = 0; i < g->shape_count(); ++i)
      if (g->shape(i).node == n.geometry_node) {
        n.shape = i;
        break;
      }
    if (n.shape == UINT32_MAX)
      return fail(e, "Scene native source shape missing: " + d.path);
    const auto shape = g->shape(n.shape);
    if (shape.flags & 2)
      return fail(e, "Scene native one-way collision adapter unavailable: " +
                         d.path);
    n.disabled = bool(shape.flags & 1);
  }
  if (n.kind == Kind::Map) {
    for (uint32_t i = 0; i < map_->source()->map_count(); ++i)
      if (map_->source()->map(i).stable_id == d.id) {
        n.map = i;
        break;
      }
    if (n.map == UINT32_MAX || !translation(d.world))
      return fail(e,
                  "Scene native TileMap source transform missing: " + d.path);
  }
  if (n.kind == Kind::Sprite) {
    const auto *a = art_->record(d.id);
    if (!a || a->kind != 0 || !material(id, *a, e))
      return fail(e, "Scene native Sprite source/material rejected: " + d.path +
                         ": " + e);
  }
  instances_.emplace(id, n);
  source_objects_.emplace(d.id, id);
  e.clear();
  return true;
}
bool PodunkSceneNative::bind(FieldObjectId id, const FieldNodeBinding &b,
                             std::string &e) {
  const FieldNodeDescriptor *d;
  const FieldNodeState *s;
  auto it = instances_.find(id);
  if (it == instances_.end() || !actual(id, d, s, e) ||
      !same(b.identity, data_->identity()) || b.stable_id != d->id ||
      b.class_index != d->class_index || b.native_class != d->native_class ||
      b.script_sha != d->script_sha || !b.family || !b.capability)
    return fail(e, "Scene native combined typed binding rejected");
  it->second.bound = true;
  e.clear();
  return true;
}
bool PodunkSceneNative::finish_factory(std::string &e) {
  if (!data_ || finished_ || !tree_->root())
    return fail(e, "Scene native factory state rejected");
  for (const auto &row : data_->records()) {
    FieldNodeDescriptor d = row;
    d.native_class = data_->classes().at(row.class_index);
    if (owns(d) && !source_objects_.count(d.id))
      return fail(e, "Scene native factory omitted source node: " + d.path);
  }
  if (!canvas_.initialize(*art_, *data_, *tree_, art_host_, e))
    return false;
  finished_ = true;
  e.clear();
  return true;
}
bool PodunkSceneNative::bind_foreign(PodunkPlayerHost &player,
                                     const PlayerInitializationData &initial,
                                     PodunkConcretePlayerEffectOwners &effects,
                                     const PlayerEffectsData &data,
                                     std::string &e) {
  if (!finished_ || foreign_)
    return fail(e, "Scene native foreign owner binding cursor rejected");
  auto owner = std::make_unique<PodunkPlayerCanvasForeign>(
      player, initial, effects, data, *registry_, *tree_);
  if (!owner->sources(e) || !canvas_.bind_foreign(*owner, e))
    return false;
  foreign_ = std::move(owner);
  e.clear();
  return true;
}
bool PodunkSceneNative::synchronize(FieldObjectId id, Instance &n,
                                    std::string &e) {
  const FieldNodeDescriptor *d;
  const FieldNodeState *s;
  if (!actual(id, d, s, e))
    return false;
  if (n.geometry_node != UINT32_MAX && geometry_bound_) {
    FieldGeometryNodeUpdate u;
    u.stable_id = n.source;
    if (!n.synchronized || s->local[0].x != n.space_local[0].x ||
        s->local[0].y != n.space_local[0].y ||
        s->local[1].x != n.space_local[1].x ||
        s->local[1].y != n.space_local[1].y ||
        s->local[2].x != n.space_local[2].x ||
        s->local[2].y != n.space_local[2].y)
      u.fields = 1;
    u.local = {s->local[0], s->local[1], s->local[2]};
    if (n.kind == Kind::Shape &&
        (!n.synchronized || n.space_disabled != (n.disabled || !n.entered))) {
      u.fields |= 4;
      u.disabled = n.disabled || !n.entered;
    }
    if (u.fields && !geometry_->apply_updates({u}, e))
      return false;
    n.space_local = s->local;
    n.space_disabled = n.disabled || !n.entered;
    n.synchronized = true;
  }
  for (uint32_t i = 0; i < map_->source()->canvas_count(); ++i)
    if (map_->source()->canvas(i).stable_id == n.source) {
      if (!map_->set_canvas_visibility(n.source,
                                       n.entered && bool(s->flags & 2), e))
        return false;
      break;
    }
  if (n.kind == Kind::Map) {
    FieldTransform world;
    const auto c = map_->canvas(map_->map(n.map).canvas);
    if (!tree_->world_transform(id, world, e) || !translation(world) ||
        world[2].x != c.position.x || world[2].y != c.position.y)
      return fail(e, "Scene native TileMap live transform not synchronized "
                     "with actual MapSpace: " +
                         d->path);
  }
  e.clear();
  return true;
}
bool PodunkSceneNative::phase(FieldObjectId id, FieldTreePhase p,
                              std::string &e) {
  auto it = instances_.find(id);
  const FieldNodeDescriptor *d;
  const FieldNodeState *s;
  if (it == instances_.end() || !it->second.bound || !actual(id, d, s, e))
    return fail(e, "Scene native phase actual owner unavailable");
  auto &n = it->second;
  if (p == FieldTreePhase::EnterNative) {
    if (n.entered || !s->inside || !root_->viewport().world_registered)
      return fail(e, "Scene native Enter world/parent cursor rejected");
    n.entered = true;
    if (!synchronize(id, n, e))
      return false;
  } else if (p == FieldTreePhase::ReadyNative) {
    if (!n.entered || !s->inside || !s->bound)
      return fail(e, "Scene native Ready without actual Enter/bind");
    if (n.kind == Kind::Sprite && !material(id, *art_->record(n.source), e))
      return false;
    if (!synchronize(id, n, e))
      return false;
    n.ready = true;
  } else if (p == FieldTreePhase::ExitNative) {
    if (!n.entered)
      return fail(e, "Scene native Exit without Enter");
    if (n.monitored && (!world_ || !world_->static_monitor_exit(id, e)))
      return false;
    n.monitored = false;
    n.entered = false;
    if (!synchronize(id, n, e))
      return false;
    monitors_active_ = false;
  } else if (p == FieldTreePhase::TransformChanged ||
             p == FieldTreePhase::LocalTransformChanged ||
             p == FieldTreePhase::VisibilityChanged ||
             p == FieldTreePhase::Hide) {
    if (!synchronize(id, n, e))
      return false;
  } else if (p == FieldTreePhase::Deleting) {
    if (n.entered || n.monitored)
      return fail(e, "Scene native deletion before physical Exit");
    if (n.geometry_node != UINT32_MAX) {
      FieldGeometryNodeUpdate u;
      u.stable_id = n.source;
      u.fields = 2;
      u.deleted = true;
      if (!geometry_->apply_updates({u}, e))
        return false;
    }
    for (uint32_t i = 0; i < map_->source()->canvas_count(); ++i)
      if (map_->source()->canvas(i).stable_id == n.source &&
          !map_->commit_deleted(n.source, e))
        return false;
    source_objects_.erase(n.source);
    instances_.erase(it);
  } else if (p == FieldTreePhase::Physics || p == FieldTreePhase::Idle ||
             p == FieldTreePhase::PhysicsInternal ||
             p == FieldTreePhase::IdleInternal || p == FieldTreePhase::Input ||
             p == FieldTreePhase::UnhandledInput ||
             p == FieldTreePhase::UnhandledKeyInput)
    return fail(
        e,
        "Scene native script/internal process must use actual typed owner: " +
            d->path);
  // The remaining base Node/Canvas notifications have no subclass body here;
  // their actual parent/path/order/visibility state is maintained by Tree.
  e.clear();
  return true;
}
bool PodunkSceneNative::set_disabled(FieldObjectId id, bool value,
                                     std::string &e) {
  auto it = instances_.find(id);
  if (it == instances_.end() || it->second.kind != Kind::Shape)
    return fail(e, "Scene native disabled target is not actual shape");
  if (!geometry_bound_)
    return fail(
        e,
        "Scene native dynamic disabled setter before source ancestors Ready");
  FieldGeometryNodeUpdate u;
  u.stable_id = it->second.source;
  u.fields = 4;
  u.disabled = value || !it->second.entered;
  if (!geometry_->apply_updates({u}, e))
    return false;
  it->second.disabled = value;
  it->second.space_disabled = u.disabled;
  return true;
}
bool PodunkSceneNative::set_collision(FieldObjectId id, uint32_t layer,
                                      uint32_t mask, std::string &e) {
  auto it = instances_.find(id);
  if (it == instances_.end() || it->second.owner == UINT32_MAX)
    return fail(e, "Scene native masks target is not actual body/Area");
  if (!geometry_bound_)
    return fail(e, "Scene native masks setter before source ancestors Ready");
  FieldGeometryNodeUpdate u;
  u.stable_id = it->second.source;
  u.fields = 8;
  u.layer = layer;
  u.mask = mask;
  return geometry_->apply_updates({u}, e);
}
bool PodunkSceneNative::activate_monitors(FieldSceneHost &scene,
                                          PodunkPlayerPhysicsWorld &world,
                                          std::string &e) {
  if (!finished_ || !scene.scene_ready() || world.registry() != registry_ ||
      world.tree() != tree_)
    return fail(
        e, "Scene native monitors require full actual source Ready/same world");
  world_ = &world;
  geometry_bound_ = true;
  monitors_active_ = true;
  std::vector<FieldGeometryNodeUpdate> updates;
  for (auto &row : instances_) {
    auto &n = row.second;
    if (n.geometry_node == UINT32_MAX)
      continue;
    const FieldNodeDescriptor *d;
    const FieldNodeState *s;
    if (!actual(row.first, d, s, e))
      return false;
    FieldGeometryNodeUpdate u;
    u.stable_id = n.source;
    u.fields = 1;
    u.local = {s->local[0], s->local[1], s->local[2]};
    if (n.kind == Kind::Shape) {
      u.fields |= 4;
      u.disabled = n.disabled || !n.entered;
    }
    updates.push_back(u);
  }
  if (!geometry_->apply_updates(updates, e)) {
    monitors_active_ = false;
    return false;
  }
  for (auto &row : instances_) {
    auto &n = row.second;
    if (n.geometry_node == UINT32_MAX)
      continue;
    n.space_local = tree_->state(row.first)->local;
    n.space_disabled = n.disabled || !n.entered;
    n.synchronized = true;
  }
  for (auto &row : instances_)
    if (!synchronize(row.first, row.second, e)) {
      monitors_active_ = false;
      return false;
    }
  for (auto &row : instances_) {
    auto &n = row.second;
    if (n.kind == Kind::Area && n.entered && n.ready && !n.monitored) {
      if (!world.admit_static_monitor(row.first, n.owner, e))
        return false;
      n.monitored = true;
    }
  }
  monitors_active_ = true;
  return physics_admitted(e);
}
bool PodunkSceneNative::physics_admitted(std::string &e) const {
  if (!finished_ || !monitors_active_ || !root_->viewport().world_registered)
    return fail(e, "Scene native physical activation incomplete");
  for (uint32_t i = 0; i < map_->source()->map_count(); ++i) {
    const auto stable = map_->source()->map(i).stable_id;
    auto q = source_objects_.find(stable);
    if (q == source_objects_.end())
      continue; // Committed source deletion only.
    const auto &n = instances_.at(q->second);
    if (!n.entered || !n.ready)
      return fail(e, "Scene native TileMap has not actually entered/Ready");
  }
  for (const auto &q : instances_) {
    const auto &n = q.second;
    if ((n.kind == Kind::Shape || n.kind == Kind::Body ||
         n.kind == Kind::Area) &&
        (!n.entered || !n.ready || (n.kind == Kind::Area && !n.monitored)))
      return fail(e, "Scene native physical source owner incomplete");
  }
  e.clear();
  return true;
}
bool PodunkSceneNative::begin_draw(uint64_t epoch, float delta,
                                   std::string &e) {
  if (!finished_ || !epoch || epoch <= draw_epoch_ || !std::isfinite(delta) ||
      delta < 0 || !root_->viewport().active)
    return fail(e, "Scene native actual draw epoch rejected");
  auto next = animations_;
  for (auto &v : next)
    if (!map_->source()->advance_animation(v.second, delta, e))
      return false;
  animations_ = std::move(next);
  draw_epoch_ = epoch;
  draw_started_ = true;
  art_gpu_->begin_frame();
  e.clear();
  return true;
}
bool PodunkSceneNative::draw(const FieldMapGateQuery &gates, std::string &e) {
  if (!draw_started_)
    return fail(e, "Scene native draw has no actual GPU frame boundary");
  FieldMapRect rect;
  if (!root_->world_rect(rect, e))
    return false;
  std::vector<FieldCanvasDraw> art;
  if (!canvas_.collect(art, e))
    return false;
  struct Command {
    int64_t z;
    uint64_t native_order;
    uint32_t tile_order;
    bool tile;
    uint32_t index;
    FieldColor color;
    bool foreign = false;
  };
  std::vector<Command> commands;
  std::map<FieldObjectId, FieldCanvasOrderSlot> slots;
  std::vector<FieldCanvasOrderSlot> foreign_slots;
  for (const auto &s : canvas_.canvas_order()) {
    slots.emplace(s.object, s);
    if (s.foreign && s.foreign_drawable) {
      commands.push_back({s.z,
                          s.native_order,
                          0,
                          false,
                          uint32_t(foreign_slots.size()),
                          {},
                          true});
      foreign_slots.push_back(s);
    }
  }
  for (uint32_t i = 0; i < art.size(); ++i) {
    const auto s = slots.find(art[i].object);
    if (s == slots.end() || !owns(art[i].object))
      return fail(e, "Scene native Sprite command has no actual native owner");
    commands.push_back({art[i].z, s->second.native_order, 0, false, i, {}});
  }
  std::vector<uint32_t> tiles;
  std::set<FieldObjectId> synchronized_maps;
  if (!map_->collect_draws(rect, gates, map_->source()->draw_count(), tiles, e))
    return false;
  for (auto index : tiles) {
    const auto pose = map_->draw(index);
    const auto layer = map_->map(pose.map);
    FieldObjectId id;
    if (!source_object(layer.stable_id, id, e))
      return false;
    auto n = instances_.find(id);
    auto slot = slots.find(id);
    if (n == instances_.end() || !n->second.entered || !n->second.ready)
      return fail(e, "Scene native TileMap draw before native Ready");
    if (slot == slots.end())
      continue; // Actual canvas ancestor visibility.
    if (synchronized_maps.insert(id).second && !synchronize(id, n->second, e))
      return false;
    commands.push_back({int64_t(slot->second.z) + pose.z,
                        slot->second.native_order, pose.order, true, index,
                        slot->second.color});
  }
  std::stable_sort(commands.begin(), commands.end(),
                   [](const Command &a, const Command &b) {
                     return std::tie(a.z, a.native_order, a.tile_order) <
                            std::tie(b.z, b.native_order, b.tile_order);
                   });
  const auto &viewport = root_->viewport();
  if (!translation(viewport.canvas))
    return fail(e,
                "Scene native compositor needs actual translated 1:1 viewport");
  const Vec2 camera{-viewport.canvas[2].x, -viewport.canvas[2].y};
  FieldCanvasArtRenderer::Delegate delegate;
  if (materials_)
    delegate = [this](const FieldCanvasDraw &a, Vec2 c, float w, float h,
                      std::string &error) {
      return materials_->draw(a, c, w, h, error);
    };
  for (const auto &c : commands) {
    if (c.foreign) {
      if (!foreign_ || !foreign_->draw(foreign_slots.at(c.index),
                                       viewport.canvas, art_->pixel_snap(), e))
        return false;
    } else if (c.tile) {
      auto pose = map_->draw(c.index);
      auto texture = pose.texture;
      const auto tex = map_->source()->texture(texture);
      if (tex.frames > 1)
        texture = tex.group + animations_.at(tex.group).frame;
      for (size_t i = 0; i < 4; ++i)
        pose.color[i] *= c.color[i];
      if (!map_gpu_->draw(pose, texture, bool(pose.flags & 4), camera.x,
                          camera.y))
        return fail(e, "Scene native actual GPU tile draw rejected");
    } else {
      auto a = art[c.index];
      if (c.z < std::numeric_limits<int32_t>::min() ||
          c.z > std::numeric_limits<int32_t>::max())
        return fail(e, "Scene native canvas z overflow");
      a.z = int32_t(c.z);
      if (!art_gpu_->draw({a}, camera, viewport.size.x, viewport.size.y,
                          delegate, e))
        return false;
    }
  }
  draw_started_ = false;
  e.clear();
  return true;
}
bool PodunkSceneNative::source_object(uint32_t id, FieldObjectId &out,
                                      std::string &e) const {
  auto q = source_objects_.find(id);
  const FieldNodeDescriptor *d;
  const FieldNodeState *s;
  if (q == source_objects_.end() || !actual(q->second, d, s, e))
    return fail(e, "Scene native actual source object unavailable");
  out = q->second;
  e.clear();
  return true;
}
bool PodunkSceneNative::shutdown(std::string &e) {
  for (const auto &q : instances_)
    if (q.second.entered || q.second.monitored)
      return fail(e,
                  "Scene native shutdown requires actual Exit and GPU fence");
  if (map_gpu_)
    map_gpu_->free();
  art_gpu_.reset();
  map_gpu_.reset();
  canvas_.clear();
  foreign_.reset();
  instances_.clear();
  source_objects_.clear();
  animations_.clear();
  art_host_ = {};
  data_ = nullptr;
  tree_ = nullptr;
  registry_ = nullptr;
  root_ = nullptr;
  map_ = nullptr;
  geometry_ = nullptr;
  art_ = nullptr;
  materials_ = nullptr;
  world_ = nullptr;
  finished_ = false;
  draw_started_ = false;
  monitors_active_ = false;
  geometry_bound_ = false;
  draw_epoch_ = 0;
  e.clear();
  return true;
}
} // namespace encore::ctr
