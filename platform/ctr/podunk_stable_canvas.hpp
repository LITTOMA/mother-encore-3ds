#pragma once
#include "podunk_native_root.hpp"
namespace encore::ctr {
// The original mainCanvasLayer instance: recipe attributes, actual World2D
// membership and native lifecycle are retained separately from UI script Ready.
class PodunkStableCanvas {
 upstream::FieldGlobalRegistry*registry_=nullptr;PodunkNativeRoot*root_=nullptr;
 const upstream::FieldNodeRecipeData*recipe_=nullptr;
 upstream::FieldObjectId object_=0;upstream::FieldNodeBinding binding_{};
 upstream::FieldRecipeCanvasLayer body_{};bool entered_=false,ready_=false;
 bool reject(std::string&e,const char*s)const{e=s;return false;}
public:
 bool prepare(upstream::FieldGlobalRegistry&r,PodunkNativeRoot&root,std::string&e){
  if(registry_||!r.data()||root.kernel_object()!=r.kernel()||root.viewport_object()!=r.root())return reject(e,"Stable canvas actual native owner rejected");
  registry_=&r;root_=&root;recipe_=&r.data()->canvas_recipe();e.clear();return true;
 }
 bool candidate(const upstream::FieldNodeDescriptor&d,const upstream::FieldIdentity&i)const{
  if(!recipe_)return false;
  const auto&r=recipe_->identity();const auto*n=recipe_->record(d.id);
  return n&&d.native_class==n->native_class&&d.script==n->script&&d.script_sha==n->script_sha&&
   i.scene_id==r.scene_id&&i.upstream_commit==r.upstream_commit&&i.source_sha256==r.source_sha256;
 }
 bool owns(upstream::FieldObjectId id)const{return object_&&id==object_;}
 bool construct(upstream::FieldObjectId id,const upstream::FieldNodeDescriptor&d,const upstream::FieldIdentity&i,std::string&e){
  auto tree=registry_?registry_->tree_owner(id):nullptr;const auto*layer=recipe_?recipe_->canvas_layer(d.id):nullptr;
  if(object_||!candidate(d,i)||!layer||!tree||!tree->state(id)||d.native_class!="CanvasLayer"||!d.script.empty()||layer->custom_viewport)
   return reject(e,"Stable canvas original native constructor rejected");
  object_=id;body_=*layer;binding_={i,d.id,d.class_index,0x454e0045,1,d.script_sha,d.native_class};e.clear();return true;
 }
 bool bind(upstream::FieldObjectId id,upstream::FieldNodeBinding&b,std::string&e){
  if(!owns(id)||!registry_->object_exists(id))return reject(e,"Stable canvas actual binding missing");
  b=binding_;e.clear();return true;
 }
 bool phase(upstream::FieldObjectId id,upstream::FieldTreePhase p,std::string&e){
  auto tree=registry_?registry_->tree_owner(id):nullptr;const auto*n=tree?tree->state(id):nullptr;
  if(!owns(id)||!n||!n->alive||!registry_->object_exists(id))return reject(e,"Stable canvas lifecycle owner missing");
  using P=upstream::FieldTreePhase;
  if(p==P::EnterNative){if(entered_||!n->inside||!root_->viewport().world_registered)return reject(e,"Stable canvas World2D enter rejected");entered_=true;}
  else if(p==P::ReadyNative){if(!entered_)return reject(e,"Stable canvas Ready precedes native Enter");ready_=true;}
  else if(p==P::ExitNative){if(!entered_)return reject(e,"Stable canvas native Exit repeated");entered_=false;}
  e.clear();return true;
 }
 bool deferred(const upstream::FieldDeferredMessage&m,std::string&e){
  if(m.kind!=upstream::FieldDeferredKind::Notification||!m.args.empty())return reject(e,"Stable canvas source-less method unsupported");
  return phase(m.object,upstream::FieldTreePhase(m.notification),e);
 }
 bool release(upstream::FieldObjectId id,std::string&e){if(!owns(id)||entered_)return reject(e,"Stable canvas released before World2D exit");object_=0;ready_=false;e.clear();return true;}
 const upstream::FieldRecipeCanvasLayer*state()const{return object_?&body_:nullptr;}
};
} // namespace encore::ctr
