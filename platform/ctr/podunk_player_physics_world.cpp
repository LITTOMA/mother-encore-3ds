#include "podunk_player_physics_world.hpp"
#include "encore/field_dialogue_visual.hpp"
#include "encore/grass_native.hpp"
#include <algorithm>
#include <cmath>

namespace encore::ctr {
using namespace upstream;
namespace {
using Value = std::shared_ptr<const GlobalYamlValue>;
Value get(Value p, std::string_view k) { return p ? p->get(k) : Value{}; }
bool text(Value p, std::string &v) {
  if (!p || p->kind != 4)
    return false;
  v = p->string;
  return true;
}
Value source_node(const PlayerInitializationData &d, std::string_view path) {
  auto n = get(d.native_source(), "nodes");
  if (!n || n->kind != 5)
    return {};
  for (auto x : n->array) {
    std::string p;
    if (text(get(x, "path"), p) && p == path)
      return x;
  }
  return {};
}
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool house_deleted_landmark(const HouseReturnSources&sources,
    const FieldNodeTreeRuntime&tree,uint32_t node){
  const auto&g=sources.geometry();
  for(uint32_t at=node;at!=UINT32_MAX;at=g.node(at).parent){
    const auto source=g.node(at);
    for(const auto&landmark:sources.reentry().landmarks())
      if(g.string(source.path)==landmark.node&&!tree.source_object(source.stable_id))return true;
  }
  return false;
}
bool house_geometry_receipt(const HouseReturnSources&sources,
    const FieldNodeTreeRuntime&old_tree,FieldObjectId old_root,
    const FieldNodeTreeRuntime&tree,FieldObjectId root,
    const FieldGlobalRegistry&registry,const FieldGeometrySpace&space,std::string&e){
  const auto&nodes=sources.tree();const auto&g=sources.geometry();
  FieldIdentity identity;const auto*s=tree.state(root);const auto*d=tree.descriptor(root);
  if(!sources.valid()||space.source()!=&g||&old_tree==&tree||!old_root||
     old_tree.root()!=old_root||old_tree.state(old_root)||old_tree.lifecycle_pending()||
     registry.object_exists(old_root)||tree.object_domain()!=registry.kernel()||
     old_tree.object_domain()!=tree.object_domain()||tree.root()!=root||!s||!d||!s->alive||
     !s->bound||registry.tree_owner(root).get()!=&tree||!registry.object_exists(root)||
     d->id!=nodes.identity().scene_id||!nodes.record(d->id)||
     !tree.object_identity(root,identity)||identity.scene_id!=nodes.identity().scene_id||
     identity.upstream_commit!=nodes.identity().upstream_commit||
     identity.source_sha256!=nodes.identity().source_sha256||
     nodes.source_scene()!=sources.reentry().target_scene()||g.source_scene()!=nodes.source_scene())
    return fail(e,"Player PhysicsWorld exact loaded House/old deletion receipt rejected");
  for(uint32_t i=0;i<g.owner_count();++i){
    const auto o=g.owner(i);const auto n=g.node(o.node);const auto id=tree.source_object(n.stable_id);
    if(!id){
      if(!house_deleted_landmark(sources,tree,o.node))
        return fail(e,"Player PhysicsWorld House source owner omitted without landmark deletion");
      continue;
    }
    const auto*state=tree.state(id);const auto*descriptor=tree.descriptor(id);FieldPhysicsRid rid;
    if(!state||!descriptor||!state->bound||!registry.object_exists(id)||
       registry.tree_owner(id).get()!=&tree||!space.house_owner_rid(id,sources,rid,e)||
       !space.rid_alive(rid))return fail(e,"Player PhysicsWorld House actual owner/RID proof differs");
    for(uint32_t j=o.shape_first;j<o.shape_first+o.shape_count;++j){
      const auto shape=g.shape(j);const auto sn=g.node(shape.node);
      const auto sid=tree.source_object(sn.stable_id);const auto*ss=tree.state(sid);
      const auto*sd=tree.descriptor(sid);const auto*source=nodes.record(sn.stable_id);
      if(!sid||!ss||!sd||!source||!ss->bound||ss->parent!=id||
         !registry.object_exists(sid)||registry.tree_owner(sid).get()!=&tree||
         sd->id!=sn.stable_id||sd->path!=source->path||sd->native_class!=source->native_class||
         sd->script!=source->script||sd->script_sha!=source->script_sha||
         !tree.object_identity(sid,identity)||identity.scene_id!=nodes.identity().scene_id||
         identity.upstream_commit!=nodes.identity().upstream_commit||identity.source_sha256!=nodes.identity().source_sha256)
        return fail(e,"Player PhysicsWorld House actual owner/shape source relation differs");
    }
  }
  e.clear();return true;
}
const char *node_signal(bool area, bool enter) {
  return area ? (enter ? "area_entered" : "area_exited")
              : (enter ? "body_entered" : "body_exited");
}
const char *shape_signal(bool area, bool enter) {
  return area ? (enter ? "area_shape_entered" : "area_shape_exited")
              : (enter ? "body_shape_entered" : "body_shape_exited");
}
const char *tree_method(bool area, bool enter) {
  return area ? (enter ? "_area_enter_tree" : "_area_exit_tree")
              : (enter ? "_body_enter_tree" : "_body_exit_tree");
}
} // namespace
bool PodunkPlayerPhysicsWorld::prepare(const PlayerInitializationData &d,
                                       FieldNodeTreeRuntime &t,
                                       FieldGlobalRegistry &r,
                                       FieldGeometrySpace &s,
                                       FieldObjectSignals &bus,
                                       SourceObject source, std::string &e) {
  if (data_ || !d.valid() || !s.source() ||
      s.source()->identity().upstream_commit != d.identity().upstream_commit ||
      bus.registry() != &r || !source || t.object_domain() != r.kernel())
    return fail(e, "Player PhysicsWorld checked source/same "
                   "space/ObjectDB/SignalBus unavailable");
  data_ = &d;
  tree_ = &t;
  registry_ = &r;
  space_ = &s;
  signals_ = &bus;
  source_ = std::move(source);
  return true;
}
bool PodunkPlayerPhysicsWorld::bind_camera(PodunkPlayerCamera &c,
                                           std::string &e) {
  if (!data_ || camera_)
    return fail(
        e, "Player PhysicsWorld native Camera binding repeated/unprepared");
  camera_ = &c;
  return true;
}
bool PodunkPlayerPhysicsWorld::rebind_tree(FieldNodeTreeRuntime &next,
    SourceObject source,std::string &e){
  if(!data_||poisoned_||locked_||flushing_||!registry_||!space_||!source||
     next.object_domain()!=registry_->kernel()||!pairs_.empty()||!pending_.empty())
    return fail(e,"Player PhysicsWorld transfer is not at the native Exit boundary");
  for(const auto &entry:natives_){
    const auto &native=entry.second;const auto *state=next.state(entry.first);
    const auto *descriptor=next.descriptor(entry.first);
    const auto *original=data_->recipe().record(native.stable);FieldIdentity identity;
    if(native.entered||native.registered||!state||state->inside||!descriptor||!original||
       registry_->tree_owner(entry.first).get()!=&next||descriptor->id!=native.stable||
       descriptor->native_class!=native.klass||descriptor->script_sha!=original->script_sha||
       !next.object_identity(entry.first,identity)||
       identity.scene_id!=data_->recipe().identity().scene_id||
       identity.source_sha256!=data_->recipe().identity().source_sha256||
       identity.upstream_commit!=data_->identity().upstream_commit)
      return fail(e,"Player PhysicsWorld did not transfer its same actual native nodes");
  }
  for(const auto &entry:monitors_){
    const auto &monitor=entry.second;
    if(natives_.count(entry.first) && (monitor.native_entered||!monitor.bodies.empty()||!monitor.areas.empty()))
      return fail(e,"Player PhysicsWorld transfer retains old entered monitor pairs");
    if(!natives_.count(entry.first)&&monitor.native_entered)
      return fail(e,"Player PhysicsWorld old static monitor is still entered");
  }
  for(const auto &entry:natives_)if(entry.second.klass=="Area2D"||entry.second.klass=="KinematicBody2D")
    if(!space_->rebind_player_owner(*data_,next,*registry_,entry.first,e)){poisoned_=true;return false;}
  tree_=&next;source_=std::move(source);sampled_=false;
  house_sources_=nullptr;house_old_tree_=nullptr;house_old_root_=house_root_=0;
  e.clear();return true;
}
bool PodunkPlayerPhysicsWorld::bind_house_geometry(const HouseReturnSources&sources,
    const FieldNodeTreeRuntime&old_tree,FieldObjectId old_root,FieldObjectId root,std::string&e){
  if(!data_||poisoned_||locked_||flushing_||house_sources_||!tree_||!registry_||!space_||
     !house_geometry_receipt(sources,old_tree,old_root,*tree_,root,*registry_,*space_,e))
    return fail(e,"Player PhysicsWorld House bind lacks actual replacement/deletion closure");
  house_sources_=&sources;house_old_tree_=&old_tree;house_old_root_=old_root;house_root_=root;
  e.clear();return true;
}
bool PodunkPlayerPhysicsWorld::construct(FieldObjectId id,
                                         const FieldNodeDescriptor &n,
                                         const PlayerInitializationData &d,
                                         std::string &e) {
  FieldIdentity identity;
  const auto *original = d.recipe().record(n.id);
  if (!data_ || poisoned_ || data_ != &d || !id || natives_.count(id) ||
      !original || original->path != n.path ||
      original->native_class != n.native_class ||
      original->script_sha != n.script_sha ||
      registry_->tree_owner(id).get() != tree_ ||
      !registry_->object_exists(id) || !tree_->object_identity(id, identity) ||
      identity.scene_id != d.recipe().identity().scene_id ||
      identity.source_sha256 != d.recipe().identity().source_sha256)
    return fail(e, "Player PhysicsWorld actual native construction "
                   "cursor/source rejected");
  Native native;
  native.klass = n.native_class;
  native.stable = n.id;
  if (n.native_class == "Camera2D") {
    if (!camera_ || !camera_->construct(id, n, e))
      return false;
  } else if (n.native_class == "Area2D" ||
             n.native_class == "KinematicBody2D") {
    if (!space_->reserve_player_owner(d, *tree_, *registry_, id, e))
      return false;
    if (n.native_class == "Area2D") {
      auto p = get(source_node(d, n.path), "properties");
      auto m = get(p, "monitoring");
      if (!m || m->kind != 1)
        return fail(e, "Player PhysicsWorld Area source monitoring absent");
      monitors_.emplace(id, Monitor{m->boolean, false, {}, {}});
    }
  } else if (n.native_class == "CollisionShape2D" ||
             n.native_class == "CollisionPolygon2D") {
    if (!n.script.empty())
      return fail(e, "Player PhysicsWorld shape script owner unsupported");
    auto disabled = get(get(source_node(d, n.path), "properties"), "disabled");
    if (!disabled || disabled->kind != 1)
      return fail(e,
                  "Player PhysicsWorld native shape disabled property absent");
    native.disabled = disabled->boolean;
  } else
    return fail(e, "Player PhysicsWorld native class outside checked "
                   "collision/Camera scope");
  natives_.emplace(id, std::move(native));
  return true;
}
bool PodunkPlayerPhysicsWorld::actual_player(FieldObjectId &out,
                                             std::string &e) const {
  for (const auto &x : natives_)
    if (x.second.stable == data_->recipe().identity().scene_id &&
        x.second.klass == "KinematicBody2D") {
      out = x.first;
      return true;
    }
  return fail(e,
              "Player PhysicsWorld root native constructor/RID not executed");
}
bool PodunkPlayerPhysicsWorld::phase(FieldObjectId id, FieldTreePhase p,
                                     float delta, bool paused, std::string &e) {
  auto it = natives_.find(id);
  if (!data_ || poisoned_ || it == natives_.end() || !std::isfinite(delta) ||
      delta < 0)
    return fail(e, "Player PhysicsWorld native phase target unavailable");
  auto &n = it->second;
  if (n.klass == "Camera2D")
    return camera_ && camera_->phase(id, p, paused, e);
  bool shape = n.klass == "CollisionShape2D" || n.klass == "CollisionPolygon2D";
  if (p == FieldTreePhase::EnterNative) {
    auto state = tree_->state(id);
    if (n.entered || !state || !state->inside || !state->bound)
      return fail(e, "Player PhysicsWorld native Enter source state rejected");
    if (shape) {
      FieldObjectId player;
      if (!actual_player(player, e) ||
          !space_->register_player_shape(*data_, *tree_, *registry_, player, id,
                                         e))
        return false;
      n.registered = true;
      if (!space_->set_player_shape_disabled(id, n.disabled, e))
        return false;
    }
    n.entered = true;
    if (n.klass == "Area2D")
      monitors_.at(id).native_entered = true;
  } else if (p == FieldTreePhase::ReadyNative) {
    if (!n.entered || (shape && !n.registered))
      return fail(e, "Player PhysicsWorld native Ready requires actual world "
                     "registration");
    if (n.klass == "Area2D") {
      bool registered = false;
      for (const auto &child : natives_) {
        const auto *s = tree_->state(child.first);
        if (s && s->parent == id && child.second.registered)
          registered = true;
      }
      if (!registered)
        return fail(e, "Player PhysicsWorld Area Ready requires actual source "
                       "shape registration");
    }
    n.ready = true;
  } else if (p == FieldTreePhase::ExitNative) {
    if (!n.entered)
      return fail(e, "Player PhysicsWorld native Exit without actual Enter");
    if (shape && n.registered) {
      if (!space_->remove_player_shape(id, e))
        return false;
      n.registered = false;
    }
    if (n.klass == "Area2D") {
      if (!clear_monitor(id, e))
        return false;
      monitors_.at(id).native_entered = false;
    }
    n.entered = false;
  } else if (p == FieldTreePhase::Deleting)
    return release(id, e);
  return true;
}
bool PodunkPlayerPhysicsWorld::disabled(FieldObjectId id, bool v,
                                        std::string &e) {
  auto n = natives_.find(id);
  if (poisoned_ || locked_ || n == natives_.end() ||
      (n->second.klass != "CollisionShape2D" &&
       n->second.klass != "CollisionPolygon2D"))
    return fail(
        e,
        "Player PhysicsWorld disabled setter target/flushing_queries rejected");
  if (n->second.registered && !space_->set_player_shape_disabled(id, v, e))
    return false;
  n->second.disabled = v;
  return true;
}
bool PodunkPlayerPhysicsWorld::shape_disabled(FieldObjectId id,bool &out,std::string &e)const{
  auto it=natives_.find(id);const auto *node=tree_?tree_->state(id):nullptr;
  const auto *d=tree_?tree_->descriptor(id):nullptr;
  if(poisoned_||!registry_||it==natives_.end()||!node||!node->alive||node->queued||!d||
     !registry_->object_exists(id)||registry_->tree_owner(id).get()!=tree_||
     d->id!=it->second.stable||d->native_class!=it->second.klass||
     (it->second.klass!="CollisionShape2D"&&it->second.klass!="CollisionPolygon2D"))
    return fail(e,"Player collision property requires the same actual live native shape");
  out=it->second.disabled;e.clear();return true;
}
bool PodunkPlayerPhysicsWorld::set_collision_mask(FieldObjectId owner,
                                                  uint32_t bit, bool v,
                                                  std::string &e) {
  if (poisoned_ || locked_ || !natives_.count(owner))
    return fail(e,
                "Player PhysicsWorld source collision mask cursor unavailable");
  return space_->set_player_collision_mask(owner, bit, v, e);
}
bool PodunkPlayerPhysicsWorld::release(FieldObjectId id, std::string &e) {
  auto n = natives_.find(id);
  if (n == natives_.end())
    return fail(e, "Player PhysicsWorld release missing actual native owner");
  if (n->second.entered || n->second.registered)
    return fail(e, "Player PhysicsWorld release before real native Exit");
  if (n->second.klass == "Area2D" || n->second.klass == "KinematicBody2D") {
    if (!space_->retire_player_owner(id, e))
      return false;
    monitors_.erase(id);
  }
  for (auto it = pending_.begin(); it != pending_.end();)
    if (it->watcher == id || it->other == id)
      it = pending_.erase(it);
    else
      ++it;
  natives_.erase(n);
  return true;
}
bool PodunkPlayerPhysicsWorld::source_object(uint32_t stable,
                                             FieldObjectId &out,
                                             std::string &e) const {
  for (const auto &n : natives_)
    if (n.second.stable == stable) {
      if (!registry_->object_exists(n.first))
        return fail(e, "Player PhysicsWorld source node retired");
      out = n.first;
      return true;
    }
  return source_ && source_(stable, out, e);
}
bool PodunkPlayerPhysicsWorld::target(const FieldGeometryContact &c,
                                      FieldObjectId &out,
                                      std::string &e) const {
  if (c.actual_owner)
    out = c.actual_owner;
  else if (!source_(c.stable_id, out, e))
    return false;
  auto t = registry_->tree_owner(out);
  auto state = t ? t->state(out) : nullptr;
  auto d = t ? t->descriptor(out) : nullptr;
  if (!state || !state->alive || !state->inside || !state->bound || !d ||
      d->id != c.stable_id)
    return fail(e, "Player PhysicsWorld contacted actual native body/Area "
                   "owner unavailable");
  return true;
}
bool PodunkPlayerPhysicsWorld::admit_static_monitor(
    FieldObjectId id, const FieldGeometryContact &c, std::string &e) {
  if (!data_ || c.actual_owner || c.owner >= space_->source()->owner_count())
    return fail(
        e,
        "Player PhysicsWorld static Area source admission repeated/rejected");
  const auto *g = space_->source();
  if (c.shape >= g->shape_count() || g->shape(c.shape).owner != c.owner ||
      g->node(g->owner(c.owner).node).stable_id != c.stable_id)
    return fail(
        e,
        "Player PhysicsWorld static Area override/native binding unsupported");
  return admit_static_monitor(id, c.owner, e);
}
bool PodunkPlayerPhysicsWorld::admit_static_monitor(FieldObjectId id,
                                                    uint32_t index,
                                                    std::string &e) {
  const auto *g = space_ ? space_->source() : nullptr;
  if (!data_ || !g || monitors_.count(id) || index >= g->owner_count())
    return fail(e, "Player PhysicsWorld static native owner cursor rejected");
  const auto owner = g->owner(index);
  const auto node = g->node(owner.node);
  auto t = registry_->tree_owner(id);
  const auto *d = t ? t->descriptor(id) : nullptr;
  const auto *s = t ? t->state(id) : nullptr;
  FieldIdentity identity{};
  FieldObjectId actual = 0;
  bool house=false;
  if(house_sources_){
    const auto*root_state=tree_?tree_->state(house_root_):nullptr;
    if(!house_old_tree_||!tree_||t.get()!=tree_||tree_->lifecycle_pending()||
       !root_state||!root_state->inside||!root_state->ready_notified||!s||!s->ready_notified)
      return fail(e,"Player PhysicsWorld House monitor requires actual completed source lifecycle");
    if(!house_geometry_receipt(*house_sources_,*house_old_tree_,house_old_root_,
          *tree_,house_root_,*registry_,*space_,e))return false;
    house=true;
  }
  if (!source_object(node.stable_id, actual, e) || actual != id || !d || !s ||
      !s->alive || !s->inside || !s->bound || d->id != node.stable_id ||
      !t->object_identity(id, identity) ||
      identity.scene_id != (house?house_sources_->tree().identity().scene_id:g->identity().scene_id) ||
      identity.upstream_commit != g->identity().upstream_commit ||
      identity.source_sha256 != g->identity().source_sha256 ||
      owner.kind != 4 || owner.space_override || (owner.flags & 8) ||
      d->script_sha != node.script_sha256 || d->native_class != "Area2D")
    return fail(e, "Player PhysicsWorld static Area actual class mismatch");
  monitors_.emplace(id, Monitor{bool(owner.flags & 2), true, {}, {}});
  e.clear();
  return true;
}
bool PodunkPlayerPhysicsWorld::admit_grass_monitor(const GrassNativeData&data,FieldObjectId id,const FieldGeometryContact&contact,std::string&e){
 auto tree=registry_?registry_->tree_owner(id):nullptr;const auto*n=tree?tree->descriptor(id):nullptr;const auto*s=tree?tree->state(id):nullptr;FieldIdentity identity;FieldGeometryActor actor;FieldGeometryOwner owner;FieldGeometryShape shape;
 if(!data_||!data.valid()||monitors_.count(id)||flushing_||locked_||!tree||tree.get()!=tree_||!n||!s||!s->inside||!s->bound||!s->ready_notified||contact.actual_owner!=id||n->id!=data.node(GrassNativeRole::Area)||!data.native_matches(*n)||!tree->object_identity(id,identity)||identity.scene_id!=data.identity().scene_id||identity.source_sha256!=data.identity().source_sha256||identity.upstream_commit!=data.identity().upstream_commit||!space_->live_geometry(contact,actor,owner,shape,e)||owner.kind!=4||actor.kind!=FieldGeometryKind::Rectangle||owner.layer!=data.profile().collision_layer||owner.mask!=data.profile().collision_mask||owner.flags!=((data.monitoring()?2u:0u)|(data.monitorable()?4u:0u)))return fail(e,"Grass actual PhysicsWorld source/Ready/Rectangle monitor rejected");
 monitors_.emplace(id,Monitor{data.monitoring(),true,{},{}});grass_monitors_.emplace(id,contact);e.clear();return true;
}
bool PodunkPlayerPhysicsWorld::static_monitor_exit(FieldObjectId id,
                                                   std::string &e) {
  if (natives_.count(id) || !monitors_.count(id) || flushing_ || locked_)
    return fail(e, "Player PhysicsWorld static Area Exit cursor rejected");
  if (!clear_monitor(id, e))
    return false;
  monitors_.erase(id);
  grass_monitors_.erase(id);
  dialogue_monitors_.erase(id);
  for (auto i = pairs_.begin(); i != pairs_.end();)
    if (i->watcher == id)
      i = pairs_.erase(i);
    else
      ++i;
  for (auto i = pending_.begin(); i != pending_.end();)
    if (i->watcher == id)
      i = pending_.erase(i);
    else
      ++i;
  e.clear();
  return true;
}
bool PodunkPlayerPhysicsWorld::geometry_admitted(FieldObjectId owner,
                                                 FieldObjectId shape,
                                                 std::string &e) const {
  auto a = natives_.find(owner), b = natives_.find(shape);
  if (a == natives_.end() || b == natives_.end() || !a->second.entered ||
      !b->second.entered || !b->second.registered)
    return fail(e,
                "Player PhysicsWorld Camera geometry not actually registered");
  std::vector<FieldGeometryContact> parts;
  if (!space_->player_shapes(owner, parts, e))
    return false;
  for (const auto &p : parts)
    if (p.actual_shape == shape)
      return true;
  return fail(e, "Player PhysicsWorld actual Area/shape relation mismatched");
}
bool PodunkPlayerPhysicsWorld::admit_dialogue_camera_monitor(
 const FieldDialogueVisualData &data,const FieldNodeRecipeData &recipe,
 FieldObjectId id,const FieldGeometryContact &contact,std::string &e){
 auto tree=registry_?registry_->tree_owner(id):nullptr;
 const auto *node=tree?tree->descriptor(id):nullptr;
 const auto *state=tree?tree->state(id):nullptr;FieldIdentity identity;
 FieldGeometryActor actor;FieldGeometryOwner owner;FieldGeometryShape shape;
 const FieldGameCameraDescriptor *camera=nullptr;
 if(node)for(const auto &c:data.camera().records())if(c.area_id==node->id)camera=&c;
 if(!data_||!data.valid()||!recipe.valid()||data.recipe_sha()!=recipe.ir_sha256()||
    monitors_.count(id)||flushing_||locked_||!tree||!node||!state||!state->inside||!state->bound||
    !registry_->object_exists(id)||!camera||contact.actual_owner!=id||
    node->native_class!="Area2D"||!recipe.record(node->id)||
    !tree->object_identity(id,identity)||identity.scene_id!=recipe.identity().scene_id||
    identity.source_sha256!=recipe.identity().source_sha256||identity.upstream_commit!=recipe.identity().upstream_commit||
    !space_->live_geometry(contact,actor,owner,shape,e)||owner.kind!=4||
    actor.kind!=FieldGeometryKind::Rectangle||actor.extents.x!=camera->shape_extents.x||actor.extents.y!=camera->shape_extents.y||
    owner.layer!=camera->area_layer||owner.mask!=camera->area_mask||
    owner.flags!=((camera->area_flags&1?2u:0u)|(camera->area_flags&2?4u:0u)))
   return fail(e,"Dialogue Camera actual PhysicsWorld shape/source rejected");
 monitors_.emplace(id,Monitor{bool(camera->area_flags&1),true,{},{}});
 dialogue_monitors_.emplace(id,contact);return true;
}
bool PodunkPlayerPhysicsWorld::collect(std::set<Pair> &out, std::string &e) {
  std::set<Pair> result;
  for (const auto &native : natives_) {
    if (!native.second.entered || (native.second.klass != "Area2D" &&
                                   native.second.klass != "KinematicBody2D"))
      continue;
    std::vector<FieldGeometryContact> parts;
    if (!space_->player_shapes(native.first, parts, e))
      return false;
    for (const auto &self : parts) {
      FieldGeometryActor a;
      FieldGeometryOwner own;
      FieldGeometryShape s;
      if (!space_->live_geometry(self, a, own, s, e))
        return false;
      FieldGeometryFilter f;
      f.areas = true;
      f.bodies = true;
      f.bilateral_mask = true;
      f.layer_mask = own.mask;
      f.reciprocal_layer = own.layer;
      std::vector<FieldGeometryContact> hits;
      if (!space_->overlap_actor(a, f, space_->indexed_instance_count(), hits,
                                 e))
        return false;
      for (const auto &contact : hits) {
        FieldObjectId other;
        if (!target(contact, other, e))
          return false;
        if (other == native.first)
          continue;
        FieldGeometryActor b;
        FieldGeometryOwner otherOwner;
        FieldGeometryShape otherShape;
        if (!space_->live_geometry(contact, b, otherOwner, otherShape, e))
          return false;
        bool otherArea = otherOwner.kind == 4;
        if (own.kind == 4 && (own.flags & 2) &&
            (!otherArea || (otherOwner.flags & 4))) {
          FieldPhysicsRid rid;
          if (!space_->physics_rid(contact, rid, e))
            return false;
          result.insert(Pair{native.first, other, rid,
                             contact.native_shape_index,
                             self.native_shape_index, otherArea});
        }
        if (otherArea && (otherOwner.flags & 2) &&
            (own.kind != 4 || (own.flags & 4))) {
          auto monitor = monitors_.find(other);
          if (monitor == monitors_.end() || !monitor->second.native_entered)
            return fail(e, "Player PhysicsWorld contacted source monitoring "
                           "Area lacks actual native consumer");
          FieldPhysicsRid rid;
          if (!space_->physics_rid(self, rid, e))
            return false;
          result.insert(Pair{other, native.first, rid, self.native_shape_index,
                             contact.native_shape_index, own.kind == 4});
        }
      }
    }
  }
  for(const auto&entry:grass_monitors_){
    FieldGeometryActor actor;FieldGeometryOwner owner;FieldGeometryShape shape;
    if(!space_->live_geometry(entry.second,actor,owner,shape,e))return false;
    if(!(owner.flags&2))continue;
    FieldGeometryFilter filter;filter.bodies=true;filter.areas=false;filter.bilateral_mask=true;filter.layer_mask=owner.mask;filter.reciprocal_layer=owner.layer;
    std::vector<FieldGeometryContact>hits;
    if(!space_->overlap_actor(actor,filter,space_->indexed_instance_count(),hits,e))return false;
    for(const auto&hit:hits){FieldObjectId other=0;FieldPhysicsRid rid;if(!target(hit,other,e)||!space_->physics_rid(hit,rid,e))return false;if(other==entry.first)continue;result.insert(Pair{entry.first,other,rid,hit.native_shape_index,entry.second.native_shape_index,false});}
  }
  for(const auto &entry:dialogue_monitors_){
    FieldGeometryActor actor;FieldGeometryOwner owner;FieldGeometryShape shape;
    if(!space_->live_geometry(entry.second,actor,owner,shape,e))return false;
    FieldGeometryFilter filter;filter.bodies=true;filter.areas=true;filter.bilateral_mask=true;
    filter.layer_mask=owner.mask;filter.reciprocal_layer=owner.layer;
    std::vector<FieldGeometryContact> hits;
    if(!space_->overlap_actor(actor,filter,space_->indexed_instance_count(),hits,e))return false;
    for(const auto &hit:hits){
      FieldObjectId other=0;FieldGeometryActor otherActor;FieldGeometryOwner otherOwner;FieldGeometryShape otherShape;
      if(!target(hit,other,e)||!space_->live_geometry(hit,otherActor,otherOwner,otherShape,e))return false;
      if(other==entry.first)continue;
      bool otherArea=otherOwner.kind==4;
      if((owner.flags&2)&&(!otherArea||(otherOwner.flags&4))){
        FieldPhysicsRid rid;if(!space_->physics_rid(hit,rid,e))return false;
        result.insert(Pair{entry.first,other,rid,hit.native_shape_index,entry.second.native_shape_index,otherArea});
      }
      if(otherArea&&(otherOwner.flags&2)&&(owner.flags&4)){
        auto monitor=monitors_.find(other);
        if(monitor==monitors_.end()||!monitor->second.native_entered)
          return fail(e,"Dialogue Camera contacted Area lacks actual monitor owner");
        FieldPhysicsRid rid;if(!space_->physics_rid(entry.second,rid,e))return false;
        result.insert(Pair{other,entry.first,rid,entry.second.native_shape_index,hit.native_shape_index,true});
      }
    }
  }
  out = std::move(result);
  return true;
}
bool PodunkPlayerPhysicsWorld::physics_step(uint64_t epoch, std::string &e) {
  if (!data_ || poisoned_ || flushing_ || sampled_ || !epoch || epoch <= epoch_)
    return fail(e, "Player PhysicsWorld actual step/flush order rejected");
  std::set<Pair> next;
  if (!collect(next, e))
    return false;
  pending_ = std::move(next);
  epoch_ = epoch;
  sampled_ = true;
  return true;
}
bool PodunkPlayerPhysicsWorld::connect_tree(FieldObjectId area,
                                            FieldObjectId other, bool otherArea,
                                            bool connecting, std::string &e) {
  if (other > uint64_t(INT64_MAX))
    return fail(e, "Player PhysicsWorld source ObjectID integer out of range");
  for (bool enter : {true, false}) {
    const char *signal = enter ? "tree_entered" : "tree_exiting";
    const char *method = tree_method(otherArea, enter);
    if (connecting) {
      if (!signals_->connect(other, signal, area, method, 0, {int64_t(other)},
                             e))
        return false;
    } else if (!signals_->disconnect(other, signal, area, method, e))
      return false;
  }
  return true;
}
bool PodunkPlayerPhysicsWorld::emit_shape(FieldObjectId area, const Pair &p,
                                          bool enter, std::string &e) {
  if (enter ? !space_->rid_alive(p.rid) : !space_->rid_issued(p.rid))
    return fail(e, "Player PhysicsWorld monitor callback lacks actual Physics "
                   "RID ownership");
  return signals_->emit(
      area, shape_signal(p.area, enter),
      {p.rid, FieldObjectRef{registry_->object_exists(p.other) ? p.other : 0},
       int64_t(p.other_shape), int64_t(p.self_shape)},
      e);
}
bool PodunkPlayerPhysicsWorld::inout(const Pair &p, bool enter,
                                     std::string &e) {
  auto m = monitors_.find(p.watcher);
  if (m == monitors_.end() || !m->second.native_entered)
    return fail(
        e,
        "Player PhysicsWorld actual source monitoring callback owner absent");
  auto &map = p.area ? m->second.areas : m->second.bodies;
  auto it = map.find(p.other);
  const auto shape = std::make_pair(p.other_shape, p.self_shape);
  if (!enter && it == map.end())
    return true;
  auto t = registry_->tree_owner(p.other);
  auto node = t ? t->state(p.other) : nullptr;
  bool alive = node && node->alive;
  locked_ = true;
  bool ok = true;
  if (enter) {
    if (it == map.end()) {
      it = map.emplace(p.other, Body{p.rid, alive && node->inside, {}}).first;
      if (alive) {
        ok = connect_tree(p.watcher, p.other, p.area, true, e);
        if (ok && it->second.inside)
          ok = signals_->emit(p.watcher, node_signal(p.area, true),
                              {FieldObjectRef{p.other}}, e);
      }
    }
    if (ok && !it->second.shapes.insert(shape).second)
      ok = fail(e, "Player PhysicsWorld duplicate native shape enter");
    if (ok && (!alive || it->second.inside))
      ok = emit_shape(p.watcher, p, true, e);
  } else {
    bool inside = it->second.inside;
    if (!it->second.shapes.erase(shape))
      ok = fail(e, "Player PhysicsWorld unknown native shape exit");
    if (ok && it->second.shapes.empty()) {
      map.erase(it);
      if (alive) {
        ok = connect_tree(p.watcher, p.other, p.area, false, e);
        if (ok && inside)
          ok = signals_->emit(p.watcher, node_signal(p.area, false),
                              {FieldObjectRef{p.other}}, e);
      }
    }
    if (ok && (!alive || inside))
      ok = emit_shape(p.watcher, p, false, e);
  }
  locked_ = false;
  if (!ok)
    poisoned_ = true;
  return ok;
}
bool PodunkPlayerPhysicsWorld::flush_queries(std::string &e) {
  if (!data_ || poisoned_ || flushing_ || !sampled_)
    return fail(e, "Player PhysicsWorld flush without one actual sampled step");
  struct Change {
    Pair pair;
    bool enter;
  };
  std::vector<Change> changes;
  for (const auto &p : pairs_)
    if (!pending_.count(p))
      changes.push_back({p, false});
  for (const auto &p : pending_)
    if (!pairs_.count(p))
      changes.push_back({p, true});
  std::sort(changes.begin(), changes.end(),
            [](const Change &a, const Change &b) { return a.pair < b.pair; });
  flushing_ = true;
  for (const auto &x : changes)
    if (!inout(x.pair, x.enter, e)) {
      flushing_ = false;
      return false;
    }
  pairs_ = std::move(pending_);
  pending_.clear();
  sampled_ = false;
  flushing_ = false;
  return true;
}
bool PodunkPlayerPhysicsWorld::clear_monitor(FieldObjectId area,
                                             std::string &e) {
  if (locked_)
    return fail(e,
                "Player PhysicsWorld clear monitoring locked by native signal");
  auto m = monitors_.find(area);
  if (m == monitors_.end())
    return fail(e, "Player PhysicsWorld clear missing Area native body");
  auto copy = m->second;
  m->second.bodies.clear();
  m->second.areas.clear();
  for (bool otherArea : {false, true})
    for (const auto &x : otherArea ? copy.areas : copy.bodies) {
      auto t = registry_->tree_owner(x.first);
      auto state = t ? t->state(x.first) : nullptr;
      if (!state || !state->alive)
        continue;
      if (!connect_tree(area, x.first, otherArea, false, e))
        return false;
      if (!x.second.inside)
        continue;
      for (const auto &shape : x.second.shapes) {
        Pair p{area,        x.first,      x.second.rid,
               shape.first, shape.second, otherArea};
        if (!emit_shape(area, p, false, e))
          return false;
      }
      if (!signals_->emit(area, node_signal(otherArea, false),
                          {FieldObjectRef{x.first}}, e))
        return false;
    }
  for (auto it = pairs_.begin(); it != pairs_.end();)
    if (it->watcher == area)
      it = pairs_.erase(it);
    else
      ++it;
  for (auto it = pending_.begin(); it != pending_.end();)
    if (it->watcher == area)
      it = pending_.erase(it);
    else
      ++it;
  return true;
}
bool PodunkPlayerPhysicsWorld::tree_event(FieldObjectId area,
                                          FieldObjectId other, bool otherArea,
                                          bool enter, std::string &e) {
  auto m = monitors_.find(area);
  if (m == monitors_.end())
    return fail(e, "Player PhysicsWorld tree signal missing real Area state");
  auto &map = otherArea ? m->second.areas : m->second.bodies;
  auto b = map.find(other);
  if (b == map.end() || b->second.inside == enter)
    return fail(e, "Player PhysicsWorld native tree transition/map mismatch");
  b->second.inside = enter;
  if (!signals_->emit(area, node_signal(otherArea, enter),
                      {FieldObjectRef{other}}, e))
    return false;
  for (const auto &s : b->second.shapes) {
    Pair p{area, other, b->second.rid, s.first, s.second, otherArea};
    if (!emit_shape(area, p, enter, e))
      return false;
  }
  return true;
}
bool PodunkPlayerPhysicsWorld::handles_callback(
    const FieldDeferredMessage &m) const {
  return monitors_.count(m.object) &&
         (m.member == "_body_enter_tree" || m.member == "_body_exit_tree" ||
          m.member == "_area_enter_tree" || m.member == "_area_exit_tree");
}
bool PodunkPlayerPhysicsWorld::deferred(const FieldDeferredMessage &m,
                                        std::string &e) {
  if (camera_ && camera_->owns(m.object))
    return camera_->deferred(m, e);
  if (!handles_callback(m) || m.kind != FieldDeferredKind::Call ||
      m.args.size() != 1)
    return fail(
        e, "Player PhysicsWorld native callback/source signature rejected");
  auto id = std::get_if<int64_t>(&m.args[0]);
  if (!id || *id <= 0)
    return fail(e,
                "Player PhysicsWorld native tree callback ObjectID rejected");
  bool area = m.member == "_area_enter_tree" || m.member == "_area_exit_tree";
  bool enter = m.member == "_area_enter_tree" || m.member == "_body_enter_tree";
  return tree_event(m.object, FieldObjectId(*id), area, enter, e);
}
bool PodunkPlayerPhysicsWorld::overlapping(FieldObjectId area, bool otherAreas,
                                           std::vector<FieldObjectId> &out,
                                           std::string &e) const {
  auto m = monitors_.find(area);
  if (poisoned_ || m == monitors_.end() || !m->second.native_entered)
    return fail(e,
                "Player PhysicsWorld overlapping getter without actual Area");
  std::vector<FieldObjectId> result;
  for (const auto &x : otherAreas ? m->second.areas : m->second.bodies)
    if (x.second.inside)
      result.push_back(x.first);
  out = std::move(result);
  return true;
}
} // namespace encore::ctr
