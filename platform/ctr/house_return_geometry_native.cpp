#include "house_return_geometry_native.hpp"
#include <algorithm>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string&e,const char*why){e=why;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){
  return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;
}
bool same(const FieldTransform&a,const FieldGeometryTransform&b){
  return a[0].x==b.x.x&&a[0].y==b.x.y&&a[1].x==b.y.x&&a[1].y==b.y.y&&
         a[2].x==b.origin.x&&a[2].y==b.origin.y;
}
FieldGeometryTransform geometry_transform(const FieldTransform&t){return {t[0],t[1],t[2]};}
bool area_signal(std::string_view name,uint32_t&arguments){
  if(name=="body_entered"||name=="body_exited"||name=="area_entered"||name=="area_exited"){
    arguments=1;return true;
  }
  if(name=="body_shape_entered"||name=="body_shape_exited"||
     name=="area_shape_entered"||name=="area_shape_exited"){
    arguments=4;return true;
  }
  return false;
}
}
bool HouseReturnGeometryNative::prepare(const HouseReturnSources&s,FieldNodeTreeRuntime&t,
    FieldGlobalRegistry&r,FieldObjectSignals&bus,FieldGeometrySpace&space,
    PodunkPlayerPhysicsWorld&world,std::string&e){
  if(sources_||!s.valid()||!s.tree().valid()||!s.geometry().valid()||!s.reentry().valid()||
     !space.source()||world.geometry_space()!=&space||world.registry()!=&r||bus.registry()!=&r||
     !r.kernel()||s.tree().source_scene()!=s.reentry().target_scene()||
     s.geometry().source_scene()!=s.reentry().target_scene()||
     space.source()->source_scene()!=s.reentry().source_scene())
    return fail(e,"House geometry native staging lacks loaded closure/same live Space owners");
  const auto&g=s.geometry();
  for(uint32_t i=0;i<g.owner_count();++i){
    const auto owner=g.owner(i);const auto node=g.node(owner.node);const auto*d=s.tree().record(node.stable_id);
    if(!d||d->native_class!=g.string(node.class_name))
      return fail(e,"House geometry native owner source class differs");
    if(owner.kind==1&&(owner.constant_linear_velocity.x||owner.constant_linear_velocity.y||owner.constant_angular_velocity))
      return fail(e,"House StaticBody native moving-platform consumer unavailable");
    if(owner.kind==4&&(owner.space_override||(owner.flags&8u)))
      return fail(e,"House Area native force/audio override consumer unavailable");
    // Kinematic/Rigid owners belong to their real external implementation.
    // Their child shapes can be held here; this check does not bind the owner.
    if(owner.kind<1||owner.kind>4)return fail(e,"House geometry unknown native collision owner");
  }
  for(uint32_t i=0;i<g.shape_count();++i){
    const auto shape=g.shape(i);const auto node=g.node(shape.node);const auto*d=s.tree().record(node.stable_id);
    if(!d||(d->native_class!="CollisionShape2D"&&d->native_class!="CollisionPolygon2D")||
       shape.flags&2u)return fail(e,"House native shape/one-way consumer unavailable");
  }
  sources_=&s;tree_=&t;registry_=&r;signals_=&bus;space_=&space;world_=&world;
  e.clear();return true;
}
bool HouseReturnGeometryNative::owns(const FieldNodeDescriptor&d)const{
  return d.native_class=="StaticBody2D"||d.native_class=="Area2D"||
         d.native_class=="CollisionShape2D"||d.native_class=="CollisionPolygon2D";
}
bool HouseReturnGeometryNative::owns(FieldObjectId id)const{return instances_.count(id)!=0;}
bool HouseReturnGeometryNative::actual(FieldObjectId id,const FieldNodeDescriptor*&d,
    const FieldNodeState*&s,std::string&e)const{
  d=tree_?tree_->descriptor(id):nullptr;s=tree_?tree_->state(id):nullptr;FieldIdentity identity;
  if(!sources_||!sources_->valid()||poisoned_||!d||!s||!s->alive||
     !registry_->object_exists(id)||registry_->tree_owner(id).get()!=tree_||
     tree_->object_domain()!=registry_->kernel()||signals_->registry()!=registry_||
     !tree_->object_identity(id,identity)||!same(identity,sources_->tree().identity()))
    return fail(e,"House native geometry actual ObjectDB/tree source owner absent");
  const auto*source=sources_->tree().record(d->id);
  if(!source||source->path!=d->path||source->class_index!=d->class_index||
     source->native_class!=d->native_class||source->script!=d->script||source->script_sha!=d->script_sha)
    return fail(e,"House native geometry actual descriptor differs from complete source tree");
  e.clear();return true;
}
bool HouseReturnGeometryNative::source_geometry(uint32_t id,uint32_t&out,std::string&e)const{
  if(!sources_)return fail(e,"House native geometry source unavailable");
  for(uint32_t i=0;i<sources_->geometry().node_count();++i)
    if(sources_->geometry().node(i).stable_id==id){out=i;return true;}
  return fail(e,"House native node has no checked geometry row");
}
bool HouseReturnGeometryNative::construct(FieldObjectId id,const FieldNodeDescriptor&d,
    const FieldIdentity&identity,std::string&e){
  const FieldNodeDescriptor*actual_d;const FieldNodeState*s;
  if(!sources_||finished_||!owns(d)||instances_.count(id)||source_objects_.count(d.id)||
     !same(identity,sources_->tree().identity())||!actual(id,actual_d,s,e)||
     actual_d->id!=d.id||s->inside||s->parent||s->bound||s->ready_notified)
    return fail(e,"House native geometry construction cursor/identity differs");
  const auto&g=sources_->geometry();Instance n;n.source=d.id;
  if(!source_geometry(d.id,n.node,e))return false;
  const auto node=g.node(n.node);
  if(!same(d.local,node.local))return fail(e,"House native initial transform differs from source geometry");
  if(d.native_class=="StaticBody2D"||d.native_class=="Area2D"){
    n.kind=d.native_class=="StaticBody2D"?Kind::StaticBody:Kind::Area;
    for(uint32_t i=0;i<g.owner_count();++i)if(g.owner(i).node==n.node){n.owner=i;break;}
    if(n.owner==UINT32_MAX)return fail(e,"House native CollisionObject owner row missing");
    n.properties=g.owner(n.owner);
    if(n.properties.kind!=(n.kind==Kind::Area?4u:1u))return fail(e,"House native owner kind differs");
    // CollisionObject2D registers its real global transform notification.
    if(!tree_->set_transform_notification(id,false,true,e))return false;
  }else{
    n.kind=Kind::Shape;
    for(uint32_t i=0;i<g.shape_count();++i)if(g.shape(i).node==n.node){n.shape=i;break;}
    if(n.shape==UINT32_MAX)return fail(e,"House native shape owner row missing");
    const auto shape=g.shape(n.shape);n.owner=shape.owner;n.disabled=bool(shape.flags&1u);
    n.cached_shape_transform=shape.cached_before_enter_tree;
    // Shape nodes update their CollisionObject shape-owner transform when
    // local Transform2D changes; parent global motion is owned by the body.
    if(!tree_->set_transform_notification(id,true,true,e))return false;
  }
  instances_.emplace(id,n);source_objects_.emplace(d.id,id);e.clear();return true;
}
bool HouseReturnGeometryNative::bind(FieldObjectId id,const FieldNodeBinding&b,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto it=instances_.find(id);
  if(it==instances_.end()||it->second.deleted||it->second.bound||!actual(id,d,s,e)||
     !same(b.identity,sources_->tree().identity())||b.stable_id!=d->id||b.class_index!=d->class_index||
     b.native_class!=d->native_class||b.script_sha!=d->script_sha||!b.family||!b.capability)
    return fail(e,"House geometry native combined actual binding differs");
  // This validates the actual script/native owner's receipt supplied to Tree.
  // No scripted ancestor is bound in FieldGeometrySpace by this method.
  it->second.bound=true;e.clear();return true;
}
bool HouseReturnGeometryNative::finish_factory(std::string&e){
  if(!sources_||finished_||!tree_->root()||tree_->lifecycle_pending())
    return fail(e,"House native geometry factory boundary differs");
  for(const auto&d:sources_->tree().records())if(owns(d)){
    auto i=source_objects_.find(d.id);
    if(i==source_objects_.end()||!instances_.count(i->second)||tree_->source_object(d.id)!=i->second)
      return fail(e,"House native factory omitted a source collision node");
  }
  finished_=true;e.clear();return true;
}
bool HouseReturnGeometryNative::live_space(std::string&e)const{
  const auto*s=tree_?tree_->state(root_):nullptr;
  if(!sources_||!replaced_||poisoned_||space_->source()!=&sources_->geometry()||
     world_->geometry_space()!=space_||world_->registry()!=registry_||!old_tree_||
     old_tree_==tree_||old_tree_->root()!=old_root_||old_tree_->state(old_root_)||
     old_tree_->lifecycle_pending()||registry_->object_exists(old_root_)||
     root_!=tree_->root()||!s||!s->alive||registry_->tree_owner(root_).get()!=tree_)
    return fail(e,"House native geometry lacks actual same-space replacement/deletion receipt");
  const FieldNodeDescriptor*d;const FieldNodeState*state;
  return actual(root_,d,state,e);
}
bool HouseReturnGeometryNative::synchronize(FieldObjectId id,Instance&n,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  if(n.deleted||!actual(id,d,s,e))return false;
  FieldTransform world;
  if(!tree_->world_transform(id,world,e))return false;
  if(n.kind==Kind::Shape){
    const auto&g=sources_->geometry();const auto shape=g.shape(n.shape);
    const auto owner=g.node(g.owner(shape.owner).node);
    const auto parent=tree_->source_object(owner.stable_id);const auto*p=tree_->state(parent);
    if(n.entered&&(!parent||!p||!p->bound||!p->inside||s->parent!=parent))
      return fail(e,"House native shape Enter requires its real CollisionObject parent owner");
    n.shape_parent=n.entered?parent:0;n.cached_shape_transform=geometry_transform(s->local);
  }
  if(replaced_){
    if(!live_space(e))return false;
    FieldGeometryNodeUpdate u;u.stable_id=n.source;u.fields=1;u.local=geometry_transform(s->local);
    if(n.kind==Kind::Shape){u.fields|=4;u.disabled=n.disabled||!n.entered;}
    else{u.fields|=8;u.layer=n.properties.layer;u.mask=n.properties.mask;
      if(n.kind==Kind::Area){u.fields|=16;u.monitoring=n.entered&&bool(n.properties.flags&2u);u.monitorable=n.entered&&bool(n.properties.flags&4u);}}
    if(!space_->apply_updates({u},e))return false;
  }
  e.clear();return true;
}
bool HouseReturnGeometryNative::phase(FieldObjectId id,FieldTreePhase phase,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto it=instances_.find(id);
  if(it==instances_.end()||it->second.deleted||!it->second.bound||!actual(id,d,s,e)||!s->bound)
    return fail(e,"House native geometry phase lacks actual bound owner");
  auto&n=it->second;
  const auto abort=[&](){poisoned_=true;return false;};
  switch(phase){
  case FieldTreePhase::EnterNative:
    if(!finished_||n.entered||!s->inside)return fail(e,"House native geometry Enter cursor differs");
    n.entered=true;
    if(!synchronize(id,n,e))return abort();
    break;
  case FieldTreePhase::ReadyNative:
    if(!n.entered||n.ready||!s->inside||!s->ready_notified)
      return fail(e,"House native geometry Ready lacks actual Enter/source Ready cursor");
    if(!synchronize(id,n,e))return abort();
    n.ready=true;
    break;
  case FieldTreePhase::ExitNative:
    if(!n.entered||!s->inside)return fail(e,"House native geometry Exit cursor differs");
    if(n.monitored&&!world_->static_monitor_exit(id,e))return abort();
    n.monitored=false;n.entered=false;monitors_active_=false;
    if(!synchronize(id,n,e))return abort();
    break;
  case FieldTreePhase::TransformChanged:
  case FieldTreePhase::LocalTransformChanged:
    if(!synchronize(id,n,e))return abort();
    break;
  case FieldTreePhase::Deleting:
    if(n.entered||n.monitored||s->inside||s->parent||!s->children.empty())
      return fail(e,"House native geometry deletion precedes actual Exit/detach/child deletion");
    if(!observe_deleted(id,e))return abort();
    n.deleted=true;
    break;
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
    if(n.kind==Kind::Shape&&!synchronize(id,n,e))return abort();
    break;
  case FieldTreePhase::TreeEntered:
    if(!n.entered||!s->inside)return fail(e,"House native Node tree_entered cursor differs");
    if(!signals_->emit(id,"tree_entered",{},e))return abort();
    break;
  case FieldTreePhase::TreeExiting:
    if(!n.entered||!s->inside)return fail(e,"House native Node tree_exiting cursor differs");
    if(!signals_->emit(id,"tree_exiting",{},e))return abort();
    break;
  case FieldTreePhase::TreeExited:
    if(n.entered||s->inside)return fail(e,"House native Node tree_exited cursor differs");
    if(!signals_->emit(id,"tree_exited",{},e))return abort();
    break;
  case FieldTreePhase::ReadySignal:
    if(!n.entered||!n.ready||!s->inside||!s->ready_notified)
      return fail(e,"House native ready signal precedes actual native/source Ready");
    if(!signals_->emit(id,"ready",{},e))return abort();
    break;
  case FieldTreePhase::ChildEntered:
  case FieldTreePhase::ChildExiting:
    if(s->parent&&instances_.count(s->parent)&&
       !signals_->emit(s->parent,phase==FieldTreePhase::ChildEntered?"child_entered_tree":"child_exiting_tree",
          {FieldObjectRef{id}},e))return abort();
    // A foreign parent has its own actual base Node signal owner.
    break;
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::NodeAdded:
  case FieldTreePhase::NodeRemoved:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::VisibilityChanged:
  case FieldTreePhase::Hide:
    // Base Node/Canvas state is executed by the actual Tree. Visibility never
    // disables a collision shape or substitutes a physical Enter/Exit.
    break;
  default:return fail(e,"House native geometry cannot execute script/process/source signal phases");
  }
  e.clear();return true;
}
bool HouseReturnGeometryNative::observe_transform(FieldObjectId id,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;uint32_t node;
  if(!actual(id,d,s,e)||!source_geometry(d->id,node,e))return false;
  const auto owned=instances_.find(id);
  if(owned!=instances_.end())return synchronize(id,owned->second,e);
  FieldTransform world;if(!tree_->world_transform(id,world,e))return false;
  if(replaced_){
    if(!live_space(e))return false;
    FieldGeometryNodeUpdate u;u.stable_id=d->id;u.fields=1;u.local=geometry_transform(s->local);
    if(!space_->apply_updates({u},e))return false;
  }
  e.clear();return true;
}
bool HouseReturnGeometryNative::observe_deleted(FieldObjectId id,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;uint32_t node;
  if(!actual(id,d,s,e)||!source_geometry(d->id,node,e)||s->inside||s->parent||!s->children.empty()||
     deleted_nodes_.count(d->id))return fail(e,"House geometry deletion observation lacks actual native predelete cursor");
  if(replaced_){
    if(!live_space(e))return false;
    FieldGeometryNodeUpdate u;u.stable_id=d->id;u.fields=2;u.deleted=true;
    if(!space_->apply_updates({u},e))return false;
  }
  deleted_nodes_.emplace(d->id,id);e.clear();return true;
}
bool HouseReturnGeometryNative::replacement_updates(std::vector<FieldGeometryNodeUpdate>&out,std::string&e){
  if(!finished_||replaced_||poisoned_||tree_->lifecycle_pending())
    return fail(e,"House native geometry replacement observations have incomplete lifecycle");
  const auto&g=sources_->geometry();std::vector<FieldGeometryNodeUpdate>updates;
  for(uint32_t i=0;i<g.node_count();++i){
    const auto source=g.node(i);const auto id=tree_->source_object(source.stable_id);
    FieldGeometryNodeUpdate u;u.stable_id=source.stable_id;
    if(!id){
      auto deleted=deleted_nodes_.find(source.stable_id);
      if(deleted==deleted_nodes_.end()||tree_->state(deleted->second)||registry_->object_exists(deleted->second))
        return fail(e,"House replacement missing node lacks actual native/ObjectDB deletion");
      u.fields=2;u.deleted=true;updates.push_back(u);continue;
    }
    const FieldNodeDescriptor*d;const FieldNodeState*s;
    if(!actual(id,d,s,e)||!s->bound||!observe_transform(id,e))return false;
    u.fields=1;u.local=geometry_transform(s->local);
    auto n=instances_.find(id);
    if(n!=instances_.end()){
      if(n->second.kind==Kind::Shape){u.fields|=4;u.disabled=n->second.disabled||!n->second.entered;}
      else{u.fields|=8;u.layer=n->second.properties.layer;u.mask=n->second.properties.mask;
        if(n->second.kind==Kind::Area){u.fields|=16;u.monitoring=n->second.entered&&bool(n->second.properties.flags&2u);u.monitorable=n->second.entered&&bool(n->second.properties.flags&4u);}}
    }
    updates.push_back(u);
  }
  out=std::move(updates);e.clear();return true;
}
bool HouseReturnGeometryNative::bind_replaced_space(const FieldNodeTreeRuntime&old_tree,
    FieldObjectId old_root,FieldObjectId root,std::string&e){
  const auto*s=tree_?tree_->state(root):nullptr;const auto*d=tree_?tree_->descriptor(root):nullptr;
  if(!finished_||replaced_||poisoned_||space_->source()!=&sources_->geometry()||world_->geometry_space()!=space_||
     !s||!d||!s->bound||!s->alive||root!=tree_->root()||d->id!=sources_->tree().identity().scene_id||
     registry_->tree_owner(root).get()!=tree_||!registry_->object_exists(root)||
     &old_tree==tree_||old_tree.object_domain()!=tree_->object_domain()||!old_root||
     old_tree.root()!=old_root||old_tree.state(old_root)||old_tree.lifecycle_pending()||registry_->object_exists(old_root))
    return fail(e,"House native bind precedes actual source replacement/old scene deletion");
  old_tree_=&old_tree;old_root_=old_root;root_=root;replaced_=true;
  for(auto&row:instances_){auto&n=row.second;if(n.deleted)continue;
    if(!synchronize(row.first,n,e)){poisoned_=true;return false;}
    if(n.kind!=Kind::Shape&&!space_->house_owner_rid(row.first,*sources_,n.rid,e)){poisoned_=true;return false;}
  }
  e.clear();return true;
}
bool HouseReturnGeometryNative::activate_monitors(std::string&e){
  if(!live_space(e)||monitors_active_||world_->tree()!=tree_||tree_->lifecycle_pending())
    return fail(e,"House Area activation requires actual completed same Tree/player world");
  for(uint32_t i=0;i<sources_->geometry().node_count();++i){
    const auto id=tree_->source_object(sources_->geometry().node(i).stable_id);
    if(!id)continue;
    const auto*s=tree_->state(id);
    if(!s||!s->inside||!s->bound||!s->ready_notified||!observe_transform(id,e))
      return fail(e,"House geometry ancestor native/script lifecycle incomplete");
  }
  for(const auto&row:instances_)if(!row.second.deleted&&(!row.second.entered||!row.second.ready))
    return fail(e,"House native collision owner lacks actual Enter/Ready");
  if(!world_->bind_house_geometry(*sources_,*old_tree_,old_root_,root_,e))return false;
  for(auto&row:instances_){auto&n=row.second;
    if(!n.deleted&&n.kind==Kind::Area&&!n.monitored){
      if(!world_->admit_static_monitor(row.first,n.owner,e)){poisoned_=true;return false;}
      n.monitored=true;
    }
  }
  monitors_active_=true;return physics_admitted(e);
}
bool HouseReturnGeometryNative::physics_admitted(std::string&e)const{
  if(!live_space(e)||!monitors_active_||world_->tree()!=tree_||tree_->lifecycle_pending())
    return fail(e,"House native collision/monitor world activation incomplete");
  for(const auto&row:instances_){const auto&n=row.second;
    if(n.deleted)continue;
    const FieldNodeDescriptor*d;const FieldNodeState*s;
    if(!actual(row.first,d,s,e)||!s->inside||!s->bound||s->world_dirty||!s->ready_notified||
       !n.entered||!n.ready||(n.kind==Kind::Area&&!n.monitored))
      return fail(e,"House actual native collision lifecycle/transform receipt incomplete");
    if(n.kind!=Kind::Shape){FieldPhysicsRid rid;
      if(!space_->house_owner_rid(row.first,*sources_,rid,e)||rid.space!=n.rid.space||rid.handle!=n.rid.handle)
        return fail(e,"House native collision owner no longer retains its actual RID");
    }else if(!n.disabled){
      const auto&g=sources_->geometry();const auto shape=g.shape(n.shape);const auto owner=g.owner(shape.owner);
      uint32_t native_index=0;
      for(uint32_t previous=owner.shape_first;previous<n.shape;++previous)native_index+=g.shape(previous).part_count;
      for(uint32_t part=0;part<shape.part_count;++part){
        FieldGeometryContact contact{shape.owner,n.shape,part,g.node(owner.node).stable_id,
            native_index+part,n.shape_parent,row.first};
        FieldGeometryActor actor;FieldGeometryOwner actual_owner;FieldGeometryShape actual_shape;
        if(!space_->live_geometry(contact,actor,actual_owner,actual_shape,e))
          return fail(e,"House collision part lacks actual geometry/script ancestor admission");
      }
    }
  }
  e.clear();return true;
}
bool HouseReturnGeometryNative::set_disabled(FieldObjectId id,bool value,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.kind!=Kind::Shape||i->second.deleted||!actual(id,d,s,e))
    return fail(e,"House disabled setter requires actual native shape");
  const auto prior=i->second.disabled;i->second.disabled=value;
  if(!synchronize(id,i->second,e)){i->second.disabled=prior;return false;}
  e.clear();return true;
}
bool HouseReturnGeometryNative::shape_disabled(FieldObjectId id,bool&out,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.kind!=Kind::Shape||i->second.deleted||!actual(id,d,s,e))
    return fail(e,"House disabled snapshot requires actual native shape");
  out=i->second.disabled;e.clear();return true;
}
bool HouseReturnGeometryNative::owner_rid(FieldObjectId id,FieldPhysicsRid&out,std::string&e)const{
  auto i=instances_.find(id);FieldPhysicsRid actual_rid;
  if(i==instances_.end()||i->second.kind==Kind::Shape||i->second.deleted||!live_space(e)||
     !space_->house_owner_rid(id,*sources_,actual_rid,e)||actual_rid.space!=i->second.rid.space||
     actual_rid.handle!=i->second.rid.handle)return fail(e,"House native owner RID unavailable or replaced");
  out=actual_rid;e.clear();return true;
}
bool HouseReturnGeometryNative::set_collision(FieldObjectId id,uint32_t layer,uint32_t mask,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  if(i==instances_.end()||i->second.kind==Kind::Shape||i->second.deleted||!actual(id,d,s,e))
    return fail(e,"House collision-mask setter requires actual native CollisionObject");
  const auto prior=i->second.properties;i->second.properties.layer=layer;i->second.properties.mask=mask;
  if(!synchronize(id,i->second,e)){i->second.properties=prior;return false;}
  e.clear();return true;
}
bool HouseReturnGeometryNative::declaration(FieldObjectId id,std::string_view signal,uint32_t&args,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=instances_.find(id);
  uint32_t count;
  if(i==instances_.end()||i->second.deleted||!actual(id,d,s,e))
    return fail(e,"House native geometry signal emitter not live");
  if(signal=="tree_entered"||signal=="tree_exiting"||signal=="tree_exited"||signal=="ready")count=0;
  else if(signal=="child_entered_tree"||signal=="child_exiting_tree")count=1;
  else if(i->second.kind!=Kind::Area||!area_signal(signal,count))
    return fail(e,"House native geometry unknown signal declaration");
  args=count;e.clear();return true;
}
bool HouseReturnGeometryNative::bind_script_receiver(FieldObjectId id,SourceDispatch dispatch,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  if(!dispatch||receivers_.count(id)||!actual(id,d,s,e)||d->script.empty())
    return fail(e,"House native Area receiver has no actual checked source script owner");
  receivers_.emplace(id,Receiver{d->script_sha,std::move(dispatch)});e.clear();return true;
}
bool HouseReturnGeometryNative::dispatch_source(const FieldDeferredMessage&m,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto receiver=receivers_.find(m.object);
  if(receiver==receivers_.end()||m.kind!=FieldDeferredKind::Call||!actual(m.object,d,s,e)||
     !s->bound||!s->inside||d->script_sha!=receiver->second.sha)
    return fail(e,"House Area callback receiver lacks actual live source binding");
  bool emitting=false;
  for(const auto&row:instances_)if(row.second.kind==Kind::Area&&!row.second.deleted&&row.second.monitored){
    for(const auto signal:{"body_entered","body_exited","area_entered","area_exited",
        "body_shape_entered","body_shape_exited","area_shape_entered","area_shape_exited"})
      emitting|=signals_->emitting_to(row.first,signal,m.object,m.member);
  }
  if(!emitting)return fail(e,"House source method lacks actual native Area signal dispatch frame");
  return receiver->second.dispatch(m,e);
}
bool HouseReturnGeometryNative::source_object(uint32_t source,FieldObjectId&out,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto i=source_objects_.find(source);
  if(i==source_objects_.end()||!actual(i->second,d,s,e)||instances_.at(i->second).deleted)
    return fail(e,"House native geometry source ObjectID not live");
  out=i->second;e.clear();return true;
}
bool HouseReturnGeometryNative::release_deleted(FieldObjectId id,std::string&e){
  auto i=instances_.find(id);
  if(i==instances_.end()||!i->second.deleted||tree_->state(id)||registry_->object_exists(id))
    return fail(e,"House native release precedes actual native/ObjectDB deletion");
  if(i->second.kind!=Kind::Shape&&i->second.rid.handle&&space_->rid_alive(i->second.rid))
    return fail(e,"House native owner release precedes actual physics RID retirement");
  if(!signals_->release(id,e))return false;
  source_objects_.erase(i->second.source);receivers_.erase(id);instances_.erase(i);e.clear();return true;
}
}
