#include "house_return_kinematic_native.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <tuple>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string&e,const char*m){e=m;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){
  return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;
}
bool equal(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
bool equal(const FieldTransform&a,const FieldGeometryTransform&b){
  return equal(a[0],b.x)&&equal(a[1],b.y)&&equal(a[2],b.origin);
}
bool finite(Vec2 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::abs(a.x)<=1000000&&std::abs(a.y)<=1000000;}
Vec2 add(Vec2 a,Vec2 b){return {a.x+b.x,a.y+b.y};}
Vec2 sub(Vec2 a,Vec2 b){return {a.x-b.x,a.y-b.y};}
Vec2 mul(Vec2 a,float b){return {a.x*b,a.y*b};}
FieldGeometryTransform transform(const FieldTransform&t){return {t[0],t[1],t[2]};}
bool inverse(const FieldTransform&t,Vec2 p,Vec2&out){
  const float det=t[0].x*t[1].y-t[0].y*t[1].x;
  if(!std::isfinite(det)||det==0)return false;
  const auto d=sub(p,t[2]);out={(t[1].y*d.x-t[1].x*d.y)/det,(-t[0].y*d.x+t[0].x*d.y)/det};
  return finite(out);
}
bool contains(FieldGeometryBounds a,const MotionQueryBounds&b){
  return b.initialized&&a.minimum.x<=b.minimum.x&&a.minimum.y<=b.minimum.y&&
      a.maximum.x>=b.maximum.x&&a.maximum.y>=b.maximum.y;
}
}
bool HouseReturnKinematicNative::prepare(const HouseReturnSources&s,FieldNodeTreeRuntime&t,
    FieldGlobalRegistry&r,FieldObjectSignals&bus,FieldGeometrySpace&space,
    PodunkPlayerPhysicsWorld&world,HouseReturnGeometryNative&geometry,std::string&e){
  if(sources_||!s.valid()||!s.tree().valid()||!s.geometry().valid()||!s.reentry().valid()||
     !space.source()||space.source()->source_scene()!=s.reentry().source_scene()||
     world.geometry_space()!=&space||world.registry()!=&r||bus.registry()!=&r||!r.kernel()||
     s.tree().source_scene()!=s.reentry().target_scene()||
     s.geometry().source_scene()!=s.reentry().target_scene())
    return fail(e,"House Kinematic staging lacks checked closure/same live physics owners");
  const auto&g=s.geometry();std::set<uint32_t>bodies;
  for(uint32_t i=0;i<g.owner_count();++i){
    const auto o=g.owner(i);if(o.kind!=2)continue;
    const auto n=g.node(o.node);const auto*d=s.tree().record(n.stable_id);
    if(!d||!owns(*d)||d->script.empty()||d->script!=g.string(n.script)||d->script_sha!=n.script_sha256||
       !equal(d->local,n.local)||!equal(d->world,n.world)||!bodies.insert(n.stable_id).second||
       o.shape_count!=1||o.flags||o.platform_leave||o.space_override||o.priority||o.gravity||
       o.gravity_distance_scale||o.gravity_vector.x||o.gravity_vector.y||o.linear_damp||o.angular_damp||
       o.constant_linear_velocity.x||o.constant_linear_velocity.y||o.constant_angular_velocity||
       !std::isfinite(o.safe_margin)||o.safe_margin<0)
      return fail(e,"House Kinematic source native fields/shape ownership unsupported");
    const auto sh=g.shape(o.shape_first);const auto sn=g.node(sh.node);const auto*sd=s.tree().record(sn.stable_id);
    if(sh.owner!=i||sh.flags&2u||sh.part_count!=1||sh.kind!=1||
       g.geometry(sh.part_first).kind!=FieldGeometryKind::Rectangle||
       !sd||sd->native_class!="CollisionShape2D"||sd->parent!=d->id||
       !equal(sd->local,sn.local)||!equal(sd->local,sh.owner_transform))
      return fail(e,"House Kinematic native actor needs its actual checked rectangle shape");
  }
  for(const auto&d:s.tree().records())if(owns(d)&&!bodies.count(d.id))
    return fail(e,"House complete tree has a Kinematic body absent from checked geometry");
  if(bodies.empty()||s.reentry().tilemaps().empty())
    return fail(e,"House Kinematic complete geometry/zero-collision TileMap proof absent");
  // Reentry's checked loader binds every placed tile to its reviewed native
  // TileSet and rejects any collision part. No Podunk map queries are retained.
  sources_=&s;tree_=&t;registry_=&r;signals_=&bus;space_=&space;world_=&world;geometry_=&geometry;
  e.clear();return true;
}
bool HouseReturnKinematicNative::owns(const FieldNodeDescriptor&d)const{return d.native_class=="KinematicBody2D";}
bool HouseReturnKinematicNative::owns(FieldObjectId id)const{return instances_.count(id)!=0;}
bool HouseReturnKinematicNative::actual(FieldObjectId id,const FieldNodeDescriptor*&d,
    const FieldNodeState*&s,std::string&e)const{
  d=tree_?tree_->descriptor(id):nullptr;s=tree_?tree_->state(id):nullptr;FieldIdentity identity;
  if(!sources_||!sources_->valid()||poisoned_||!d||!s||!s->alive||
     !registry_->object_exists(id)||registry_->tree_owner(id).get()!=tree_||
     tree_->object_domain()!=registry_->kernel()||signals_->registry()!=registry_||
     !tree_->object_identity(id,identity)||!same(identity,sources_->tree().identity()))
    return fail(e,"House Kinematic actual ObjectDB/tree owner absent");
  const auto*source=sources_->tree().record(d->id);
  if(!source||source->path!=d->path||source->native_class!=d->native_class||source->class_index!=d->class_index||
     source->script!=d->script||source->script_sha!=d->script_sha||tree_->source_object(d->id)!=id)
    return fail(e,"House Kinematic actual descriptor differs from full source tree");
  e.clear();return true;
}
bool HouseReturnKinematicNative::construct(FieldObjectId id,const FieldNodeDescriptor&d,
    const FieldIdentity&identity,std::string&e){
  const FieldNodeDescriptor*ad;const FieldNodeState*s;
  if(!sources_||finished_||!owns(d)||instances_.count(id)||source_objects_.count(d.id)||
     !same(identity,sources_->tree().identity())||!actual(id,ad,s,e)||ad->id!=d.id||
     s->inside||s->parent||s->bound||s->ready_notified)
    return fail(e,"House Kinematic construction cursor/identity differs");
  const auto&g=sources_->geometry();Instance n;n.source=d.id;bool found=false;
  for(uint32_t i=0;i<g.owner_count();++i){const auto o=g.owner(i);
    if(o.kind==2&&g.node(o.node).stable_id==d.id){n.owner=i;n.node=o.node;n.shape=o.shape_first;n.properties=o;found=true;break;}}
  if(!found||!equal(d.local,g.node(n.node).local)||!tree_->set_transform_notification(id,false,true,e))
    return fail(e,"House Kinematic constructor native source row/transform differs");
  instances_.emplace(id,n);source_objects_.emplace(d.id,id);e.clear();return true;
}
bool HouseReturnKinematicNative::bind(FieldObjectId id,const FieldNodeBinding&b,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.bound||i->second.deleted||!actual(id,d,s,e)||
     !same(b.identity,sources_->tree().identity())||b.stable_id!=d->id||b.class_index!=d->class_index||
     b.native_class!=d->native_class||b.script_sha!=d->script_sha||!b.family||!b.capability)
    return fail(e,"House Kinematic combined native/source binding differs");
  i->second.bound=true;e.clear();return true;
}
bool HouseReturnKinematicNative::finish_factory(std::string&e){
  if(!sources_||finished_||!tree_->root()||tree_->lifecycle_pending())
    return fail(e,"House Kinematic factory boundary incomplete");
  for(const auto&d:sources_->tree().records())if(owns(d)){
    auto i=source_objects_.find(d.id);
    if(i==source_objects_.end()||!instances_.count(i->second)||tree_->source_object(d.id)!=i->second)
      return fail(e,"House Kinematic factory omitted an actual source body");
  }
  finished_=true;e.clear();return true;
}
bool HouseReturnKinematicNative::source_receipt(FieldObjectId id,const Instance&n,bool ready,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  if(!actual(id,d,s,e)||!n.script||!n.script_data||n.script->data()!=n.script_data||
     !n.script_data->valid()||n.script_data->source_pin()!=sources_->tree().identity().upstream_commit||
     !n.script->error().empty())
    return fail(e,"House Kinematic has no actual checked NPC source runtime");
  const auto&data=*n.script_data;bool scene=false,script=false;
  for(const auto&f:data.sources()){
    if(f.path==sources_->reentry().target_scene())scene=f.sha256==sources_->tree().identity().source_sha256;
    if(f.path==d->script)script=f.sha256==d->script_sha;
  }
  const FieldNpcDescriptor*desc=nullptr;const FieldNpcInstance*instance=nullptr;
  size_t expected=0;for(const auto&v:sources_->tree().records())if(owns(v))++expected;
  for(const auto&v:data.npcs())if(v.id==n.npc_id)desc=&v;
  for(const auto&v:n.script->npcs())if(v.id==n.npc_id)instance=&v;
  if(!scene||!script||data.npcs().size()!=expected||!desc||!instance||instance->index>=data.npcs().size()||
     data.npcs()[instance->index].id!=desc->id||desc->id!=n.source||desc->node!=d->path||
     desc->ready_ordinal!=d->ready||
     desc->safe_margin!=sources_->geometry().owner(n.owner).safe_margin||
     !equal(desc->position,sources_->geometry().node(n.node).world.origin)||desc->geometry.empty())
    return fail(e,"House NPC runtime source fingerprint/native body mapping differs");
  const auto&body=desc->geometry.front();const auto&g=sources_->geometry();
  const auto sh=g.shape(n.shape);const auto sn=g.node(sh.node);const auto primitive=g.geometry(sh.part_first);
  const float c=std::cos(body.rotation),q=std::sin(body.rotation);
  if(body.role!=1||body.kind!=1||body.layer!=g.owner(n.owner).layer||body.mask!=g.owner(n.owner).mask||
     !equal(body.offset,sn.local.origin)||body.value.x!=primitive.parameters[0]||body.value.y!=primitive.parameters[1]||
     !equal(sn.local.x,{c*body.scale.x,q*body.scale.x})||!equal(sn.local.y,{-q*body.scale.y,c*body.scale.y}))
    return fail(e,"House NPC source actor geometry differs from native export");
  if(ready&&(!instance->ready||instance->destroyed||instance->queued_free))
    return fail(e,"House NPC source Ready has not completed on its actual runtime instance");
  e.clear();return true;
}
bool HouseReturnKinematicNative::bind_source(FieldObjectId id,FieldNpcRuntime&runtime,uint32_t npc,std::string&e){
  auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted||i->second.script||!runtime.data())
    return fail(e,"House Kinematic source attachment lacks a concrete NPC owner");
  auto candidate=i->second;candidate.script=&runtime;candidate.script_data=runtime.data();candidate.npc_id=npc;
  if(!source_receipt(id,candidate,false,e))return false;
  i->second=candidate;e.clear();return true;
}
bool HouseReturnKinematicNative::live_space(std::string&e)const{
  const auto*s=tree_?tree_->state(root_):nullptr;const auto*d=tree_?tree_->descriptor(root_):nullptr;
  if(!sources_||!replaced_||poisoned_||space_->source()!=&sources_->geometry()||world_->geometry_space()!=space_||
     world_->registry()!=registry_||!old_tree_||old_tree_==tree_||old_tree_->root()!=old_root_||
     old_tree_->state(old_root_)||old_tree_->lifecycle_pending()||registry_->object_exists(old_root_)||
     root_!=tree_->root()||!s||!d||!s->alive||!s->bound||d->id!=sources_->tree().identity().scene_id||
     registry_->tree_owner(root_).get()!=tree_)
    return fail(e,"House Kinematic lacks actual same-space replacement/old-root deletion");
  const FieldNodeDescriptor*ad;const FieldNodeState*as;return actual(root_,ad,as,e);
}
bool HouseReturnKinematicNative::synchronize(FieldObjectId id,Instance&n,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  if(n.deleted||!actual(id,d,s,e)||!geometry_->observe_transform(id,e))return false;
  const auto sh=sources_->geometry().shape(n.shape);const auto sid=sources_->geometry().node(sh.node).stable_id;
  const auto shape=tree_->source_object(sid);
  // During allocation this child does not yet exist. After the factory it must
  // be the actual checked shape and retain this CollisionObject parent.
  if(shape){FieldTransform world;if(!tree_->world_transform(shape,world,e))return false;}
  if(replaced_){
    if(!live_space(e))return false;
    FieldGeometryNodeUpdate u;u.stable_id=n.source;u.fields=8;
    u.layer=n.entered?n.properties.layer:0;u.mask=n.entered?n.properties.mask:0;
    if(!space_->apply_updates({u},e))return false;
  }
  e.clear();return true;
}
bool HouseReturnKinematicNative::phase(FieldObjectId id,FieldTreePhase p,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted||!i->second.bound||!actual(id,d,s,e)||!s->bound)
    return fail(e,"House Kinematic native phase lacks actual bound body");
  auto&n=i->second;const auto abort=[&](){poisoned_=true;return false;};
  switch(p){
  case FieldTreePhase::EnterNative:
    if(!finished_||n.entered||!s->inside)return fail(e,"House Kinematic native Enter cursor differs");
    n.entered=true;
    if(activated_){if(!source_receipt(id,n,true,e))return abort();n.active=true;}
    if(!synchronize(id,n,e))return abort();break;
  case FieldTreePhase::ReadyNative:
    if(!n.entered||n.ready||!s->inside||!s->ready_notified)return fail(e,"House Kinematic native Ready precedes actual Enter");
    if(!synchronize(id,n,e))return abort();n.ready=true;break;
  case FieldTreePhase::ExitNative:
    if(!n.entered||!s->inside)return fail(e,"House Kinematic native Exit cursor differs");
    n.entered=false;n.active=false;if(!synchronize(id,n,e))return abort();break;
  case FieldTreePhase::TransformChanged:
  case FieldTreePhase::LocalTransformChanged:
    if(!synchronize(id,n,e))return abort();break;
  case FieldTreePhase::Deleting:
    if(n.entered||s->inside||s->parent||!s->children.empty())return fail(e,"House Kinematic delete precedes actual Exit/detach/child delete");
    if(!geometry_->observe_deleted(id,e))return abort();n.deleted=true;n.active=false;break;
  case FieldTreePhase::TreeEntered:
  case FieldTreePhase::TreeExiting:
    if(!n.entered||!s->inside)return fail(e,"House Kinematic native tree signal cursor differs");
    if(!signals_->emit(id,p==FieldTreePhase::TreeEntered?"tree_entered":"tree_exiting",{},e))return abort();break;
  case FieldTreePhase::TreeExited:
    if(n.entered||s->inside)return fail(e,"House Kinematic tree_exited precedes native Exit");
    if(!signals_->emit(id,"tree_exited",{},e))return abort();break;
  case FieldTreePhase::ReadySignal:
    if(!n.ready||!n.entered||!s->inside||!s->ready_notified||!source_receipt(id,n,true,e))return false;
    if(!signals_->emit(id,"ready",{},e))return abort();break;
  case FieldTreePhase::ChildEntered:
  case FieldTreePhase::ChildExiting:
    if(instances_.count(s->parent)&&!child_phase(id,p,e))return abort();break;
  case FieldTreePhase::VisibilityChanged:
    if(!signals_->emit(id,"visibility_changed",{},e))return abort();break;
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::Hide:
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::NodeAdded:
  case FieldTreePhase::NodeRemoved:
    // Actual Tree executes base Node/Canvas state. Visibility itself does not
    // disable collision: npc.gd's concrete receiver performs that source write.
    break;
  default:return fail(e,"House Kinematic native cannot execute source script/process phases");
  }
  e.clear();return true;
}
bool HouseReturnKinematicNative::child_phase(FieldObjectId child,FieldTreePhase p,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  if((p!=FieldTreePhase::ChildEntered&&p!=FieldTreePhase::ChildExiting)||!actual(child,d,s,e))return false;
  auto i=instances_.find(s->parent);
  if(i==instances_.end())return fail(e,"House Kinematic child signal has a different actual parent owner");
  const FieldNodeDescriptor*pd;const FieldNodeState*ps;
  if(i->second.deleted||!i->second.entered||!actual(s->parent,pd,ps,e)||!ps->inside||!s->inside)
    return fail(e,"House Kinematic child signal lacks actual entered parent/child");
  return signals_->emit(s->parent,p==FieldTreePhase::ChildEntered?"child_entered_tree":"child_exiting_tree",{FieldObjectRef{child}},e);
}
bool HouseReturnKinematicNative::declaration(FieldObjectId id,std::string_view name,uint32_t&out,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.deleted||!actual(id,d,s,e))return fail(e,"House Kinematic signal owner absent");
  if(name=="tree_entered"||name=="tree_exiting"||name=="tree_exited"||name=="ready"||name=="visibility_changed")out=0;
  else if(name=="child_entered_tree"||name=="child_exiting_tree")out=1;
  else return fail(e,"House Kinematic unsupported native signal declaration");
  e.clear();return true;
}
bool HouseReturnKinematicNative::replacement_updates(std::vector<FieldGeometryNodeUpdate>&out,std::string&e){
  if(!finished_||replaced_||poisoned_||tree_->lifecycle_pending())return fail(e,"House Kinematic replacement snapshot cursor differs");
  std::vector<FieldGeometryNodeUpdate>rows;
  for(auto&v:instances_){auto&n=v.second;if(n.deleted)continue;
    const FieldNodeDescriptor*d;const FieldNodeState*s;
    if(!actual(v.first,d,s,e)||!s->bound||!synchronize(v.first,n,e))return false;
    FieldGeometryNodeUpdate u;u.stable_id=n.source;u.fields=1|8;u.local=transform(s->local);
    u.layer=n.entered?n.properties.layer:0;u.mask=n.entered?n.properties.mask:0;rows.push_back(u);
  }
  out=std::move(rows);e.clear();return true;
}
bool HouseReturnKinematicNative::bind_replaced_space(const FieldNodeTreeRuntime&old,
    FieldObjectId old_root,FieldObjectId root,std::string&e){
  if(!finished_||replaced_||!old_root||old.root()!=old_root||old.state(old_root)||old.lifecycle_pending()||
     &old==tree_||old.object_domain()!=tree_->object_domain()||registry_->object_exists(old_root)||
     space_->source()!=&sources_->geometry()||root!=tree_->root())
    return fail(e,"House Kinematic binding precedes actual same-space replacement/deletion");
  old_tree_=&old;old_root_=old_root;root_=root;replaced_=true;
  if(!live_space(e)){replaced_=false;return false;}
  for(auto&v:instances_){auto&n=v.second;if(n.deleted)continue;
    if(!synchronize(v.first,n,e)||!space_->house_owner_rid(v.first,*sources_,n.rid,e)){poisoned_=true;return false;}}
  e.clear();return true;
}
FieldGeometryContact HouseReturnKinematicNative::contact(FieldObjectId id,const Instance&n)const{
  const auto sh=sources_->geometry().shape(n.shape);
  return {n.owner,n.shape,0,n.source,0,id,tree_->source_object(sources_->geometry().node(sh.node).stable_id)};
}
bool HouseReturnKinematicNative::admitted(FieldObjectId id,const Instance&n,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;FieldPhysicsRid rid;
  if(n.deleted||!n.active||!n.entered||!n.ready||!live_space(e)||!actual(id,d,s,e)||
     !s->inside||!s->bound||s->queued||!s->ready_notified||s->world_dirty||
     world_->tree()!=tree_||tree_->lifecycle_pending()||!source_receipt(id,n,true,e)||
     !space_->house_owner_rid(id,*sources_,rid,e)||rid.space!=n.rid.space||rid.handle!=n.rid.handle)
    return fail(e,"House Kinematic activation lacks actual native/script/Space RID receipts");
  const auto c=contact(id,n);const FieldNodeDescriptor*sd;const FieldNodeState*ss;FieldObjectId held;
  if(!c.actual_shape||!actual(c.actual_shape,sd,ss,e)||ss->parent!=id||!ss->inside||!ss->bound||
     !ss->ready_notified||ss->world_dirty||!geometry_->source_object(sd->id,held,e)||held!=c.actual_shape)
    return fail(e,"House Kinematic activation lacks actual native child shape owner");
  e.clear();return true;
}
bool HouseReturnKinematicNative::activate_sources(std::string&e){
  if(!finished_||activated_||!live_space(e)||world_->tree()!=tree_||tree_->lifecycle_pending())
    return fail(e,"House Kinematic source activation boundary differs");
  for(auto&v:instances_){auto&n=v.second;if(n.deleted)continue;
    n.active=true;std::vector<FieldGeometryContact>parts;
    if(!admitted(v.first,n,e)||!body_shapes(v.first,parts,e)){
      for(auto&x:instances_)x.second.active=false;return false;}}
  activated_=true;e.clear();return true;
}
bool HouseReturnKinematicNative::begin_physics(uint64_t epoch,float delta,std::string&e){
  if(!activated_||!live_space(e)||!epoch||epoch<=epoch_||!std::isfinite(delta)||delta<0||delta>60)
    return fail(e,"House Kinematic actual physics frame order/delta rejected");
  for(const auto&v:instances_)if(!v.second.deleted&&v.second.entered&&!admitted(v.first,v.second,e))return false;
  epoch_=epoch;delta_=delta;e.clear();return true;
}
bool HouseReturnKinematicNative::owner_rid(FieldObjectId id,FieldPhysicsRid&out,std::string&e)const{
  auto i=instances_.find(id);FieldPhysicsRid rid;
  if(i==instances_.end()||i->second.deleted||!live_space(e)||!space_->house_owner_rid(id,*sources_,rid,e)||
     rid.space!=i->second.rid.space||rid.handle!=i->second.rid.handle)
    return fail(e,"House Kinematic actual retained physics RID unavailable");
  out=rid;e.clear();return true;
}
bool HouseReturnKinematicNative::body_shapes(FieldObjectId id,std::vector<FieldGeometryContact>&out,std::string&e)const{
  auto i=instances_.find(id);
  if(i==instances_.end()||!admitted(id,i->second,e))return false;
  const auto c=contact(id,i->second);bool disabled;
  if(!geometry_->shape_disabled(c.actual_shape,disabled,e))return false;
  std::vector<FieldGeometryContact>parts;
  if(!disabled){FieldGeometryActor actor;FieldGeometryOwner o;FieldGeometryShape sh;FieldPhysicsRid rid;
    if(!space_->live_geometry(c,actor,o,sh,e)||o.kind!=2||o.layer!=i->second.properties.layer||o.mask!=i->second.properties.mask||
       !space_->physics_rid(c,rid,e)||rid.space!=i->second.rid.space||rid.handle!=i->second.rid.handle)
      return fail(e,"House Kinematic live part differs from actual native owner/RID");
    parts.push_back(c);
  }
  out=std::move(parts);e.clear();return true;
}
bool HouseReturnKinematicNative::set_world_position(FieldObjectId id,Vec2 position,std::string&e){
  auto i=instances_.find(id);const FieldNodeDescriptor*d;const FieldNodeState*s;
  if(i==instances_.end()||i->second.deleted||!finite(position)||!actual(id,d,s,e))return fail(e,"House Kinematic source position setter rejected");
  auto local=s->local;
  if(s->canvas_parent){FieldTransform parent;
    if(!tree_->world_transform(s->canvas_parent,parent,e)||!inverse(parent,position,local[2]))
      return fail(e,"House Kinematic source position parent transform rejected");
  }else local[2]=position;
  if(!tree_->set_local(id,local,e))return false;
  if(!synchronize(id,i->second,e)){poisoned_=true;return false;}
  e.clear();return true;
}
bool HouseReturnKinematicNative::set_collision(FieldObjectId id,uint32_t layer,uint32_t mask,std::string&e){
  auto i=instances_.find(id);const FieldNodeDescriptor*d;const FieldNodeState*s;
  if(i==instances_.end()||i->second.deleted||!actual(id,d,s,e))return fail(e,"House Kinematic collision setter lacks actual native owner");
  const auto prior=i->second.properties;i->second.properties.layer=layer;i->second.properties.mask=mask;
  if(!synchronize(id,i->second,e)){i->second.properties=prior;return false;}
  e.clear();return true;
}
bool HouseReturnKinematicNative::move_and_slide(FieldObjectId id,Vec2 velocity,float dt,
    Vec2&out_position,Vec2&out_velocity,std::string&e){
  auto i=instances_.find(id);
  if(i==instances_.end()||!epoch_||dt!=delta_||!finite(velocity)||!admitted(id,i->second,e))
    return fail(e,"House NPC move_and_slide lacks actual source/physics frame admission");
  const auto&n=i->second;FieldTransform world;
  if(!tree_->world_transform(id,world,e))return false;
  const auto position=world[2];const auto c=contact(id,n);bool disabled;
  if(!geometry_->shape_disabled(c.actual_shape,disabled,e))return false;
  SlideResult solved;
  if(disabled){solved={add(position,mul(velocity,dt)),velocity};
    if(!finite(solved.position))return fail(e,"House disabled native actor motion overflow");
  }else{
    FieldGeometryActor actor;FieldGeometryOwner own;FieldGeometryShape shape;
    if(!space_->live_geometry(c,actor,own,shape,e))return false;
    actor.transform.origin=sub(actor.transform.origin,position);PodunkSolidShape moving;
    if(!podunk_prepare_shape(actor,moving,e))return false;
    const auto travel=mul(velocity,dt);const float margin=n.properties.safe_margin;
    FieldGeometryBounds bounds{{position.x+moving.minimum.x+std::min(0.0f,travel.x)-margin,
        position.y+moving.minimum.y+std::min(0.0f,travel.y)-margin},
        {position.x+moving.maximum.x+std::max(0.0f,travel.x)+margin,
        position.y+moving.maximum.y+std::max(0.0f,travel.y)+margin}};
    bool closed=false;
    // Same native query-closure bound as the existing NPC/Player solver.
    for(unsigned attempt=0;attempt<16;++attempt){
      FieldGeometryFilter f;f.bodies=true;f.areas=false;f.bilateral_mask=true;
      f.layer_mask=n.properties.mask;f.reciprocal_layer=n.properties.layer;
      std::vector<FieldGeometryContact>hits;
      if(!space_->candidates(bounds,f,space_->indexed_instance_count(),hits,e))return false;
      std::vector<PodunkSolidShape>solids;
      for(const auto&hit:hits){if(hit.actual_owner==id)continue;
        FieldGeometryActor other;FieldGeometryOwner o;FieldGeometryShape sh;
        if(!space_->live_geometry(hit,other,o,sh,e))return false;
        const auto owner=hit.actual_owner;auto t=registry_->tree_owner(owner);
        const auto*s=t?t->state(owner):nullptr;const auto*d=t?t->descriptor(owner):nullptr;
        if(!owner||!registry_->object_exists(owner)||!s||!d||!s->alive||!s->inside||!s->bound||s->queued||
           d->id!=hit.stable_id||o.kind==3||(sh.flags&2u)||o.constant_linear_velocity.x||
           o.constant_linear_velocity.y||o.constant_angular_velocity)
          return fail(e,"House NPC motion collider lacks actual supported native owner");
        auto npc=instances_.find(owner);
        if(npc!=instances_.end()&&!admitted(owner,npc->second,e))return false;
        PodunkSolidShape solid;if(!podunk_prepare_shape(other,solid,e))return false;solids.push_back(std::move(solid));
      }
      MotionQueryBounds visited;SlideResult candidate;
      if(!podunk_solve_motion(moving,solids,position,velocity,dt,margin,candidate,visited,e))return false;
      if(contains(bounds,visited)){solved=candidate;closed=true;break;}
      if(!visited.initialized)return fail(e,"House NPC native motion query closure absent");
      bounds.minimum={std::min(bounds.minimum.x,visited.minimum.x),std::min(bounds.minimum.y,visited.minimum.y)};
      bounds.maximum={std::max(bounds.maximum.x,visited.maximum.x),std::max(bounds.maximum.y,visited.maximum.y)};
    }
    if(!closed)return fail(e,"House NPC native motion query closure exceeded capacity");
  }
  if(!set_world_position(id,solved.position,e))return false;
  out_position=solved.position;out_velocity=solved.velocity;e.clear();return true;
}
bool HouseReturnKinematicNative::sample_area_pairs(std::vector<AreaBodyPair>&out,std::string&e)const{
  if(!activated_||!live_space(e)||world_->tree()!=tree_)return fail(e,"House NPC Area sampling lacks actual activated source world");
  std::vector<AreaBodyPair>pairs;
  for(const auto&entry:instances_){const auto&n=entry.second;if(n.deleted||!n.entered)continue;
    std::vector<FieldGeometryContact>parts;if(!body_shapes(entry.first,parts,e))return false;
    for(const auto&self:parts){FieldGeometryActor a;FieldGeometryOwner own;FieldGeometryShape sh;
      if(!space_->live_geometry(self,a,own,sh,e))return false;
      FieldGeometryFilter f;f.bodies=false;f.areas=true;f.bilateral_mask=true;
      f.layer_mask=own.mask;f.reciprocal_layer=own.layer;
      std::vector<FieldGeometryContact>hits;
      if(!space_->overlap_actor(a,f,space_->indexed_instance_count(),hits,e))return false;
      for(const auto&hit:hits){FieldGeometryActor b;FieldGeometryOwner area;FieldGeometryShape shape;
        if(!space_->live_geometry(hit,b,area,shape,e))return false;
        if(area.kind!=4)return fail(e,"House NPC Area sampler received a non-Area native part");
        if(!(area.flags&2u))continue;
        const auto watcher=hit.actual_owner;auto t=registry_->tree_owner(watcher);
        const auto*s=t?t->state(watcher):nullptr;const auto*d=t?t->descriptor(watcher):nullptr;
        if(!watcher||!registry_->object_exists(watcher)||!s||!d||!s->inside||!s->bound||!s->alive||
           s->queued||!s->ready_notified||s->world_dirty||d->id!=hit.stable_id||d->native_class!="Area2D")
          return fail(e,"House NPC Area sampler lacks actual native watcher owner");
        FieldPhysicsRid rid;if(!owner_rid(entry.first,rid,e))return false;
        pairs.push_back({watcher,entry.first,rid,self.native_shape_index,hit.native_shape_index});
      }
    }
  }
  const auto key=[](const AreaBodyPair&p){return std::tie(p.area,p.body_rid.handle,p.body_shape,p.area_shape,p.body);};
  std::sort(pairs.begin(),pairs.end(),[&](const auto&a,const auto&b){return key(a)<key(b);});
  pairs.erase(std::unique(pairs.begin(),pairs.end(),[&](const auto&a,const auto&b){return key(a)==key(b);}),pairs.end());
  out=std::move(pairs);e.clear();return true;
}
bool HouseReturnKinematicNative::source_object(uint32_t source,FieldObjectId&out,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=source_objects_.find(source);
  if(i==source_objects_.end()||!actual(i->second,d,s,e)||instances_.at(i->second).deleted)
    return fail(e,"House Kinematic actual source ObjectID absent");
  out=i->second;e.clear();return true;
}
bool HouseReturnKinematicNative::release_deleted(FieldObjectId id,std::string&e){
  auto i=instances_.find(id);
  if(i==instances_.end()||!i->second.deleted||tree_->state(id)||registry_->object_exists(id)||
     (i->second.rid.handle&&space_->rid_alive(i->second.rid)))
    return fail(e,"House Kinematic release precedes actual Tree/ObjectDB/RID deletion");
  if(!signals_->release(id,e))return false;
  source_objects_.erase(i->second.source);instances_.erase(i);e.clear();return true;
}
}
