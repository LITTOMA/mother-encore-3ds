#include "encore/field_battle_bg_resources.hpp"
namespace encore::upstream {
namespace {bool fail(std::string&e,const char*s){e=s;return false;}}
const FieldBgValue*FieldBgValue::member(std::string_view k)const{if(kind!=FieldBgValueKind::Map)return nullptr;for(const auto&v:map)if(v.first==k)return &v.second;return nullptr;}
bool FieldBattleBgData::source_hash(std::string_view p,std::array<uint8_t,32>&h)const{auto i=sources_.find(std::string(p));if(i==sources_.end())return false;h=i->second;return true;}
const FieldBgPackedGraph*FieldBattleBgData::resource(uint32_t id)const{for(const auto&r:scenes_)if(r.id==id)return &r;return nullptr;}
const FieldBgTexturePayload*FieldBattleBgData::texture(std::string_view path)const{for(const auto&t:textures_)if(t.path==path)return &t;return nullptr;}
bool FieldBattleBgData::resource_alias(uint32_t scene,uint32_t local,FieldBgResourceAlias&out)const{auto i=aliases_.find({scene,local});if(i==aliases_.end())return false;out=i->second;return true;}
FieldBattleBgPackedScene::FieldBattleBgPackedScene(FieldGlobalExternalBinding b,std::shared_ptr<const FieldBattleBgData>d,uint32_t id):binding_(std::move(b)),data_(std::move(d)),source_(id){}
const FieldBgPackedGraph*FieldBattleBgPackedScene::graph()const{return data_&&data_->valid()?data_->resource(source_):nullptr;}
bool FieldBattleBgPackedScene::state(FieldGlobalExternalState&s,std::string&e)const{
 const auto*g=graph();if(!g||!binding_.object||binding_.source.native_class!="PackedScene"||binding_.source.source!=g->path||binding_.source.source_sha!=g->sha||binding_.source.identity.scene_id!=g->id||binding_.source.identity.source_sha256!=g->sha||binding_.source.identity.upstream_commit!=data_->identity().upstream_commit)return fail(e,"Original battle PackedScene actual owned graph rejected");
 s={};s.name=binding_.source.name;e.clear();return true;
}
bool FieldBattleBgPackedScene::deferred(const FieldDeferredMessage&,std::string&e){return fail(e,"Battle PackedScene Resource is not a live Node");}
bool FieldBattleBgPackedScene::persist_append(FieldObjectId,std::string&e){return fail(e,"Battle PackedScene Resource cannot enter persistNodes");}
bool FieldBattleBgPackedScene::assign_stable_canvas(FieldObjectId,std::string&e){return fail(e,"Battle PackedScene Resource does not own stableCanvas");}
bool FieldBattleBgPackedScene::instance(FieldNodeTreeRuntime&,FieldObjectId&,std::string&e)const{return fail(e,"Original full battle native BackBufferCopy/material/Shader graph instance adapters pending");}
bool FieldBattleBgResources::initialize(std::shared_ptr<const FieldBattleBgData>d,FieldGlobalRegistry&r,std::string&e){if(data_||!d||!d->valid()||!r.kernel())return fail(e,"Battle resource source data/actual registry rejected");data_=std::move(d);registry_=&r;e.clear();return true;}
bool FieldBattleBgResources::owns(const std::map<std::string,FieldObjectId>&map,std::string&e)const{
 if(!complete()||!registry_||map!=loaded_||owners_.size()!=data_->resources().size())return fail(e,"Battle source dictionary complete factory ownership rejected");
 for(const auto&item:map){auto owner=owners_.find(item.first);FieldGlobalExternalState state;if(owner==owners_.end()||registry_->source_resource(item.second)!=owner->second||!owner->second->state(state,e)||state.parent||state.inside||state.ready)return fail(e,"Battle source dictionary actual PackedScene ObjectDB owner rejected");}e.clear();return true;
}
bool FieldBattleBgResources::load(std::map<std::string,FieldObjectId>&out,std::string&e){
 if(!data_||!registry_)return fail(e,"Battle source resource loader not initialized");
 for(const auto&v:loaded_)if(!registry_->source_resource(v.second))return fail(e,"Battle source PackedScene ObjectDB ownership lost");
 while(cursor_<data_->resources().size()){
  const auto&s=data_->resources()[cursor_];FieldGlobalExternalSpec spec;spec.identity=data_->identity();spec.identity.scene_id=s.id;spec.identity.source_sha256=s.sha;spec.stable_id=s.id;spec.role=4;spec.native_class="PackedScene";spec.source=s.path;spec.source_sha=s.sha;
  FieldObjectId id=0;if(!registry_->allocate_object(id,e))return false;FieldGlobalExternalBinding b;b.object=id;b.source=spec;b.family=0x454e004a;b.capability=1;
  auto owner=std::make_unique<FieldBattleBgPackedScene>(b,data_,s.id);auto actual=owner.get();if(!registry_->publish_source_resource(spec,id,std::move(owner),e))return false;
  loaded_[s.key]=id;owners_[s.key]=actual;order_.emplace_back(s.key,id);++cursor_;
 }out=loaded_;e.clear();return true;
}
}
