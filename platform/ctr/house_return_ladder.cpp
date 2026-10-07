#include "house_return_ladder.hpp"
#include <algorithm>
#include <cmath>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
}
bool HouseReturnLadder::prepare(const HouseReturnLadderData&d,const FieldNodeTreeData&nodes,
 const FieldGeometryView&geometry,const PodunkPlayerSources&sources,FieldNodeTreeRuntime&tree,
 FieldGlobalRegistry&registry,FieldObjectSignals&signals,FieldGeometrySpace&space,
 PodunkPlayerHost&player,std::string&e){
 if(data_||!sources.initialization||!sources.ready||!sources.motion||signals.registry()!=&registry||
    player.registry()!=&registry||!player.ready_complete()||
    !d.matches(nodes,geometry,*sources.initialization,*sources.ready,*sources.motion,e))
  return fail(e,"House Ladder actual source/player owners incomplete");
 if(!player.motion().bind_ladder(d,player.animations(),e))return false;
 data_=&d;nodes_=&nodes;geometry_=&geometry;tree_=&tree;registry_=&registry;
 signals_=&signals;space_=&space;player_=&player;ir_=d.ir_sha256();e.clear();return true;
}
bool HouseReturnLadder::actual(std::string&e)const{
 if(!data_||poisoned_||!data_->valid()||data_->ir_sha256()!=ir_||!object_||
    tree_->object_domain()!=registry_->kernel()||signals_->registry()!=registry_||
    !registry_->object_exists(object_)||registry_->tree_owner(object_).get()!=tree_)
  return fail(e,"House Ladder source/actual ObjectDB owner unavailable");
 const auto*d=tree_->descriptor(object_);const auto*n=tree_->state(object_);
 FieldIdentity identity;std::array<uint8_t,32>sha{};
 if(!d||!n||!n->alive||d->id!=data_->id()||d->path!=data_->node()||d->native_class!="Area2D"||
    d->script_methods||d->script!=data_->text(HouseLadderText::Script)||
    !data_->source_hash(d->script,sha)||sha!=d->script_sha||
    !tree_->object_identity(object_,identity)||identity.scene_id!=nodes_->identity().scene_id||
    identity.upstream_commit!=nodes_->identity().upstream_commit||
    identity.source_sha256!=nodes_->identity().source_sha256)
  return fail(e,"House Ladder live source body/closed lifecycle differs");
 return true;
}
bool HouseReturnLadder::construct(FieldObjectId id,std::string&e){
 if(!data_||object_)return fail(e,"House Ladder duplicate/unprepared source constructor");
 const auto*n=tree_->state(id);const auto*d=tree_->descriptor(id);
 if(!n||!d||d->id!=data_->id()||n->inside||n->bound||n->parent||n->ready_notified)
  return fail(e,"House Ladder actual script-attachment cursor differs");
 object_=id;if(!actual(e)){object_=0;return false;}
 // The reviewed script has no declarations or _init. Constructor attachment
 // owns this exact body; all native properties/children remain native work.
 e.clear();return true;
}
bool HouseReturnLadder::connect_source(std::string&e){
 if(!actual(e)||connected_)return fail(e,"House Ladder PackedScene connection cursor differs");
 const auto*n=tree_->state(object_);const auto shape=tree_->source_object(data_->shape_id());
 const auto*s=tree_->state(shape);
 if(n->inside||!n->parent||!s||s->parent!=object_||!tree_->root())
  return fail(e,"House Ladder original PackedScene child/parent closure incomplete");
 for(const auto&c:data_->connections())if(!signals_->connect(object_,c.signal,object_,c.method,c.flags,{},e)){
  poisoned_=true;return false;
 }
 connected_=true;e.clear();return true;
}
bool HouseReturnLadder::bind_geometry(std::string&e){
 if(!actual(e)||!connected_||geometry_bound_||space_->source()!=geometry_)
  return fail(e,"House Ladder actual House physics-space source not committed");
 std::array<uint8_t,32>sha{};if(!data_->source_hash(data_->text(HouseLadderText::Script),sha)||
   !space_->bind_script(data_->id(),sha,0x454e0078,1,e))return false;
 geometry_bound_=true;e.clear();return true;
}
bool HouseReturnLadder::source_constructed(FieldObjectId id)const{
 return data_&&!poisoned_&&id==object_&&registry_->object_exists(id)&&tree_->state(id);
}
bool HouseReturnLadder::dispatch(const FieldDeferredMessage&m,std::string&e){
 if(!actual(e)||!connected_||!geometry_bound_||space_->source()!=geometry_||
    m.object!=object_||m.kind!=FieldDeferredKind::Call||
    m.args.size()!=1||!std::holds_alternative<FieldObjectRef>(m.args[0]))
  return fail(e,"House Ladder actual source signal/method signature differs");
 const bool enter=m.member==data_->text(HouseLadderText::EnterMethod);
 const bool exit=m.member==data_->text(HouseLadderText::ExitMethod);
 if((!enter&&!exit)||!signals_->emitting_to(object_,
    data_->text(enter?HouseLadderText::EnterSignal:HouseLadderText::ExitSignal),object_,m.member))
  return fail(e,"House Ladder source callback lacks actual Area signal frame");
 const auto body=std::get<FieldObjectRef>(m.args[0]).id;
 const auto owner=registry_->tree_owner(body);const auto*n=owner?owner->state(body):nullptr;
 const auto*d=owner?owner->descriptor(body):nullptr;
 if(!body||!n||!d||!n->bound||!registry_->object_exists(body))
  return fail(e,"House Ladder actual signal body native/source owner missing");
 // Exact source PartyObject check, closed over all current House static bodies
 // and the actual persistent Player owner. Future PartyObject subclasses are
 // rejected unless their real receiver stack is implemented.
 if(body!=player_->body().object()){
  const auto*source=nodes_->record(d->id);FieldIdentity identity;
  if(owner.get()!=tree_||!source||source->script!=d->script||source->script_sha!=d->script_sha||
     !owner->object_identity(body,identity)||identity.scene_id!=nodes_->identity().scene_id||
     identity.source_sha256!=nodes_->identity().source_sha256||
     std::find(data_->non_party_bodies().begin(),data_->non_party_bodies().end(),d->id)==data_->non_party_bodies().end())
   return fail(e,"House Ladder unknown PartyObject/body receiver is unsupported");
  // Original non-PartyObject branch: no ladder/unladder or position assignment.
  e.clear();return true;
 }
 std::array<uint8_t,32>sha{};
 if(player_->registry()!=registry_||player_->tree()!=owner.get()||owner.get()!=tree_||
    d->script!=data_->text(HouseLadderText::PlayerScript)||
    !data_->source_hash(d->script,sha)||sha!=d->script_sha||!player_->ready_complete())
  return fail(e,"House Ladder foreign persistent Player source receiver");
 bool ok=enter?player_->motion().ladder(e):player_->motion().unladder(e);
 if(!ok){poisoned_=true;return false;}
 if(enter){
  FieldTransform ladder,world;
  if(!tree_->world_transform(object_,ladder,e)||!tree_->world_transform(body,world,e)){
   poisoned_=true;return false;
  }
  const auto*state=tree_->state(body);FieldTransform parent{{{1,0},{0,1},{0,0}}};
  if(!state||(state->canvas_parent&&!tree_->world_transform(state->canvas_parent,parent,e))){poisoned_=true;return false;}
  const double determinant=double(parent[0].x)*parent[1].y-double(parent[1].x)*parent[0].y;
  if(!std::isfinite(determinant)||determinant==0){poisoned_=true;return fail(e,"Ladder Player parent global transform singular");}
  // Source changes only global_position.x AFTER the virtual ladder call.
  const double x=double(ladder[2].x)-parent[2].x,y=double(world[2].y)-parent[2].y;
  auto local=state->local;local[2]={float((parent[1].y*x-parent[1].x*y)/determinant),
                                float((-parent[0].y*x+parent[0].x*y)/determinant)};
  if(!std::isfinite(local[2].x)||!std::isfinite(local[2].y)||!tree_->set_local(body,local,e)){
   poisoned_=true;return false;
  }
 }
 e.clear();return true;
}
bool HouseReturnLadder::release_deleted(std::string&e){
 if(!data_||!object_||tree_->state(object_)||registry_->object_exists(object_))
  return fail(e,"House Ladder source release precedes actual native/ObjectDB deletion");
 if(!signals_->release(object_,e))return false;object_=0;e.clear();return true;
}
}
