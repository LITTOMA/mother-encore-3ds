#pragma once
#include "encore/field_global_registry.hpp"
namespace encore::upstream {
// Complete serialized native/Variant graph. Type tags are resource data only;
// no method/opcode/script evaluator or SceneReady approval exists here.
enum class FieldBgValueKind:uint8_t {Nil,Boolean,Integer,Real,String,Array,Map};
struct FieldBgValue {
 FieldBgValueKind kind=FieldBgValueKind::Nil;bool boolean=false;int64_t integer=0;double real=0;
 std::string text;std::vector<FieldBgValue>array;
 std::vector<std::pair<std::string,FieldBgValue>>map;
 const FieldBgValue*member(std::string_view)const;
};
struct FieldBgPackedGraph {uint32_t id=0;std::string path,key;std::array<uint8_t,32>sha{};FieldBgValue graph;};
struct FieldBgResourceAlias {uint32_t scene=0,local=0;std::string source_object_id;};
struct FieldBgTexturePayload {std::string path;std::array<uint8_t,32>sha{};uint32_t width=0,height=0;std::vector<uint8_t>png;};
class FieldBattleBgData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);
 bool load_file(const char*,const FieldIdentity&,std::string&);
 bool valid()const{return valid_;}FieldIdentity identity()const{return identity_;}
 const std::vector<FieldBgPackedGraph>&resources()const{return scenes_;}
 const FieldBgPackedGraph*resource(uint32_t)const;
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
 const FieldBgTexturePayload*texture(std::string_view)const;
 bool resource_alias(uint32_t scene,uint32_t local,FieldBgResourceAlias&)const;
private:
 bool valid_=false;FieldIdentity identity_{};std::string directory_;
 std::map<std::string,std::array<uint8_t,32>>sources_;
 std::map<std::pair<uint32_t,uint32_t>,FieldBgResourceAlias>aliases_;
 std::vector<FieldBgPackedGraph>scenes_;std::vector<FieldBgTexturePayload>textures_;
};
// Actual PackedScene Resource owns a checked complete immutable original
// SceneState/resource graph and texture codec bytes. It does not enter Tree.
class FieldBattleBgPackedScene final:public FieldGlobalSourceResource {
public:
 FieldBattleBgPackedScene(FieldGlobalExternalBinding,std::shared_ptr<const FieldBattleBgData>,uint32_t);
 const char*resource_class()const override{return "PackedScene";}
 FieldGlobalExternalBinding binding()const override{return binding_;}
 bool state(FieldGlobalExternalState&,std::string&)const override;
 bool deferred(const FieldDeferredMessage&,std::string&)override;
 bool persist_append(FieldObjectId,std::string&)override;
 bool assign_stable_canvas(FieldObjectId,std::string&)override;
 const FieldBgPackedGraph*graph()const;
 // Source graph class adapters/renderers must be bound before instantiation.
 bool instance(FieldNodeTreeRuntime&,FieldObjectId&,std::string&)const;
private:FieldGlobalExternalBinding binding_{};std::shared_ptr<const FieldBattleBgData>data_;uint32_t source_=0;
};
class FieldBattleBgResources {
public:
 bool initialize(std::shared_ptr<const FieldBattleBgData>,FieldGlobalRegistry&,std::string&);
 // Original Directory order retained. Source Dictionary owns actual ObjectIDs.
 // Repeating after a partial failure resumes without recreating earlier values.
 bool load(std::map<std::string,FieldObjectId>&,std::string&);
 bool complete()const{return data_&&cursor_==data_->resources().size();}
 FieldIdentity identity()const{return data_?data_->identity():FieldIdentity{};}
 FieldObjectId object_domain()const{return registry_?registry_->kernel():0;}
 bool source_hash(std::string_view p,std::array<uint8_t,32>&h)const{return data_&&data_->source_hash(p,h);}
 bool owns(const std::map<std::string,FieldObjectId>&,std::string&)const;
 const std::vector<std::pair<std::string,FieldObjectId>>&insertion_order()const{return order_;}
private:std::shared_ptr<const FieldBattleBgData>data_;FieldGlobalRegistry*registry_=nullptr;
 size_t cursor_=0;std::map<std::string,FieldObjectId>loaded_;
 std::map<std::string,const FieldBattleBgPackedScene*>owners_;
 std::vector<std::pair<std::string,FieldObjectId>>order_;
};
}
