#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
namespace encore::upstream {
using FieldTintColor=std::array<float,4>;
enum class FieldTintKind:uint32_t {Scene=1,EnemyPrototype=2,ActorPrototype=3};
struct FieldTintTarget {uint32_t source_id=0;bool exists=false;FieldTintColor initial_self_modulate{};std::string node_path,node,kind;};
struct FieldTintDescriptor {uint32_t id=0,scene_id=0,ready_ordinal=0;FieldTintKind kind=FieldTintKind::Scene;std::string scene,node;std::vector<FieldTintTarget>targets;};
class FieldTintData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}uint32_t scene_id()const{return scene_;}const std::array<uint8_t,20>&source_pin()const{return pin_;}
 const FieldTintColor&default_tint()const{return default_;}const std::vector<FieldTintDescriptor>&records()const{return records_;}
 const FieldTintDescriptor*record(uint32_t)const;const FieldTintDescriptor*prototype(FieldTintKind)const;
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:bool valid_=false;uint32_t scene_=0;std::array<uint8_t,20>pin_{};FieldTintColor default_{};std::vector<FieldTintDescriptor>records_;std::map<std::string,std::array<uint8_t,32>>sources_;
};
struct FieldTintHost {
 // Resolve only the checked source child path within this actual instance.
 // A missing node requires an actual source lifecycle removal/null lookup;
 // an unimplemented child adapter must reject instead of returning absent.
 std::function<bool(uint32_t,const FieldTintDescriptor&,const FieldTintTarget&,bool&,uint32_t&,std::string&)>resolve;
 // CanvasItem.self_modulate colors only that target, never its descendants.
 std::function<bool(uint32_t,FieldTintColor,std::string&)>self_modulate;
};
struct FieldTintInstance {uint32_t id=0,descriptor_id=0;bool ready=false;FieldTintColor tint{};std::vector<uint32_t>targets,connections;};
enum class FieldTintEventKind:uint8_t {Ready,SelfModulate,ChangedTint,Connect};
struct FieldTintEvent {FieldTintEventKind kind{};uint32_t sender=0,target=0;FieldTintColor color{};};
class FieldTintRuntime {
public:
 const FieldTintData*data()const{return data_;}
 bool initialize(const FieldTintData&,FieldTintHost,std::string&);
 // Scene instance ID equals its source descriptor; enemy/actor instance IDs
 // come from the existing authoritative factory, never generated here.
 bool create(uint32_t descriptor_id,uint32_t instance_id);bool ready(uint32_t);
 bool set_tint(uint32_t,FieldTintColor);bool connect_tint(uint32_t sender,uint32_t receiver);bool destroy(uint32_t);
 const FieldTintInstance*instance(uint32_t)const;const std::vector<FieldTintEvent>&events()const{return events_;}
 std::vector<FieldTintEvent>take_events(){auto out=std::move(events_);events_.clear();return out;}
 const std::string&error()const{return error_;}
private:
 const FieldTintData*data_=nullptr;FieldTintHost host_;std::map<uint32_t,FieldTintInstance>instances_;std::vector<FieldTintEvent>events_;std::vector<uint32_t>active_signal_;std::string error_;
 bool fail(const char*);bool populate(FieldTintInstance&);bool reachable(uint32_t,uint32_t,std::vector<uint32_t>&)const;
};
}
