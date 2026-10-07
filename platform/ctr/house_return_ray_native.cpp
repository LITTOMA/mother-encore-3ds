#include "house_return_ray_native.hpp"
#include <cmath>
#include <set>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string&e,const char*m){e=m;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){return a.scene_id==b.scene_id&&
    a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;}
bool equal(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=1000000&&std::abs(p.y)<=1000000;}
Vec2 xform(const FieldTransform&t,Vec2 p){return {t[2].x+t[0].x*p.x+t[1].x*p.y,
    t[2].y+t[0].y*p.x+t[1].y*p.y};}
}
bool HouseReturnRayNative::prepare(const HouseReturnSources&s,const FieldNpcWorldData&d,
    const FieldNpcData&npc,FieldNodeTreeRuntime&t,FieldGlobalRegistry&r,
    FieldObjectSignals&bus,FieldGeometrySpace&space,PodunkPlayerPhysicsWorld&world,
    HouseReturnKinematicNative&kinematic,std::string&e){
  if(sources_||!s.valid()||&d!=&s.npc_world()||&npc!=&s.npcs()||!d.valid()||!npc.valid()||!same(d.identity(),s.tree().identity())||
     npc.scene_id()!=s.tree().identity().scene_id||npc.source_pin()!=d.identity().upstream_commit||
     world.geometry_space()!=&space||world.registry()!=&r||bus.registry()!=&r||!r.kernel()||
     !space.source()||space.source()->source_scene()!=s.reentry().source_scene()||
     !finite(d.zero_cast())||(!d.zero_cast().x&&!d.zero_cast().y))
    return fail(e,"House Ray native preparation lacks actual checked NPCWorld/same Space closure");
  bool scene=false;for(const auto&f:npc.sources())
    if(f.path==s.reentry().target_scene())scene=f.sha256==s.tree().identity().source_sha256;
  if(!scene)return fail(e,"House Ray NPC source scene fingerprint differs");
  std::set<uint32_t>rays;
  for(const auto&ray:d.rays()){
    const auto*n=s.tree().record(ray.id);const auto*parent=s.tree().record(ray.parent);
    const auto*link=d.npc(ray.parent);const FieldNpcDescriptor*source=nullptr;
    for(const auto&v:npc.npcs())if(v.id==ray.parent)source=&v;
    if(!n||n->native_class!="RayCast2D"||!n->script.empty()||n->parent!=ray.parent||n->path!=ray.path||
       !parent||parent->native_class!="KinematicBody2D"||!link||link->ray!=ray.id||
       !source||source->node!=parent->path||source->geometry.size()!=9||!rays.insert(ray.id).second||
       source->geometry[5].kind!=3||source->geometry[5].mask!=ray.mask||
       !equal(source->geometry[5].value,ray.cast)||!equal(source->geometry[5].offset,n->local[2])||
       !finite(ray.cast))return fail(e,"House Ray checked native source/NPC child mapping differs");
  }
  for(const auto&n:s.tree().records())if(n.native_class=="RayCast2D"&&!rays.count(n.id))
    return fail(e,"House complete tree Ray native owner closure incomplete");
  if(rays.empty())return fail(e,"House Ray checked native rows absent");
  sources_=&s;data_=&d;npcs_=&npc;tree_=&t;registry_=&r;signals_=&bus;space_=&space;world_=&world;kinematic_=&kinematic;
  e.clear();return true;
}
bool HouseReturnRayNative::owns(const FieldNodeDescriptor&d)const{return data_&&data_->ray(d.id)&&d.native_class=="RayCast2D";}
bool HouseReturnRayNative::owns(FieldObjectId id)const{return instances_.count(id)!=0;}
bool HouseReturnRayNative::actual(FieldObjectId id,const FieldNodeDescriptor*&d,const FieldNodeState*&s,std::string&e)const{
  d=tree_?tree_->descriptor(id):nullptr;s=tree_?tree_->state(id):nullptr;FieldIdentity identity;
  if(!sources_||!sources_->valid()||!data_->valid()||!npcs_->valid()||poisoned_||!d||!s||!s->alive||
     !registry_->object_exists(id)||registry_->tree_owner(id).get()!=tree_||
     tree_->object_domain()!=registry_->kernel()||signals_->registry()!=registry_||
     !tree_->object_identity(id,identity)||!same(identity,sources_->tree().identity())||
     !same(data_->identity(),identity)||tree_->source_object(d->id)!=id)
    return fail(e,"House Ray actual ObjectDB/tree/native source owner absent");
  const auto*source=sources_->tree().record(d->id);
  if(!source||source->path!=d->path||source->native_class!=d->native_class||source->class_index!=d->class_index||
     source->script!=d->script||source->script_sha!=d->script_sha)
    return fail(e,"House Ray actual full-tree descriptor differs");
  e.clear();return true;
}
bool HouseReturnRayNative::construct(FieldObjectId id,const FieldNodeDescriptor&d,const FieldIdentity&i,std::string&e){
  const FieldNodeDescriptor*ad;const FieldNodeState*s;
  if(!sources_||finished_||!owns(d)||!same(i,data_->identity())||instances_.count(id)||source_objects_.count(d.id)||
     !actual(id,ad,s,e)||ad->id!=d.id||s->inside||s->parent||s->bound||s->ready_notified)
    return fail(e,"House Ray native construction source/cursor differs");
  // Source-native property values, including booleans, come solely from the
  // separately checked NPCWorld binary. No default inference from class name.
  Instance n;n.source=*data_->ray(d.id);instances_.emplace(id,n);source_objects_.emplace(d.id,id);
  e.clear();return true;
}
bool HouseReturnRayNative::bind(FieldObjectId id,FieldNodeBinding&out,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted||i->second.bound||!actual(id,d,s,e)||!d->script.empty())
    return fail(e,"House Ray native binding absent/duplicate/attached script unsupported");
  FieldNodeBinding b;b.identity=data_->identity();b.stable_id=d->id;b.class_index=d->class_index;
  b.family=0x454e006a;b.capability=1;b.native_class=d->native_class;b.script_sha=d->script_sha;
  i->second.bound=true;out=std::move(b);e.clear();return true;
}
bool HouseReturnRayNative::finish_factory(std::string&e){
  if(!sources_||finished_||!tree_->root()||tree_->lifecycle_pending())return fail(e,"House Ray native factory boundary differs");
  for(const auto&ray:data_->rays()){
    auto i=source_objects_.find(ray.id);
    if(i==source_objects_.end()||tree_->source_object(ray.id)!=i->second||!instances_.count(i->second))
      return fail(e,"House Ray native factory omitted a source row");
  }
  finished_=true;e.clear();return true;
}
bool HouseReturnRayNative::live_space(std::string&e)const{
  const auto*s=tree_?tree_->state(root_):nullptr;const auto*d=tree_?tree_->descriptor(root_):nullptr;
  if(!sources_||!replaced_||poisoned_||space_->source()!=&sources_->geometry()||world_->geometry_space()!=space_||
     world_->registry()!=registry_||!old_tree_||old_tree_==tree_||old_tree_->root()!=old_root_||old_tree_->state(old_root_)||
     old_tree_->lifecycle_pending()||registry_->object_exists(old_root_)||root_!=tree_->root()||!s||!d||!s->alive||
     !s->bound||d->id!=sources_->tree().identity().scene_id||registry_->tree_owner(root_).get()!=tree_)
    return fail(e,"House Ray native World2D lacks actual replacement/old-tree deletion receipt");
  const FieldNodeDescriptor*ad;const FieldNodeState*as;return actual(root_,ad,as,e);
}
bool HouseReturnRayNative::parent_receipt(FieldObjectId id,Instance&n,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;FieldObjectId parent=0;
  if(!actual(id,d,s,e)||!s->inside||!s->parent||
     !kinematic_->source_object(n.source.parent,parent,e)||parent!=s->parent||!kinematic_->owns(parent))
    return fail(e,"House Ray native parent is not its actual checked Kinematic owner");
  const auto*p=tree_->state(parent);
  if(!p||!p->inside||!p->bound)return fail(e,"House Ray actual parent native Enter/binding absent");
  n.parent=parent;
  if(replaced_){FieldPhysicsRid rid;
    if(!kinematic_->owner_rid(parent,rid,e))return false;
    if(n.parent_rid.handle&&(n.parent_rid.space!=rid.space||n.parent_rid.handle!=rid.handle))
      return fail(e,"House Ray parent exclusion RID changed without actual parent lifetime");
    n.parent_rid=rid;
  }
  e.clear();return true;
}
bool HouseReturnRayNative::bind_replaced_space(const FieldNodeTreeRuntime&old,
    FieldObjectId old_root,FieldObjectId root,std::string&e){
  if(!finished_||replaced_||!old_root||old.root()!=old_root||old.state(old_root)||old.lifecycle_pending()||
     &old==tree_||old.object_domain()!=tree_->object_domain()||registry_->object_exists(old_root)||
     space_->source()!=&sources_->geometry()||root!=tree_->root())
    return fail(e,"House Ray native bind precedes actual same-space replacement");
  old_tree_=&old;old_root_=old_root;root_=root;replaced_=true;
  if(!live_space(e)){replaced_=false;return false;}
  for(auto&row:instances_)if(!row.second.deleted&&row.second.entered&&!parent_receipt(row.first,row.second,e)){
    poisoned_=true;return false;}
  e.clear();return true;
}
bool HouseReturnRayNative::phase(FieldObjectId id,FieldTreePhase p,bool paused,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted||!i->second.bound||!actual(id,d,s,e)||!s->bound||
     s->binding.family!=0x454e006a||s->binding.capability!=1)
    return fail(e,"House Ray native phase lacks its actual checked binding");
  auto&n=i->second;const auto abort=[&](){poisoned_=true;return false;};
  switch(p){
  case FieldTreePhase::EnterNative:
    if(!finished_||n.entered||!s->inside)return fail(e,"House Ray native Enter cursor differs");
    n.entered=true;
    if(!parent_receipt(id,n,e)||
       !(n.source.enabled?tree_->add_group(id,"physics_process_internal",e):tree_->remove_group(id,"physics_process_internal",e)))return abort();
    break;
  case FieldTreePhase::ReadyNative:
    if(!n.entered||n.ready||!s->inside||!s->ready_notified)return fail(e,"House Ray native Ready precedes actual Enter");
    n.ready=true;break;
  case FieldTreePhase::PhysicsInternal:
    if(!n.entered||!n.ready||!s->inside)return fail(e,"House Ray internal physics precedes actual native lifecycle");
    if(n.source.enabled&&tree_->can_process(id,paused)&&!update(id,n,e))return false;
    break;
  case FieldTreePhase::ExitNative:
    if(!n.entered||!s->inside)return fail(e,"House Ray native Exit cursor differs");
    if(n.source.enabled&&!tree_->remove_group(id,"physics_process_internal",e))return abort();
    n.entered=false;break;
  case FieldTreePhase::Deleting:
    if(n.entered||s->inside||s->parent||!s->children.empty())return fail(e,"House Ray delete precedes actual Exit/detach/child delete");
    n.deleted=true;break;
  case FieldTreePhase::TreeEntered:
  case FieldTreePhase::TreeExiting:
    if(!n.entered||!s->inside)return fail(e,"House Ray tree signal cursor differs");
    if(!signals_->emit(id,p==FieldTreePhase::TreeEntered?"tree_entered":"tree_exiting",{},e))return abort();break;
  case FieldTreePhase::TreeExited:
    if(n.entered||s->inside)return fail(e,"House Ray tree_exited precedes actual Exit");
    if(!signals_->emit(id,"tree_exited",{},e))return abort();break;
  case FieldTreePhase::ReadySignal:
    if(!n.entered||!n.ready||!s->inside||!s->ready_notified)return fail(e,"House Ray ready signal native cursor differs");
    if(!signals_->emit(id,"ready",{},e))return abort();break;
  case FieldTreePhase::ChildEntered:
  case FieldTreePhase::ChildExiting:
    if(!kinematic_->child_phase(id,p,e))return abort();break;
  case FieldTreePhase::VisibilityChanged:
    if(!signals_->emit(id,"visibility_changed",{},e))return abort();break;
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::NodeAdded:
  case FieldTreePhase::NodeRemoved:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::TransformChanged:
  case FieldTreePhase::LocalTransformChanged:
  case FieldTreePhase::Hide:
    // Actual Tree owns Node/Canvas fields. Transform changes deliberately do
    // not update the ray cache; Godot updates it only on native physics/force.
    break;
  default:return fail(e,"House Ray native cannot execute an attached script/source process phase");
  }
  e.clear();return true;
}
bool HouseReturnRayNative::update(FieldObjectId id,Instance&n,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  if(!live_space(e)||!actual(id,d,s,e)||!n.entered||!n.ready||!s->inside||!s->bound||s->queued||
     tree_->lifecycle_pending()||world_->tree()!=tree_||!parent_receipt(id,n,e))
    return fail(e,"House Ray query lacks actual completed source World2D/native lifecycle");
  std::vector<FieldGeometryContact>parent_shapes;
  if(!kinematic_->body_shapes(n.parent,parent_shapes,e))return false;
  FieldTransform world;if(!tree_->world_transform(id,world,e))return false;
  auto cast=n.source.cast;if(!cast.x&&!cast.y)cast=data_->zero_cast();
  const auto from=world[2],to=xform(world,cast);
  if(!finite(from)||!finite(to)||equal(from,to))return fail(e,"House Ray source transform collapses the checked cast");
  FieldGeometryFilter filter;filter.layer_mask=n.source.mask;filter.bodies=n.source.bodies;filter.areas=n.source.areas;
  if(n.source.exclude){
    if(!space_->rid_alive(n.parent_rid))return fail(e,"House Ray exclusion lacks actual parent physics RID");
    filter.exclude_stable_id=n.source.parent;
  }
  bool collided=false;FieldGeometryRayHit hit;
  if(!space_->ray(from,to,filter,space_->indexed_instance_count(),collided,hit,e))return false;
  auto candidate=n.cached;candidate.collided=collided;
  if(collided){const auto target=hit.actual_owner;auto owner=registry_->tree_owner(target);
    const auto*state=owner?owner->state(target):nullptr;const auto*descriptor=owner?owner->descriptor(target):nullptr;
    FieldGeometryActor actor;FieldGeometryOwner native_owner;FieldGeometryShape shape;FieldPhysicsRid rid;
    if(!target||!registry_->object_exists(target)||!state||!descriptor||!state->alive||!state->inside||!state->bound||
       state->queued||state->world_dirty||descriptor->id!=hit.stable_id||
       !space_->live_geometry(hit,actor,native_owner,shape,e)||!space_->physics_rid(hit,rid,e)||
       !space_->rid_alive(rid)||(n.source.exclude&&rid.space==n.parent_rid.space&&rid.handle==n.parent_rid.handle))
      return fail(e,"House Ray cached result lacks an actual supported collider/RID");
    if(kinematic_->owns(target)){
      std::vector<FieldGeometryContact>parts;if(!kinematic_->body_shapes(target,parts,e))return false;
    }
    candidate.against=target;candidate.shape=hit.native_shape_index;candidate.point=hit.position;candidate.normal=hit.normal;
  }else{candidate.against=0;candidate.shape=0;}
  // A miss preserves point/normal, as the original native RayCast2D does.
  n.cached=candidate;e.clear();return true;
}
bool HouseReturnRayNative::set_enabled(FieldObjectId id,bool value,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted||!actual(id,d,s,e))return fail(e,"House Ray enabled setter actual native owner absent");
  if(i->second.entered&&!(value?tree_->add_group(id,"physics_process_internal",e):tree_->remove_group(id,"physics_process_internal",e)))return false;
  i->second.source.enabled=value;
  // Original set_enabled(false) clears only collided. get_collider still
  // resolves the cached against ObjectID until a later real ray update.
  if(!value)i->second.cached.collided=false;
  e.clear();return true;
}
bool HouseReturnRayNative::set_cast_to(FieldObjectId id,Vec2 value,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted||!finite(value)||!actual(id,d,s,e))return fail(e,"House Ray cast setter actual source/value rejected");
  i->second.source.cast=value;e.clear();return true;
}
bool HouseReturnRayNative::set_collision_mask(FieldObjectId id,uint32_t mask,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted||!actual(id,d,s,e))return fail(e,"House Ray mask setter actual native owner absent");
  i->second.source.mask=mask;e.clear();return true;
}
bool HouseReturnRayNative::set_exclude_parent(FieldObjectId id,bool value,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted||!actual(id,d,s,e))return fail(e,"House Ray parent exclusion setter owner absent");
  if(i->second.entered&&!parent_receipt(id,i->second,e))return false;
  i->second.source.exclude=value;e.clear();return true;
}
bool HouseReturnRayNative::set_collide_with(FieldObjectId id,bool bodies,bool areas,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted||!actual(id,d,s,e))return fail(e,"House Ray collision-kind setter owner absent");
  i->second.source.bodies=bodies;i->second.source.areas=areas;e.clear();return true;
}
bool HouseReturnRayNative::cache(FieldObjectId id,Cache&out,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted||!actual(id,d,s,e))return fail(e,"House Ray cache getter actual native owner absent");
  out=i->second.cached;e.clear();return true;
}
bool HouseReturnRayNative::collider(FieldObjectId id,FieldObjectId&out,std::string&e)const{
  Cache value;if(!cache(id,value,e))return false;
  out=value.against&&registry_->object_exists(value.against)?value.against:0;e.clear();return true;
}
bool HouseReturnRayNative::force_update(FieldObjectId id,std::string&e){
  auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted)return fail(e,"House Ray force update native owner absent");
  return update(id,i->second,e);
}
bool HouseReturnRayNative::deferred(const FieldDeferredMessage&m,std::string&e){
  if(!owns(m.object)||m.kind!=FieldDeferredKind::Call)return fail(e,"House Ray native method receiver/kind rejected");
  if(m.member=="force_raycast_update"&&m.args.empty())return force_update(m.object,e);
  if(m.args.size()!=1)return fail(e,"House Ray native method arity rejected");
  if(m.member=="set_cast_to"){auto v=std::get_if<Vec2>(&m.args[0]);return v?set_cast_to(m.object,*v,e):fail(e,"House Ray cast method requires Vector2");}
  if(m.member=="set_collision_mask"){auto v=std::get_if<int64_t>(&m.args[0]);return v&&*v>=0&&uint64_t(*v)<=UINT32_MAX?
      set_collision_mask(m.object,uint32_t(*v),e):fail(e,"House Ray collision mask method requires uint32");}
  auto v=std::get_if<bool>(&m.args[0]);if(!v)return fail(e,"House Ray native Boolean method value rejected");
  if(m.member=="set_enabled")return set_enabled(m.object,*v,e);
  if(m.member=="set_exclude_parent_body")return set_exclude_parent(m.object,*v,e);
  auto i=instances_.find(m.object);
  if(m.member=="set_collide_with_bodies")return set_collide_with(m.object,*v,i->second.source.areas,e);
  if(m.member=="set_collide_with_areas")return set_collide_with(m.object,i->second.source.bodies,*v,e);
  return fail(e,"House Ray unsupported native source method");
}
bool HouseReturnRayNative::declaration(FieldObjectId id,std::string_view name,uint32_t&out,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted||!actual(id,d,s,e))return fail(e,"House Ray native signal emitter absent");
  if(name=="tree_entered"||name=="tree_exiting"||name=="tree_exited"||name=="ready"||name=="visibility_changed")out=0;
  else if(name=="child_entered_tree"||name=="child_exiting_tree")out=1;
  else return fail(e,"House Ray unsupported native signal declaration");
  e.clear();return true;
}
bool HouseReturnRayNative::source_object(uint32_t source,FieldObjectId&out,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=source_objects_.find(source);
  if(i==source_objects_.end()||!actual(i->second,d,s,e)||instances_.at(i->second).deleted)return fail(e,"House Ray actual source ObjectID absent");
  out=i->second;e.clear();return true;
}
bool HouseReturnRayNative::release_deleted(FieldObjectId id,std::string&e){
  auto i=instances_.find(id);
  if(i==instances_.end()||!i->second.deleted||tree_->state(id)||registry_->object_exists(id))
    return fail(e,"House Ray release precedes actual native/ObjectDB deletion");
  if(!signals_->release(id,e))return false;
  source_objects_.erase(i->second.source.id);instances_.erase(i);e.clear();return true;
}
}
