#pragma once
#include "encore/field_node_recipe.hpp"
#include <memory>
namespace encore::upstream {
struct FieldGlobalAutoload {
 uint32_t id=0,kind=0,ordinal=0;
 std::string name,path,native_class,script;
 std::array<uint8_t,32>source_sha{},script_sha{};
};
struct FieldGlobalFunctionProof {std::string source,method;uint32_t line=0;std::array<uint8_t,32>sha{};};
class FieldGlobalRegistryData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);
 bool load_file(const char*,const FieldIdentity&,std::string&);
 bool valid()const{return valid_;}bool scene_admitted()const{return false;}
 FieldIdentity identity()const{return identity_;}
 const std::string&root_name()const{return root_name_;}
 const std::string&root_native()const{return root_native_;}
 const std::string&kernel_native()const{return kernel_native_;}
 const std::string&main_scene()const{return main_scene_;}
 const std::string&engine_commit()const{return engine_commit_;}
 uint64_t initial_object_counter()const{return object_counter_;}
 uint64_t initial_fast_name_counter()const{return fast_counter_;}
 uint32_t ui_autoload()const{return ui_;}uint32_t global_autoload()const{return global_;}
 const std::vector<std::string>&player_parents()const{return player_parents_;}
 const std::vector<FieldGlobalAutoload>&autoloads()const{return autoloads_;}
 const std::vector<FieldGlobalFunctionProof>&functions()const{return functions_;}
 const FieldNodeRecipeData&canvas_recipe()const{return canvas_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
 bool engine_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 bool valid_=false;FieldIdentity identity_{};
 uint64_t object_counter_=0,fast_counter_=0;uint32_t ui_=0,global_=0;
 std::string root_name_,root_native_,kernel_native_,main_scene_,engine_commit_;
 std::vector<std::string>player_parents_;std::vector<FieldGlobalAutoload>autoloads_;
 std::vector<FieldGlobalFunctionProof>functions_;
 std::map<std::string,std::array<uint8_t,32>>sources_,engine_sources_;
 FieldNodeRecipeData canvas_;
};
// The registry owns these actual native/source adapter objects, never IDs
// provided by a caller. Implementations must expose live state, not constants
// that claim a missing script constructor, Ready or Viewport is implemented.
struct FieldGlobalExternalSpec {
 FieldIdentity identity{};uint32_t stable_id=0,role=0;
 std::string name,native_class,source,script;
 std::array<uint8_t,32>source_sha{},script_sha{};
};
struct FieldGlobalExternalBinding {
 FieldObjectId object=0;FieldGlobalExternalSpec source;
 uint32_t family=0,capability=0;
};
struct FieldGlobalExternalState {
 std::string name;FieldObjectId parent=0;
 std::vector<FieldObjectId>children;
 bool inside=false,ready=false;
 // True only at the actual original UiManager._ready source cursor after
 // onready instances + randomize/menu_flavors/_load_battle_bgs complete.
 bool ui_before_canvas=false;
 FieldObjectId current_scene=0,stable_canvas=0;
};
class FieldGlobalExternalObject {
public:
 virtual ~FieldGlobalExternalObject()=default;
 virtual FieldGlobalExternalBinding binding()const=0;
 virtual bool state(FieldGlobalExternalState&,std::string&)const=0;
 virtual bool deferred(const FieldDeferredMessage&,std::string&)=0;
 virtual bool persist_append(FieldObjectId,std::string&)=0;
 virtual bool assign_stable_canvas(FieldObjectId,std::string&)=0;
};
// Actual non-Node Resource owner. A PackedScene retains its checked complete
// recipe; this interface never grants native/script lifecycle admission.
class FieldGlobalSourceResource:public FieldGlobalExternalObject {
public:
 virtual const char*resource_class()const=0;
};
class FieldGlobalRegistry;
// Native Reference lifetime is owned by source Ref holders, not the ObjectDB.
// This is independent of Node lifecycle and Resource loading.
class FieldGlobalNativeReference {
public:
 virtual ~FieldGlobalNativeReference()=default;
 virtual FieldGlobalExternalBinding binding()const=0;
 virtual const char*native_class()const=0;
 virtual const FieldGlobalRegistry*registry()const=0;
 virtual bool checked_source_hash(std::string_view,std::array<uint8_t,32>&)const=0;
};
struct FieldGlobalRegistryHost {
 std::function<bool(FieldObjectId,const FieldGlobalExternalSpec&,std::unique_ptr<FieldGlobalExternalObject>&,std::string&)>construct;
};
class FieldGlobalRegistry {
public:
 using NodeDispatch=std::function<bool(const FieldDeferredMessage&,std::string&)>;
 bool initialize(const FieldGlobalRegistryData&,FieldGlobalRegistryHost,std::string&);
 bool allocate_object(FieldObjectId&,std::string&);
 bool allocate_fast_name(uint64_t&,std::string&);
 bool object_exists(FieldObjectId)const;
 std::shared_ptr<FieldNodeTreeRuntime>tree_owner(FieldObjectId)const;
 bool retire_object(FieldObjectId,std::string&);
 // Invoke after actual source Tree deletion flush; removes dead ObjectDB
 // entries and releases old kernel ownership without touching live waiters.
 size_t collect_dead_tree_objects();
 // Register only actual checked Tree objects whose ObjectIDs were allocated
 // by this same registry. No hash IDs or copied metadata proxy nodes.
 bool publish_source_resource(const FieldGlobalExternalSpec&,FieldObjectId,std::unique_ptr<FieldGlobalSourceResource>,std::string&);
 const FieldGlobalSourceResource*source_resource(FieldObjectId)const;
 bool publish_native_reference(const FieldGlobalExternalSpec&,FieldObjectId,
                               const std::shared_ptr<FieldGlobalNativeReference>&,std::string&);
 std::shared_ptr<const FieldGlobalNativeReference>native_reference(FieldObjectId)const;
 bool publish_branch(std::shared_ptr<FieldNodeTreeRuntime>,FieldObjectId,NodeDispatch,std::string&);
 bool attach_scene(FieldObjectId,std::string&);
 // Called by the actual native root only after its real child list changes.
 // Makes the source parent visible to NodePath lookup before Enter/Ready.
 bool observe_external_parent(FieldObjectId parent,FieldObjectId child,std::string&);
 bool detach_scene(FieldObjectId,std::string&);
 bool persistent_reparent(FieldObjectId,FieldObjectId,std::string&);
 bool construct_autoload(uint32_t,std::string&);
 bool attach_autoload(uint32_t,std::string&);
 bool select_current_scene(FieldNodeTreeRuntime&,FieldObjectId,std::string&);
 // Observe the actual source global assignment before root.add_child/Ready.
 bool observe_global_current_scene(FieldObjectId,std::string&);
 // Source SceneTree.set_current_scene occurs later, after persistent moves.
 bool observe_tree_current_scene(FieldObjectId,std::string&);
 // Original SceneTree.add_current_scene assigns native identity before
 // out-of-tree root.add_child; global.currentScene is assigned later in Ready.
 bool observe_bootstrap_tree_current_scene(FieldObjectId,std::string&);
 bool create_stable_canvas(FieldObjectId&,std::string&);
 bool lookup_absolute(std::string_view,FieldObjectId&,std::string&)const;
 bool resolve_path(FieldObjectId,std::string_view,FieldObjectId&,std::string&)const;
 bool get_path_to(FieldObjectId,FieldObjectId,std::string&,std::string&)const;
 bool get_path(FieldObjectId,std::string&,std::string&)const;
 bool enqueue(FieldDeferredMessage,std::string&);
 bool flush_messages(std::string&);
 bool dispatch(const FieldDeferredMessage&,std::string&);
 FieldObjectId root()const{return root_;}FieldObjectId kernel()const{return kernel_;}
 FieldObjectId stable_canvas()const{return stable_canvas_;}
 FieldObjectId current_scene()const{return current_scene_;}
 FieldObjectId tree_current_scene()const{return tree_current_scene_;}
 size_t object_count()const{return objects_.size();}
 bool poisoned()const{return poisoned_;}
private:
 struct Object {
  std::shared_ptr<FieldNodeTreeRuntime>tree;NodeDispatch dispatch;
  std::unique_ptr<FieldGlobalExternalObject>external;
  const FieldGlobalSourceResource*resource=nullptr;
  std::weak_ptr<FieldGlobalNativeReference>reference;
  bool reference_published=false;
  uint32_t definition=0;FieldObjectId external_parent=0;
 };
 bool snapshot(FieldObjectId,FieldGlobalExternalState&,std::string&)const;
 bool construct(const FieldGlobalExternalSpec&,FieldObjectId&,std::string&);
 bool equal_spec(const FieldGlobalExternalSpec&,const FieldGlobalExternalSpec&)const;
 const FieldGlobalRegistryData*data_=nullptr;FieldGlobalRegistryHost host_;
 std::map<FieldObjectId,Object>objects_;
 std::map<uint32_t,FieldObjectId>autoload_objects_;
 std::deque<FieldDeferredMessage>messages_;
 uint64_t counter_=0,fast_counter_=0;
 FieldObjectId kernel_=0,root_=0,current_scene_=0,tree_current_scene_=0,stable_canvas_=0;
 bool initialized_=false,poisoned_=false,flushing_=false;
};
}
