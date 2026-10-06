#pragma once
#include "encore/field_data.hpp"
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <variant>

namespace encore::upstream {
class FieldNodeRecipeData;
class FieldSpriteData;
using FieldObjectId=uint64_t;
using FieldTransform=std::array<Vec2,3>;
using FieldColor=std::array<float,4>;
struct FieldNodeDescriptor {
 uint32_t id=0,parent=0,owner=0,canvas_parent=0,class_index=0,ready=0,pause=0,flags=0,light_mask=0,script_methods=0;
 int32_t index=0,priority=0,z=0;
 FieldTransform local{},world{};FieldColor modulate{},self_modulate{};
 std::string path,name,script,native_class;
 bool native_generated=false;std::array<uint8_t,32>script_sha{};std::vector<std::string>groups;
};
class FieldNodeTreeData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);
 bool load_file(const char*,const FieldIdentity&,std::string&);
 bool valid()const{return valid_;}bool scene_admitted()const{return false;}
 FieldIdentity identity()const{return identity_;}const std::string&source_scene()const{return scene_;}
 const std::vector<FieldNodeDescriptor>&records()const{return records_;}
 const FieldNodeDescriptor*record(uint32_t)const;
 const std::vector<std::string>&classes()const{return classes_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 bool valid_=false;FieldIdentity identity_{};std::string scene_;std::vector<std::string>classes_;
 std::vector<FieldNodeDescriptor>records_;std::map<uint32_t,size_t>index_;
 std::map<std::string,std::array<uint8_t,32>>sources_;
};
enum class FieldTreePhase:uint32_t {
 EnterNative,EnterScript,TreeEntered,NodeAdded,ChildEntered,
 PostEnterNative,ReadyNative,ReadyScript,ReadySignal,
 ExitScript,TreeExiting,ExitNative,NodeRemoved,ChildExiting,TreeExited,
 VisibilityChanged,Hide,TransformChanged,LocalTransformChanged,Parented,Unparented,ChildMoved,Deleting,
 Idle,Physics,IdleInternal,PhysicsInternal,Input,UnhandledInput,UnhandledKeyInput,PathChanged
};
// A live source adapter owns the native subclass and every inherited script.
// None of these receipts approve an arbitrary node, missing callback or script.
struct FieldNodeBinding {
 FieldIdentity identity{};uint32_t stable_id=0,class_index=0,family=0,capability=0;
 std::array<uint8_t,32>script_sha{};
 std::string native_class;
};
struct FieldObjectRef {FieldObjectId id=0;};
// Opaque PhysicsSpace identity; a Node ObjectID is never a physics RID.
struct FieldPhysicsRid { const void *space = nullptr; uint64_t handle = 0; };
using FieldDeferredValue=std::variant<std::monostate,bool,int64_t,double,std::string,Vec2,FieldObjectRef,FieldColor,FieldPhysicsRid>;
enum class FieldDeferredKind:uint32_t {Call,Set,Notification};
struct FieldDeferredMessage {
 FieldObjectId object=0;FieldDeferredKind kind=FieldDeferredKind::Call;
 std::string member;int32_t notification=0;std::vector<FieldDeferredValue>args;
};
struct FieldNodeTreeHost {
 // Actual global registry SceneTree ObjectID; zero is independent mode.
 FieldObjectId object_domain=0;
 // The actual global ObjectDB allocator is shared across old/new scenes and
 // dynamic factories. Stable source IDs are not runtime ObjectIDs.
 std::function<bool(FieldObjectId&,std::string&)>allocate_object;
 // Register the actual native node before source script construction. This
 // never supplies script fields, parentage, Enter or Ready notifications.
 std::function<bool(FieldObjectId,const FieldNodeDescriptor&,const FieldIdentity&,std::string&)>native_allocated;
 // Source script attachment, before instance name/groups/parent/owner. The
 // concrete source owner admits ordered properties and inherited constructors.
 // New owning initialization APIs require this callback; it never grants Ready.
 std::function<bool(FieldObjectId,const FieldNodeDescriptor&,const FieldIdentity&,std::string&)>construct_source;
 std::function<bool(uint64_t&,std::string&)>allocate_fast_name;
 std::function<bool(FieldObjectId,const FieldNodeDescriptor&,FieldNodeBinding&,std::string&)>bind;
 std::function<bool(FieldObjectId,const FieldNodeBinding&,FieldTreePhase,std::string&)>dispatch;
 std::function<bool(const FieldDeferredMessage&,std::string&)>deferred;
 // Bind both to the same actual global MessageQueue, or neither for an
 // independent source kernel. Partial binding is rejected at initialize.
 std::function<bool(FieldDeferredMessage,std::string&)>enqueue_global;
 std::function<bool(std::string&)>flush_global;
 std::function<bool(FieldObjectId)>object_exists;
 // Register exactly with the actual Viewport's input group, never a fake ID.
 std::function<bool(FieldObjectId,uint32_t,bool,std::string&)>input_registration;
 // Resolve inherited pause through the actual external SceneTree parent.
 std::function<bool(FieldObjectId)>external_pause_process;
 std::function<bool(FieldObjectId,std::string_view,FieldObjectId&,std::string&)>external_path;
 std::function<bool(FieldObjectId,const FieldNodeBinding&,std::string&)>release;
};
struct FieldNodeState {
 FieldObjectId object=0,parent=0,owner=0,canvas_parent=0;uint32_t source=0;
 std::string name;std::vector<FieldObjectId>children;
 bool alive=true,inside=false,ready_first=true,ready_notified=false,queued=false,bound=false;
 uint32_t blocked=0,pause=0,flags=0;int32_t priority=0,z=0;
 FieldTransform local{},world{};FieldColor modulate{},self_modulate{};
 bool world_dirty=false;std::vector<std::string>groups;
 std::array<bool,3>input_enabled{};bool block_transform_notify=false;
 FieldNodeBinding binding{};
};
class FieldNodeTreeRuntime {
public:
 bool initialize(const FieldNodeTreeData&,FieldNodeTreeHost,std::string&);
 bool initialize_recipe(const FieldNodeRecipeData&,FieldNodeTreeHost,std::string&);
 // One checked source .new() descriptor; actual Node name stays empty until
 // source set_name/add_child. This is not arbitrary class/script evaluation.
 bool initialize_source_node(const FieldIdentity&,const FieldNodeDescriptor&,FieldNodeTreeHost,std::string&);
 bool set_name(FieldObjectId,std::string_view,std::string&);
 const FieldNodeState*state(FieldObjectId)const;
 const FieldNodeDescriptor*descriptor(FieldObjectId)const;
 bool object_identity(FieldObjectId,FieldIdentity&)const;
 FieldObjectId source_object(uint32_t)const;
 FieldObjectId root()const{return root_;}
 bool get_node(FieldObjectId,std::string_view,FieldObjectId&,std::string&)const;
 bool get_path_to(FieldObjectId,FieldObjectId,std::string&,std::string&)const;
 bool enter(std::string&);bool resume_lifecycle(std::string&);
 // Original project startup enters every staged branch before any Ready.
 // These calls retain the same live objects, source bodies and failure cursor.
 bool enter_branch_only(std::string&);
 bool ready_entered_branch(std::string&);
 bool lifecycle_pending()const{return !frames_.empty();}
 bool exit(std::string&);
 bool request_ready(FieldObjectId,std::string&);
 bool move_child(FieldObjectId,FieldObjectId,int32_t,std::string&);
 bool remove_child(FieldObjectId,FieldObjectId,std::string&);
 bool add_child(FieldObjectId,FieldObjectId,std::string&);
 // Instantiate the entire checked subtree, retaining each original node's
 // source descriptor. The factory is never a class-name/script evaluator.
 bool instantiate(const FieldNodeTreeData&,uint32_t,FieldObjectId&,std::string&);
 // Complete source recipe, including constructor-owned internal nodes.
 // Creation stays out of tree until the actual deferred add_child executes.
 bool instantiate_recipe(const FieldNodeRecipeData&,FieldObjectId&,std::string&);
 // The audited CharacterSprite body creates exactly one native AnimationTree.
 // This derives a constructor descriptor from both checked source packs and
 // the live caller; it cannot instantiate an arbitrary class or script.
 bool instantiate_builtin_source(FieldObjectId,const FieldNodeTreeData&,
                                 const FieldSpriteData&,FieldObjectId&,std::string&);
 // Same live ObjectIDs and bindings, after the actual source remove_child.
 // Global deferred messages retain identity and are not drained/recreated.
 bool transfer_detached_subtree(FieldNodeTreeRuntime&,FieldObjectId,std::string&);
 FieldObjectId object_domain()const{return host_.object_domain;}
 bool set_owner(FieldObjectId,FieldObjectId,std::string&);
 bool set_local(FieldObjectId,const FieldTransform&,std::string&);
 bool world_transform(FieldObjectId,FieldTransform&,std::string&);
 bool set_transform_notification(FieldObjectId,bool local,bool enabled,std::string&);
 bool block_transform_notifications(FieldObjectId,bool,std::string&);
 bool force_update_transform(FieldObjectId,std::string&);
 bool flush_transform_notifications(std::string&);
 bool set_visible(FieldObjectId,bool,std::string&);
 bool set_behind_parent(FieldObjectId,bool,std::string&);
 bool visible_in_tree(FieldObjectId)const;
 bool effective_color(FieldObjectId,FieldColor&,std::string&)const;
 bool effective_z(FieldObjectId,int32_t&,std::string&)const;
 bool set_modulate(FieldObjectId,const FieldColor&,bool self,std::string&);
 bool set_pause_mode(FieldObjectId,uint32_t,std::string&);
 bool set_process_priority(FieldObjectId,int32_t,std::string&);
 bool set_process(FieldObjectId,bool physics,bool enabled,std::string&);
 bool set_input_process(FieldObjectId,uint32_t kind,bool enabled,std::string&);
 bool can_process(FieldObjectId,bool tree_paused)const;
 bool group(std::string_view,std::vector<FieldObjectId>&,std::string&)const;
 bool add_group(FieldObjectId,std::string_view,std::string&);
 bool remove_group(FieldObjectId,std::string_view,std::string&);
 bool process(bool physics,bool paused,std::string&);
 bool dispatch_input(FieldObjectId,uint32_t kind,bool paused,std::string&);
 bool enqueue(FieldDeferredMessage,std::string&);
 bool flush_messages(std::string&);
 bool queue_free(FieldObjectId,std::string&);
 bool flush_delete_queue(std::string&);
 size_t object_count()const{return nodes_.size();}
private:
 struct Lifecycle {FieldObjectId id=0;uint32_t mode=0,stage=0;size_t child=0;};
 struct Deletion {FieldObjectId id=0;int32_t key=0;};
 struct OwnedSource {FieldIdentity identity{};FieldNodeDescriptor descriptor;bool recipe=false;};
 bool instantiate_records(const FieldIdentity&,const std::vector<FieldNodeDescriptor>&,uint32_t,bool,FieldObjectId&,std::string&);
 FieldNodeState*live(FieldObjectId);const OwnedSource*source(FieldObjectId)const;
 bool bind(FieldObjectId,std::string&);bool emit(FieldObjectId,FieldTreePhase,std::string&);
 bool drive(std::string&);bool destroy(FieldObjectId,std::string&);
 bool branch(FieldObjectId,uint32_t,std::string&);
 bool detach(FieldObjectId,FieldObjectId,std::string&);
 void invalidate(FieldObjectId);bool visibility_changed(FieldObjectId,bool,std::string&);
 void queue_transform(FieldObjectId);void erase_transform(FieldObjectId);
 bool ancestor(FieldObjectId,FieldObjectId)const;
 std::vector<size_t>tree_key(FieldObjectId)const;
 std::map<FieldObjectId,FieldNodeState>nodes_;
 std::map<FieldObjectId,OwnedSource>sources_;
 std::map<uint32_t,FieldObjectId>source_index_;
 std::map<std::string,std::vector<FieldObjectId>>group_index_;
 struct GroupCache {uint64_t version=0;std::vector<FieldObjectId>ordered;};
 mutable std::map<std::string,GroupCache>group_cache_;
 uint64_t order_version_=1;
 FieldNodeTreeHost host_;FieldObjectId root_=0;
 std::vector<Lifecycle>frames_;std::vector<Deletion>deletes_;
 std::deque<FieldDeferredMessage>messages_;
 std::list<FieldObjectId>transforms_;
 std::map<FieldObjectId,std::list<FieldObjectId>::iterator>transform_index_;
 bool flushing_=false,deleting_=false,driving_=false,poisoned_=false;
 enum class SplitLifecycle {None,Entering,AwaitingReady,Readying};
 SplitLifecycle split_lifecycle_=SplitLifecycle::None;
 void complete_split_lifecycle();
};
}
