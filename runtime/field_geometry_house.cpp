#include "encore/field_geometry_space.hpp"
#include "encore/field_global_registry.hpp"
#include "encore/house_reentry.hpp"
#include "encore/house_return_sources.hpp"
#include "encore/player_initialization.hpp"
#include "encore/field_door.hpp"
#include <set>

namespace encore::upstream {
namespace {
constexpr uint32_t no_node=UINT32_MAX;
bool equal(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
bool equal(const FieldTransform&a,const FieldGeometryTransform&b){
  return equal(a[0],b.x)&&equal(a[1],b.y)&&equal(a[2],b.origin);
}
bool pin(const FieldIdentity&a,const FieldIdentity&b){
  return a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;
}
bool same_transform(const FieldTransform&a,const FieldTransform&b){
  return equal(a[0],b[0])&&equal(a[1],b[1])&&equal(a[2],b[2]);
}
}
bool FieldGeometrySpace::persistent_door_node(const DynamicOwner&o,
    FieldObjectId object,std::string&e,bool detached_shape_delete)const{
  const auto*n=o.tree?o.tree->state(object):nullptr;
  const auto*d=o.tree?o.tree->descriptor(object):nullptr;FieldIdentity identity;
  const auto*expected=object==o.object?&o.door_descriptor:
    object==o.door_shape?&o.shape_descriptor:nullptr;
  FieldDoorDescriptor source;
  if(!o.door||!o.door->valid()||!o.door_geometry||!o.door_geometry->valid()||
     !o.door->find(o.door_id,source)||!o.registry||!o.tree||!expected||!n||!d||
     !n->alive||!n->bound||!o.registry->object_exists(object)||
     o.registry->tree_owner(object).get()!=o.tree||
     o.tree->source_object(d->id)!=object||!o.tree->object_identity(object,identity)||
     identity.scene_id!=o.door->identity().scene_id||!pin(identity,o.door->identity())||
     d->id!=expected->id||d->parent!=expected->parent||d->path!=expected->path||
     d->native_class!=expected->native_class||d->class_index!=expected->class_index||
     d->script!=expected->script||d->script_sha!=expected->script_sha||
     !same_transform(d->local,expected->local)||!same_transform(d->world,expected->world)||
     (object==o.object?(d->id!=source.id||d->native_class!="Area2D"||
       d->path!=o.door->string(source.node)||d->script!=o.door->script()||
       d->script_sha!=o.door->script_sha()):
       (d->id!=source.shape||d->native_class!="CollisionShape2D"||
        !d->script.empty()||
        (n->parent!=o.object&&!(detached_shape_delete&&!n->parent&&!n->inside))))){
    e="Persistent Door original source/native ObjectDB binding differs";return false;
  }
  e.clear();return true;
}
bool FieldGeometrySpace::retain_detached_door(const FieldDoorData&data,uint32_t stable,
    FieldNodeTreeRuntime&tree,FieldGlobalRegistry&registry,FieldObjectId object,
    FieldObjectId shape,bool native_disabled,FieldPersistentDoorGeometry&out,std::string&e){
  FieldDoorDescriptor d;
  const auto*o=tree.state(object);const auto*s=tree.state(shape);
  if(!source_||!data.valid()||!data.find(stable,d)||!o||!s||
     o->parent||o->inside||s->inside||o->ready_first||s->ready_first||
     o->blocked||s->blocked||o->queued||s->queued||tree.lifecycle_pending()||
     !tree.object_domain()||tree.object_domain()!=registry.kernel()||
     dynamic_owners_.count(object)||!tree.descriptor(object)||!tree.descriptor(shape)||
     source_->source_scene()!=data.source_scene()||!pin(source_->identity(),data.identity())){
    e="Persistent Door retention requires actual source detach/Exit and original space";return false;
  }
  DynamicOwner owner;owner.tree=&tree;owner.registry=&registry;owner.object=object;
  owner.door=&data;owner.door_geometry=source_;owner.door_id=stable;owner.door_shape=shape;
  owner.door_descriptor=*tree.descriptor(object);owner.shape_descriptor=*tree.descriptor(shape);
  if(!persistent_door_node(owner,object,e)||!persistent_door_node(owner,shape,e))return false;
  uint32_t owner_index=no_node,shape_index=no_node;
  for(uint32_t i=0;i<source_->owner_count();++i)
    if(source_->node(source_->owner(i).node).stable_id==stable)owner_index=i;
  for(uint32_t i=0;i<source_->shape_count();++i)
    if(source_->node(source_->shape(i).node).stable_id==d.shape)shape_index=i;
  const auto rid=static_rids_.find(owner_index);
  if(owner_index==no_node||shape_index==no_node||rid==static_rids_.end()||!rid->second||
     rid->second>next_rid_||owner_index>=owners_.size()||shape_index>=disabled_.size()){
    e="Persistent Door exact old static owner/shape/RID absent";return false;
  }
  const auto po=source_->owner(owner_index);const auto ps=source_->shape(shape_index);
  const auto op=source_->node(po.node),sp=source_->node(ps.node);
  const auto os=owners_[owner_index];
  if(po.kind!=4||po.space_override||(po.flags&8)||ps.owner!=owner_index||
     ps.kind!=uint32_t(FieldGeometryKind::Rectangle)||ps.part_count!=1||(ps.flags&2)||
     owner_shapes_[owner_index].size()!=1||
     os.layer!=d.layer||os.mask!=d.mask||os.monitoring!=bool(d.flags&1)||
     os.monitorable!=bool(d.flags&2)||!nodes_[po.node].bound||nodes_[po.node].deleted||
     nodes_[ps.node].deleted||source_->string(op.path)!=owner.door_descriptor.path||
     source_->string(sp.path)!=owner.shape_descriptor.path||op.script_sha256!=data.script_sha()||
     !equal(owner.door_descriptor.local,op.local)||!equal(owner.shape_descriptor.local,sp.local)||
     sp.local.origin.x!=d.shape_offset.x||sp.local.origin.y!=d.shape_offset.y){
    e="Persistent Door original Rectangle/owner semantics differ";return false;
  }
  const Instance*original=nullptr;
  for(const auto&v:instances_)if(v.contact.owner==owner_index&&v.contact.shape==shape_index){
    if(original||v.deleted||!v.resolved||v.actor.kind!=FieldGeometryKind::Rectangle||
       v.actor.extents.x!=d.extents.x||v.actor.extents.y!=d.extents.y){
      e="Persistent Door original native Rectangle parts differ";return false;
    }
    original=&v;
  }
  if(!original){e="Persistent Door original Rectangle unavailable";return false;}
  DynamicInstance kept;kept.tree=&tree;kept.registry=&registry;kept.player=object;kept.door=&data;
  kept.actor=original->actor;kept.owner=po;kept.owner.layer=os.layer;kept.owner.mask=os.mask;
  kept.owner.flags=(po.flags&~6u)|(os.monitoring?2u:0u)|(os.monitorable?4u:0u);
  kept.shape=ps;kept.disabled=true;kept.shape.flags=(ps.flags&~1u)|(native_disabled?1u:0u);
  kept.contact={no_node,no_node,0,stable,original->contact.native_shape_index,object,shape};
  owner.rid=rid->second;
  // The original native owner survives. The old immutable-scene indexing no
  // longer owns its Rectangle or RID; all other old geometry remains normal.
  dynamic_owners_.emplace(object,owner);dynamic_.push_back(std::move(kept));
  static_rids_.erase(rid);
  for(auto&v:instances_)if(v.contact.owner==owner_index)v.deleted=true;
  return persistent_door(object,out,e);
}
bool FieldGeometrySpace::persistent_door(FieldObjectId object,
    FieldPersistentDoorGeometry&out,std::string&e)const{
  const auto at=dynamic_owners_.find(object);
  if(at==dynamic_owners_.end()||!at->second.door||!at->second.rid||
     !persistent_door_node(at->second,object,e)){
    e="Persistent Door actual retained owner/RID missing";return false;
  }
  const auto&o=at->second;FieldPersistentDoorGeometry next;
  next.data=o.door;next.tree=o.tree;next.registry=o.registry;next.door=object;
  next.shape=o.door_shape;next.source_id=o.door_id;next.rid={this,o.rid};
  for(const auto&v:dynamic_)if(v.contact.actual_owner==object){
    if(next.contact.actual_owner||v.door!=o.door||!persistent_door_node(o,v.contact.actual_shape,e)){
      e="Persistent Door retained Rectangle binding differs";return false;
    }
    next.contact=v.contact;next.disabled=v.disabled;
  }
  out=next;e.clear();return true;
}
bool FieldGeometrySpace::rebind_persistent_door(const FieldDoorData&data,FieldObjectId object,
    FieldNodeTreeRuntime&old_tree,FieldNodeTreeRuntime&next,FieldGlobalRegistry&registry,std::string&e){
  auto at=dynamic_owners_.find(object);
  if(at==dynamic_owners_.end()||at->second.door!=&data||at->second.tree!=&old_tree||
     at->second.registry!=&registry||&old_tree==&next||old_tree.state(object)||
     old_tree.object_domain()!=next.object_domain()||next.object_domain()!=registry.kernel()){
    e="Persistent Door native transfer has foreign old/new owner";return false;
  }
  auto candidate=at->second;candidate.tree=&next;
  const auto*o=next.state(object);const auto*s=next.state(candidate.door_shape);
  if(!o||!s||o->inside||s->inside||o->parent||o->ready_first||s->ready_first||
     !persistent_door_node(candidate,object,e)||!persistent_door_node(candidate,candidate.door_shape,e))return false;
  bool found=false;
  for(const auto&v:dynamic_)if(v.contact.actual_owner==object){
    if(v.door!=&data||v.tree!=&old_tree||!v.disabled||found){
      e="Persistent Door transfer retains active/foreign shapes";return false;
    }
    found=true;
  }
  if(!found){e="Persistent Door native shape was lost before transfer";return false;}
  at->second.tree=&next;
  for(auto&v:dynamic_)if(v.contact.actual_owner==object)v.tree=&next;
  e.clear();return true;
}
bool FieldGeometrySpace::update_persistent_door(const FieldDoorData&data,
    FieldObjectId object,bool disabled,std::string&e){
  auto at=dynamic_owners_.find(object);
  if(at==dynamic_owners_.end()||at->second.door!=&data||
     !persistent_door_node(at->second,object,e)||
     !persistent_door_node(at->second,at->second.door_shape,e))return false;
  auto&o=at->second;const auto*parent=o.tree->state(object);const auto*shape=o.tree->state(o.door_shape);
  for(auto&v:dynamic_)if(v.contact.actual_owner==object){
    v.disabled=disabled||!parent->inside||!shape->inside;
    v.shape.flags=(v.shape.flags&~1u)|(disabled?1u:0u);e.clear();return true;
  }
  e="Persistent Door source shape update lacks actual Rectangle";return false;
}
bool FieldGeometrySpace::delete_persistent_door_shape(const FieldDoorData&data,
    FieldObjectId object,std::string&e){
  for(auto at=dynamic_.begin();at!=dynamic_.end();++at)if(at->contact.actual_shape==object){
    const auto owner=dynamic_owners_.find(at->contact.actual_owner);
    const auto*s=at->tree->state(object);
    if(owner==dynamic_owners_.end()||owner->second.door!=&data||at->door!=&data||
       !s||s->inside||s->parent||!at->disabled||
       !persistent_door_node(owner->second,object,e,true))return false;
    dynamic_.erase(at);e.clear();return true;
  }
  e="Persistent Door Delete has no retained source shape";return false;
}
bool FieldGeometrySpace::delete_persistent_door_owner(const FieldDoorData&data,
    FieldObjectId object,std::string&e){
  auto at=dynamic_owners_.find(object);const auto*s=at!=dynamic_owners_.end()?at->second.tree->state(object):nullptr;
  if(at==dynamic_owners_.end()||at->second.door!=&data||!s||s->inside||
     !persistent_door_node(at->second,object,e))return false;
  for(const auto&v:dynamic_)if(v.contact.actual_owner==object){
    e="Persistent Door native RID retirement precedes shape deletion";return false;
  }
  dynamic_owners_.erase(at);e.clear();return true;
}
bool FieldGeometrySpace::replace_house_world(const HouseReentryData&return_data,
    const FieldGeometryView&geometry,const FieldNodeTreeRuntime&old_tree,
    FieldObjectId old_root,FieldNodeTreeRuntime&house_tree,
    const std::vector<FieldGeometryNodeUpdate>&updates,std::string&e){
  if(!source_||!return_data.valid()||!geometry.valid()||
     source_->source_scene()!=return_data.source_scene()||
     geometry.source_scene()!=return_data.target_scene()||
     !pin(geometry.identity(),return_data.identity())||
     source_->identity().upstream_commit!=return_data.identity().upstream_commit||
     !old_root||old_tree.root()!=old_root||old_tree.state(old_root)||
     old_tree.lifecycle_pending()||&old_tree==&house_tree||
     !old_tree.object_domain()||old_tree.object_domain()!=house_tree.object_domain()||
     !house_tree.state(house_tree.root())||dynamic_owners_.empty()){
    e="House geometry replacement source/actual scene exit/shapes rejected";return false;
  }
  // Detached original Door shapes are retained but inactive. Every other
  // dynamic shape must have completed its source Exit before replacement.
  for(const auto&v:dynamic_){
    const auto owner=dynamic_owners_.find(v.contact.actual_owner);
    const auto*o=v.tree?v.tree->state(v.contact.actual_owner):nullptr;
    const auto*s=v.tree?v.tree->state(v.contact.actual_shape):nullptr;
    if(!v.door||owner==dynamic_owners_.end()||owner->second.door!=v.door||
       v.tree!=&old_tree||!o||!s||o->parent||o->inside||s->inside||o->queued||s->queued||!v.disabled||
       !persistent_door_node(owner->second,o->object,e)||
       !persistent_door_node(owner->second,s->object,e)){
      e="House replacement retains non-detached/foreign dynamic geometry";return false;
    }
  }
  for(uint32_t i=0;i<source_->node_count();++i)
    if(old_tree.source_object(source_->node(i).stable_id)){
      const auto object=old_tree.source_object(source_->node(i).stable_id);
      bool retained=false;
      for(const auto&entry:dynamic_owners_){const auto&o=entry.second;
        if(o.door&&o.tree==&old_tree&&(object==o.object||object==o.door_shape)&&
           persistent_door_node(o,object,e))retained=true;
      }
      if(!retained){e="House geometry replacement retains an old native collider ancestor";return false;}
    }
  // Only the detached original Player and explicitly retained source Door
  // CollisionObject owners may survive. Grass/Camera owners must retire
  // through their real Exit/Delete callbacks before replacing static space.
  for(const auto&v:dynamic_owners_){
    const auto&o=v.second;
    if((o.door?!persistent_door_node(o,o.object,e):(!o.data||!o.data->valid()))||o.grass||o.dialogue||o.recipe||
       !o.registry||!o.registry->object_exists(v.first)||!o.tree||
       o.tree->object_domain()!=house_tree.object_domain()||
       v.first!=o.object||!o.rid||o.rid>next_rid_){
      e="House geometry replacement retained owner is not the actual Player";return false;
    }
  }
  FieldGeometrySpace candidate;
  candidate.next_rid_=next_rid_;
  if(!candidate.configure(geometry,cell_size_,e)||!candidate.apply_updates(updates,e))return false;
  std::vector<FieldObjectId>actual(geometry.node_count());
  std::set<FieldObjectId>seen;
  for(uint32_t i=0;i<geometry.node_count();++i){
    const auto n=geometry.node(i);
    bool deleted=false;
    for(uint32_t at=i;at!=no_node;at=geometry.node(at).parent)
      deleted|=candidate.nodes_[at].deleted;
    const auto object=house_tree.source_object(n.stable_id);
    if(deleted){
      if(object){e="House geometry deletion observation precedes actual native deletion";return false;}
      continue;
    }
    const auto*s=house_tree.state(object);
    const auto*d=house_tree.descriptor(object);
    FieldIdentity identity;
    if(!object||!s||!d||!s->bound||!seen.insert(object).second||
       !house_tree.object_identity(object,identity)||!pin(identity,geometry.identity())||
       d->id!=n.stable_id||d->path!=geometry.string(n.path)||
       d->native_class!=geometry.string(n.class_name)||
       d->script!=geometry.string(n.script)||d->script_sha!=n.script_sha256||
       !equal(d->local,n.local)||!equal(s->local,candidate.nodes_[i].local)||
       (n.parent==no_node?object!=house_tree.root():
        s->parent!=house_tree.source_object(geometry.node(n.parent).stable_id))){
      e="House geometry actual native source node/transform/ownership differs";return false;
    }
    actual[i]=object;
  }
  for(auto&instance:candidate.instances_){
    const auto shape=geometry.shape(instance.contact.shape);
    const auto owner=geometry.owner(instance.contact.owner);
    instance.contact.actual_owner=actual[owner.node];
    instance.contact.actual_shape=actual[shape.node];
    if(!instance.deleted&&(!instance.contact.actual_owner||!instance.contact.actual_shape)){
      e="House geometry live shape has no actual native collider";return false;
    }
  }
  for(uint32_t i=0;i<geometry.owner_count();++i)
    if(!actual[geometry.owner(i).node])candidate.static_rids_.erase(i);
  candidate.native_tree_=&house_tree;
  candidate.dynamic_owners_=std::move(dynamic_owners_);
  candidate.dynamic_=std::move(dynamic_);
  *this=std::move(candidate);
  e.clear();return true;
}
bool FieldGeometrySpace::native_instance(const Instance&entry,std::string&e)const{
  if(!native_tree_)return true;
  const auto&c=entry.contact;
  const auto*owner=native_tree_->state(c.actual_owner);
  const auto*shape=native_tree_->state(c.actual_shape);
  const auto*od=native_tree_->descriptor(c.actual_owner);
  const auto*sd=native_tree_->descriptor(c.actual_shape);
  const auto packed_shape=source_->shape(c.shape);
  const auto packed_owner=source_->owner(c.owner);
  FieldIdentity oi,si;
  if(!owner||!shape||!od||!sd||!owner->inside||!shape->inside||
     !owner->bound||!shape->bound||shape->parent!=owner->object||
     owner->world_dirty||shape->world_dirty||
     od->id!=source_->node(packed_owner.node).stable_id||
     sd->id!=source_->node(packed_shape.node).stable_id||
     !native_tree_->object_identity(owner->object,oi)||
     !native_tree_->object_identity(shape->object,si)||
     !pin(oi,source_->identity())||!pin(si,source_->identity())||
     !equal(owner->local,nodes_[packed_owner.node].local)||
     !equal(shape->local,nodes_[packed_shape.node].local)||
     !equal(owner->world,nodes_[packed_owner.node].world)||
     !equal(shape->world,nodes_[packed_shape.node].world)){
    e="House geometry actual native collider lifecycle/transform differs";return false;
  }
  return true;
}
bool FieldGeometrySpace::house_owner_rid(FieldObjectId object,
    const HouseReturnSources&sources,FieldPhysicsRid&out,std::string&e)const{
  const auto*state=native_tree_?native_tree_->state(object):nullptr;
  const auto*descriptor=native_tree_?native_tree_->descriptor(object):nullptr;
  FieldIdentity identity;
  if(!sources.valid()||source_!=&sources.geometry()||!native_tree_||!object||
     !state||!descriptor||!state->alive||state->queued||!state->bound||
     state->world_dirty||!native_tree_->object_domain()||
     !native_tree_->object_identity(object,identity)||
     identity.scene_id!=sources.tree().identity().scene_id||
     !pin(identity,sources.tree().identity())||
     !pin(identity,source_->identity())){
    e="House native RID observation requires the actual cross-bound destination";return false;
  }
  const auto*source_node=sources.tree().record(descriptor->id);
  if(!source_node||source_node->path!=descriptor->path||
     source_node->native_class!=descriptor->native_class||
     source_node->script!=descriptor->script||source_node->script_sha!=descriptor->script_sha||
     native_tree_->source_object(descriptor->id)!=object){
    e="House native RID source ObjectID/descriptor differs";return false;
  }
  for(uint32_t index=0;index<source_->owner_count();++index){
    const auto owner=source_->owner(index);
    if(owner.node>=source_->node_count()||source_->node(owner.node).stable_id!=descriptor->id)continue;
    const auto it=static_rids_.find(index);
    if(it==static_rids_.end()||!it->second||it->second>next_rid_||
       owner.node>=nodes_.size()||nodes_[owner.node].deleted||
       !equal(state->local,nodes_[owner.node].local)||
       !equal(state->world,nodes_[owner.node].world)){
      e="House native RID was retired or its actual transform changed";return false;
    }
    out={this,it->second};e.clear();return true;
  }
  e="House native RID object is not a source CollisionObject owner";return false;
}
} // namespace encore::upstream
