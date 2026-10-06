#pragma once
#include "encore/battle_data.hpp"
#include <array>
#include <map>
#include <functional>
namespace encore::upstream {
struct FieldSceneActionReference{uint32_t id=0,kind=0,parent=0,mask=0,layer=0,descendants=0,cameras=0,viewports=0;bool collision_properties=false;std::string node,script;Vec2 position{},scale{};float rotation=0;std::array<float,6>world{};};
struct FieldSceneActionShape{uint32_t id=0;bool disabled=false;std::string node;std::vector<BattleValue>parts;};
struct FieldSceneActionObject{uint32_t source_id=0;std::string path;};
struct FieldSceneActionBinding{uint32_t id=0,kind=0,ready_ordinal=0,layer=0,mask=0,flags=0,parent_id=0,object_id=0,action=0;bool copy=false,check_state=false,set_state=false;std::string node,parent_path,object_path,method,check_flag,set_flag;std::vector<FieldSceneActionShape>shapes;std::vector<FieldSceneActionObject>objects;};
struct FieldSceneActionConnection{uint32_t kind=0;std::string signal,method;};
class FieldSceneActionsData{
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);bool valid()const{return valid_;}
 const std::array<uint8_t,20>&source_pin()const{return pin_;}const std::string&scene()const{return scene_;}
 const std::vector<FieldSceneActionBinding>&bindings()const{return bindings_;}const FieldSceneActionBinding*binding(uint32_t)const;
 const std::vector<FieldSceneActionReference>&references()const{return references_;}const FieldSceneActionReference*reference(uint32_t)const;
 const std::vector<FieldSceneActionConnection>&connections()const{return connections_;}
 const std::string&deferred_method()const{return method_;}const std::vector<std::string>&deferred_properties()const{return properties_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 std::map<std::string,std::array<uint8_t,32>>sources_;
 bool valid_=false;std::array<uint8_t,20>pin_{};std::string scene_,method_;std::vector<std::string>scripts_,properties_;
 std::vector<FieldSceneActionBinding>bindings_;std::vector<FieldSceneActionReference>references_;std::vector<FieldSceneActionConnection>connections_;
};
struct FieldSceneActionNodeState{bool live=false;uint32_t kind=0;uint64_t parent=0;};
struct FieldSceneActionState{uint32_t id=0;bool ready=false,alive=true;uint64_t parent=0,object=0;std::vector<uint64_t>objects;};
enum class FieldSceneDeferredKind:uint32_t{AddChild=1,SetMask=2,SetLayer=3};
struct FieldSceneDeferred{uint64_t token=0,target=0,item=0;FieldSceneDeferredKind kind=FieldSceneDeferredKind::AddChild;uint32_t value=0,target_kind=0,item_kind=0;bool known_absent=false;};
struct FieldSceneActionsHost{
 std::function<bool(const FieldSceneActionBinding&,const FieldSceneActionsData&,std::string&)>admit_ready;
 // Resolve actual NodePath during onready/Ready. Return ObjectID=0 only for
 // known get_node_or_null absence. Cached nonzero freed identities are errors
 // during script evaluation, never re-resolved to a replacement object.
 std::function<bool(uint32_t origin,std::string_view path,uint32_t expected_source,uint64_t&,std::string&)>resolve;
 std::function<bool(uint64_t,FieldSceneActionNodeState&,std::string&)>describe;
 std::function<bool(uint32_t body,bool&,std::string&)>body_is_player;
 // Preflight ALL actual moving leaf nodes, full dynamic map/shape ownership,
 // preserve local transforms/inherited canvas depth, true Tree exit/enter
 // without repeated Ready, queue room and typed masks, before any mutation.
 std::function<bool(const FieldSceneActionBinding&,const FieldSceneActionState&,std::string&)>admit_reparent;
 std::function<bool(uint64_t parent,uint64_t item,std::string&)>remove_child,add_child;
 std::function<bool(uint64_t,uint32_t role,uint32_t&,std::string&)>read_collision;
 std::function<bool(uint64_t,uint32_t role,uint32_t value,std::string&)>set_collision;
 // Register in the actual global MessageQueue FIFO and return its unique
 // monotonic token. That queue calls flush_deferred for THIS entry in order;
 // no private all-actions flush that could reorder other scene consumers.
 std::function<bool(const FieldSceneDeferred&,uint64_t&,std::string&)>enqueue;
 std::function<bool(const FieldSceneActionBinding&,const FieldSceneActionState&,std::string&)>admit_event;
 std::function<bool(uint64_t,uint32_t action,std::string_view source_method,bool&,std::string&)>has_method;
 std::function<bool(uint64_t,uint32_t action,std::string&)>call_method;
 std::function<bool(std::string_view,bool&exists,bool&value,std::string&)>read_flag;
 std::function<bool(std::string_view,bool,std::string&)>write_existing_flag;
};
class FieldSceneActionsRuntime{
public:
 bool initialize(const FieldSceneActionsData&,FieldSceneActionsHost,std::string&);
 bool ready(uint32_t,std::string&);bool body_enter(uint32_t,uint32_t body,std::string&);
 bool reparent(uint32_t,std::string&);bool activate_event(uint32_t,std::string&);bool set_flag(uint32_t,std::string&);
 bool flush_deferred(uint64_t token,std::string&);bool exit_tree(uint32_t,std::string&);
 const FieldSceneActionState*state(uint32_t)const;const std::vector<FieldSceneDeferred>&pending()const{return pending_;}
 const FieldSceneActionsData*content()const{return data_;}
private:
 FieldSceneActionState*active(uint32_t,std::string&);bool queue(FieldSceneDeferred,std::string&);
 const FieldSceneActionsData*data_=nullptr;FieldSceneActionsHost host_;size_t ready_index_=0;uint64_t last_token_=0;
 std::vector<FieldSceneActionState>states_;std::vector<FieldSceneDeferred>pending_;
};
}
