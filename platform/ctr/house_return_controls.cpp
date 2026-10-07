#include "house_return_controls.hpp"
#include <algorithm>
#include <cmath>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
bool same(FieldIdentity a,FieldIdentity b){return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;}
bool equal(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
float number(const HouseControlRecord&r,const char*k){return r.properties.at(k).numbers[0];}
Vec2 vector(const HouseControlRecord&r,const char*k){const auto&v=r.properties.at(k).numbers;return{v[0],v[1]};}
Vec2 initial_position(const HouseControlRecord&r){return{number(r,"margin_left"),number(r,"margin_top")};}
Vec2 initial_size(const HouseControlRecord&r){return{number(r,"margin_right")-number(r,"margin_left"),number(r,"margin_bottom")-number(r,"margin_top")};}
}
bool HouseReturnControlsNative::prepare(const HouseReturnControlsData&d,
 const FieldNodeTreeData&s,const FieldCanvasArtData&a,FieldNodeTreeRuntime&t,
 FieldGlobalRegistry&r,PodunkNativeRoot&root,HouseReturnButtonPromptNative&p,
 PodunkSceneNative&native,std::string&e){
 if(data_||!d.matches(s,a,e)||r.poisoned()||!r.kernel()||root.kernel_object()!=r.kernel()||!root.viewport_object()||(t.object_domain()&&t.object_domain()!=r.kernel()))return fail(e,"House Control actual native Root/Tree/Registry resources rejected");
 data_=&d;source_=&s;art_=&a;tree_=&t;registry_=&r;root_=&root;prompt_=&p;native_=&native;e.clear();return true;
}
bool HouseReturnControlsNative::owns(const FieldNodeDescriptor&d)const{
 auto*r=data_?data_->record(d.id):nullptr;return r&&d.path==r->node&&d.native_class==r->native_class;
}
bool HouseReturnControlsNative::owns(FieldObjectId id)const{return entries_.count(id)!=0;}
bool HouseReturnControlsNative::actual(FieldObjectId id,const Entry*&entry,std::string&e)const{
 auto i=entries_.find(id);auto*n=tree_?tree_->descriptor(id):nullptr;auto*s=tree_?tree_->state(id):nullptr;FieldIdentity identity;
 if(!data_||i==entries_.end()||!n||!s||!s->alive||registry_->poisoned()||!registry_->object_exists(id)||registry_->tree_owner(id).get()!=tree_||tree_->source_object(n->id)!=id||!tree_->object_identity(id,identity)||!same(identity,data_->identity())||n->id!=i->second.source->id||!owns(*n))return fail(e,"House Control actual native object/owner/source rejected");
 entry=&i->second;e.clear();return true;
}
bool HouseReturnControlsNative::prompt_state(FieldObjectId id,HouseButtonPromptControlState&out,std::string&e)const{
 const Entry*entry=nullptr;if(!actual(id,entry,e))return false;const auto&s=*entry->source;
 if(s.pose_owner!=1||prompt_->tree()!=tree_||prompt_->registry()!=registry_||!prompt_->native_control_snapshot(id,out,e)||out.tree!=tree_||out.registry!=registry_||out.object!=id||out.source_id!=s.id||out.source_parent!=s.parent||out.parent!=tree_->source_object(s.parent)||!out.constructed||!same(out.binding.identity,data_->identity())||out.binding.stable_id!=s.id||out.binding.native_class!=s.native_class)return fail(e,"House Control same actual Prompt native leaf receipt rejected");
 return true;
}
bool HouseReturnControlsNative::canvas_state(FieldObjectId id,PodunkSceneControlState&out,std::string&e)const{
 const Entry*entry=nullptr;if(!actual(id,entry,e))return false;const auto&r=*entry->source;
 if(r.pose_owner!=2||native_->canvas_tree()!=tree_||native_->canvas_data()!=art_||!native_->control_state(id,out,e)||out.tree!=tree_||out.registry!=registry_||out.object!=id||out.source_id!=r.id||out.native_class!=r.native_class||!out.constructed||!same(out.identity,data_->identity()))return fail(e,"House Control same actual Canvas native leaf receipt rejected");e.clear();return true;
}
bool HouseReturnControlsNative::construct(FieldObjectId id,const FieldNodeDescriptor&n,const FieldIdentity&i,std::string&e){
 auto*r=data_?data_->record(n.id):nullptr;auto*live=tree_?tree_->descriptor(id):nullptr;auto*state=tree_?tree_->state(id):nullptr;
 if(!r||!owns(n)||!same(i,data_->identity())||entries_.count(id)||!live||!state||!state->alive||live->id!=n.id||tree_->source_object(n.id)!=id||registry_->tree_owner(id).get()!=tree_||!registry_->object_exists(id)||!equal(state->local[2],initial_position(*r)))return fail(e,"House Control actual source native allocation/property order rejected");
 Entry entry;entry.source=r;entry.position=initial_position(*r);entry.size=initial_size(*r);
 if(r->pose_owner==0){
  // The actual un-scripted native constructor cache is detached here. Enter
  // obtains its real parent anchor rectangle; it is never a viewport guess.
  auto min=vector(*r,"rect_min_size");entry.size.x=std::max(entry.size.x,min.x);entry.size.y=std::max(entry.size.y,min.y);
 }
 entries_.emplace(id,std::move(entry));
 if(r->pose_owner==1){
  // Native allocation precedes PackedScene parent assignment. Read the real
  // leaf constructor directly here; the full actual-parent receipt is
  // required later at bind/finish_factory and every lifecycle/draw boundary.
  PromptNativeControlState p;
  if(prompt_->tree()!=tree_||prompt_->registry()!=registry_||!prompt_->native().control_state(id,p,e)||p.tree!=tree_||p.registry!=registry_||p.object!=id||p.source_id!=r->id||!same(p.binding.identity,i)||p.binding.stable_id!=r->id||p.binding.native_class!=r->native_class){entries_.erase(id);return fail(e,"House Control same actual Prompt constructor receipt rejected");}
  const std::string text=r->native_class=="Label"?r->properties.at("text").text:std::string{};
  if(p.entered||p.ready||!equal(p.position,initial_position(*r))||!equal(p.size,initial_size(*r))||p.text!=text){entries_.erase(id);return fail(e,"House Control actual Prompt native constructor properties differ");}
 }
 if(r->pose_owner==2){PodunkSceneControlState p;if(!canvas_state(id,p,e)||p.entered||p.ready||!equal(p.position,initial_position(*r))||!equal(p.size,initial_size(*r))){entries_.erase(id);return fail(e,"House Control actual Canvas native constructor properties differ");}}
 e.clear();return true;
}
bool HouseReturnControlsNative::bind(FieldObjectId id,const FieldNodeBinding&b,std::string&e){
 const Entry*entry=nullptr;if(!actual(id,entry,e))return false;auto*n=tree_->descriptor(id);
 if(entry->bound||!same(b.identity,data_->identity())||b.stable_id!=n->id||b.class_index!=n->class_index||b.native_class!=n->native_class||b.script_sha!=n->script_sha||!b.family||!b.capability)return fail(e,"House Control actual combined source/native binding rejected");
 if(entry->source->pose_owner==1){HouseButtonPromptControlState p;if(!prompt_state(id,p,e)||p.binding.family!=b.family||p.binding.capability!=b.capability)return fail(e,"House Control original Prompt native binding differs");}
 if(entry->source->pose_owner==2){PodunkSceneControlState p;if(!canvas_state(id,p,e)||!p.bound)return fail(e,"House Control original Canvas native binding absent");}
 auto&v=entries_.at(id);v.binding=b;v.bound=true;e.clear();return true;
}
bool HouseReturnControlsNative::finish_factory(std::string&e)const{
 if(!data_||prompt_->tree()!=tree_||prompt_->registry()!=registry_)return fail(e,"House Control factory actual Prompt owner absent");
 for(const auto&r:data_->records()){
  const Entry*entry=nullptr;auto id=tree_->source_object(r.id);if(!id||!actual(id,entry,e)||!entry->bound)return fail(e,"House Control factory lacks actual native constructor/binding");
  if(r.pose_owner==1){HouseButtonPromptControlState p;if(!prompt_state(id,p,e))return false;}
  if(r.pose_owner==2){PodunkSceneControlState p;if(!canvas_state(id,p,e)||!p.bound)return false;}
 }
 e.clear();return true;
}
bool HouseReturnControlsNative::resize(FieldObjectId id,Entry&entry,Vec2 parent_size,std::string&e){
 if(!std::isfinite(parent_size.x)||!std::isfinite(parent_size.y)||parent_size.x<0||parent_size.y<0)return fail(e,"House Control actual parent anchor rectangle rejected");
 const auto&r=*entry.source;
 Vec2 position={number(r,"margin_left")+number(r,"anchor_left")*parent_size.x,number(r,"margin_top")+number(r,"anchor_top")*parent_size.y};
 Vec2 size={number(r,"margin_right")+number(r,"anchor_right")*parent_size.x-position.x,number(r,"margin_bottom")+number(r,"anchor_bottom")*parent_size.y-position.y};auto min=vector(r,"rect_min_size");
 auto grow=[](float&pos,float&size,float min,int32_t direction){if(min>size){if(direction==0)pos+=size-min;else if(direction==2)pos+=.5f*(size-min);size=min;}};
 grow(position.x,size.x,min.x,r.properties.at("grow_horizontal").integer);grow(position.y,size.y,min.y,r.properties.at("grow_vertical").integer);
 // _size_changed updates the same native transform cache. Signal dispatch is
 // provided by the actual Root/Canvas parent observer, never another clock.
 auto local=tree_->state(id)->local;local[2]=position;if(!equal(entry.position,position)&&!tree_->set_local(id,local,e))return false;
 entry.position=position;entry.size=size;entry.parent_size=parent_size;entry.parent_rect_observed=true;e.clear();return true;
}
bool HouseReturnControlsNative::parent_rect_changed(PodunkSceneNative&native,FieldObjectId parent,std::string&e){
 auto*n=tree_?tree_->descriptor(parent):nullptr;auto*s=tree_?tree_->state(parent):nullptr;
 if(!data_||!parent||!n||!s||!s->alive||!s->inside||!root_->viewport().inside||n->native_class!="Node2D"||native.canvas_tree()!=tree_||native.canvas_data()!=art_||!native.owns(parent)||!registry_->object_exists(parent)||registry_->tree_owner(parent).get()!=tree_)return fail(e,"House Control actual source native Canvas parent missing");
 // The loaded source proof covers CanvasItem's native zero anchorable rect
 // and Node2D's absence of an override. The actual owning native instance is
 // required above; arbitrary callers cannot supply a guessed rectangle.
 const Vec2 size{};
 bool found=false;for(auto&v:entries_)if(v.second.source->native_class=="Control"&&tree_->source_object(v.second.source->parent)==parent){const Entry*entry=nullptr;if(!actual(v.first,entry,e)||!resize(v.first,v.second,size,e))return false;found=true;}
 if(!found)return fail(e,"House Control parent rectangle has no checked native receiver");e.clear();return true;
}
bool HouseReturnControlsNative::phase(FieldObjectId id,FieldTreePhase phase,std::string&e){
 if(phase==FieldTreePhase::Deleting)return release_deleted(id,e);
 const Entry*entry=nullptr;if(!actual(id,entry,e)||!entry->bound)return fail(e,"House Control native phase lacks actual binding");auto&v=entries_.at(id);auto*s=tree_->state(id);
 if(v.source->pose_owner){
  bool entered=false,ready=false;
  if(v.source->pose_owner==1){HouseButtonPromptControlState p;if(!prompt_state(id,p,e))return false;entered=p.entered;ready=p.ready;}
  else{PodunkSceneControlState p;if(!canvas_state(id,p,e)||!p.bound)return false;entered=p.entered;ready=p.ready;}
  switch(phase){
  case FieldTreePhase::EnterNative:if(!entered||v.entered||!s->inside)return fail(e,"House Control actual native Enter cursor differs");v.entered=true;break;
  case FieldTreePhase::ReadyNative:if(!entered||!ready||!v.entered||!s->ready_notified||s->ready_first)return fail(e,"House Control actual native Ready cursor differs");v.ready=true;break;
  case FieldTreePhase::ExitNative:if(entered||!v.entered)return fail(e,"House Control actual native Exit cursor differs");v.entered=false;break;
  case FieldTreePhase::Deleting:return release_deleted(id,e);
  case FieldTreePhase::PostEnterNative:case FieldTreePhase::Parented:case FieldTreePhase::Unparented:case FieldTreePhase::ChildMoved:case FieldTreePhase::PathChanged:case FieldTreePhase::TransformChanged:case FieldTreePhase::LocalTransformChanged:case FieldTreePhase::VisibilityChanged:case FieldTreePhase::Hide:break;
  default:return fail(e,"House Control unsupported Prompt native phase");
  }
 }else{
  switch(phase){
  case FieldTreePhase::EnterNative:if(v.entered||!s->inside||!root_->viewport().inside)return fail(e,"House Control actual Enter cursor rejected");v.entered=true;break;
  case FieldTreePhase::PostEnterNative:if(!v.entered||!v.parent_rect_observed)return fail(e,"House Control actual parent anchor rectangle not admitted");break;
  case FieldTreePhase::ReadyNative:if(!v.entered||!v.parent_rect_observed||!s->ready_notified||s->ready_first)return fail(e,"House Control actual native Ready cursor rejected");v.ready=true;break;
  case FieldTreePhase::ExitNative:if(!v.entered)return fail(e,"House Control actual Exit cursor rejected");v.entered=false;v.parent_rect_observed=false;break;
  case FieldTreePhase::Deleting:return release_deleted(id,e);
  case FieldTreePhase::Parented:case FieldTreePhase::Unparented:case FieldTreePhase::ChildMoved:case FieldTreePhase::PathChanged:case FieldTreePhase::TransformChanged:case FieldTreePhase::LocalTransformChanged:case FieldTreePhase::VisibilityChanged:case FieldTreePhase::Hide:break;
  default:return fail(e,"House Control unsupported source/native phase");
  }
 }
 if(phase==FieldTreePhase::ReadyNative&&!root_->house_gui_registered(id,e)){v.ready=false;return false;}
 e.clear();return true;
}
bool HouseReturnControlsNative::release_deleted(FieldObjectId id,std::string&e){
 auto i=entries_.find(id);auto*s=tree_?tree_->state(id):nullptr;
 if(i==entries_.end()||i->second.entered||(s&&s->inside))return fail(e,"House Control native deletion before actual Exit rejected");
 if(!root_->house_gui_unregistered(id,e))return false;
 // The borrowed Prompt leaf has its real deletion owner; this erases only
 // this Control's retained admission, never its ObjectID or GPU assets.
 entries_.erase(i);e.clear();return true;
}
bool HouseReturnControlsNative::snapshot(FieldObjectId id,HouseControlNativeState&out,std::string&e)const{
 const Entry*entry=nullptr;if(!actual(id,entry,e))return false;const auto&s=*entry->source;HouseControlNativeState v;v.source=&s;v.object=id;v.parent=tree_->source_object(s.parent);v.owner=tree_->source_object(s.owner);v.binding=entry->binding;v.constructed=true;v.bound=entry->bound;v.entered=entry->entered;v.ready=entry->ready;
 if(s.pose_owner==0){v.position=entry->position;v.size=entry->size;}
 else if(s.pose_owner==1){HouseButtonPromptControlState p;if(!prompt_state(id,p,e)||p.entered!=v.entered||p.ready!=v.ready)return fail(e,"House Control same Prompt native lifecycle snapshot differs");v.position=p.position;v.size=p.size;v.text=p.text;}
 else{PodunkSceneControlState p;if(!canvas_state(id,p,e)||p.entered!=v.entered||p.ready!=v.ready)return fail(e,"House Control same Canvas native lifecycle snapshot differs");v.position=p.position;v.size=p.size;}
 out=std::move(v);e.clear();return true;
}
bool HouseReturnControlsNative::admit_control(const FieldCanvasControlBoundary&b,FieldObjectId id,FieldObjectId owner,std::string&e)const{
 const Entry*entry=nullptr;if(!actual(id,entry,e))return false;const auto&r=*entry->source;
 if(art_->control_boundary(r.id)!=&b||b.id!=r.id||b.native_properties_sha!=r.properties_sha||b.owner_sha!=r.owner_sha||owner!=tree_->source_object(r.owner)||!registry_->object_exists(owner)||registry_->tree_owner(owner).get()!=tree_||!entry->bound)return fail(e,"House Control complete original native property digest/owner differs");
 auto*n=tree_->descriptor(owner);if(!n||n->script!=r.owner_script||n->script_sha!=r.owner_sha)return fail(e,"House Control actual original script owner differs");
 if(r.pose_owner==1){HouseButtonPromptControlState p;if(!prompt_state(id,p,e))return false;}
 e.clear();return true;
}
bool HouseReturnControlsNative::control_snapshot(const FieldCanvasControlBoundary&b,FieldObjectId id,FieldObjectId owner,bool&drawable,std::string&e)const{
 if(!admit_control(b,id,owner,e))return false;HouseControlNativeState s;auto*state=tree_->state(id);
 if(!snapshot(id,s,e)||!s.entered||!s.ready||!state->inside||!state->ready_notified||state->ready_first)return fail(e,"House Control draw requires actual native Enter/Ready");
 if(b.native_class=="Label"&&!prompt_->native().owns_drawable(id))return fail(e,"House Label actual same native GPU/font owner missing");drawable=b.native_class=="Label";e.clear();return true;
}
bool HouseReturnControlsNative::owns_drawable(FieldObjectId id)const{auto i=entries_.find(id);return i!=entries_.end()&&i->second.source->native_class=="Label"&&prompt_&&prompt_->native().owns_drawable(id);}
bool HouseReturnControlsNative::draw_leaf(const FieldCanvasOrderSlot&slot,const FieldTransform&viewport,bool snap,std::string&e){
 auto*b=art_?art_->control_boundary(slot.source):nullptr;bool drawable=false;
 if(!b||!same(slot.identity,data_->identity())||!owns_drawable(slot.object)||!control_snapshot(*b,slot.object,tree_->source_object(b->owner_id),drawable,e)||!drawable)return fail(e,"House Label native Canvas slot/Ready/source rejected");
 return prompt_->native().draw_leaf(slot,viewport,snap,e);
}
}
