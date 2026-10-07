#include "encore/field_geometry_space.hpp"
#include "encore/field_global_registry.hpp"
#include "encore/house_reentry.hpp"
#include "encore/house_return_sources.hpp"
#include "encore/player_initialization.hpp"
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
     !house_tree.state(house_tree.root())||dynamic_owners_.empty()||!dynamic_.empty()){
    e="House geometry replacement source/actual scene exit/shapes rejected";return false;
  }
  for(uint32_t i=0;i<source_->node_count();++i)
    if(old_tree.source_object(source_->node(i).stable_id)){
      e="House geometry replacement retains an old native collider ancestor";return false;
    }
  // Only the detached original Player CollisionObject owners may survive.
  // Grass/Camera owners belong to their concrete lifetimes and must retire
  // through their real Exit/Delete callbacks before replacing static space.
  for(const auto&v:dynamic_owners_){
    const auto&o=v.second;
    if(!o.data||!o.data->valid()||o.grass||o.dialogue||o.recipe||
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
