#pragma once
#include "encore/field_node_tree.hpp"
#include "encore/house_data.hpp"
#include "encore/world.hpp"
#include "encore/phone_runtime.hpp"
#include "encore/dialogue_choices.hpp"
#include "encore/house_inspection_data.hpp"
#include "encore/drawer_program.hpp"
#include "encore/basement_progression.hpp"
#include "encore/basement_actor_assets.hpp"
#include "encore/field_door.hpp"
#include "encore/field_npc.hpp"
#include "encore/field_interact_dialog.hpp"
#include "encore/house_reentry.hpp"
#include <set>
namespace encore::upstream {
class HousePresentation;
class HouseRuntime;
class FieldGlobalRegistry;
// Borrowed actual source caller. Neither this receipt nor admission executes
// a constructor/Ready or replaces the existing Room VM.
struct HouseSourceNpcProgramme {
 const FieldNpcRuntime*source=nullptr;
 const FieldNodeTreeRuntime*tree=nullptr;
 const FieldNodeTreeData*tree_data=nullptr;
 const FieldGlobalRegistry*registry=nullptr;
 const HouseReentryData*reentry=nullptr;
 const FieldDoorData*doors=nullptr;
 HouseView house{};
 FieldObjectId object=0;
 uint32_t source_id=0,original_npc=house_no_index,programme=house_no_index;
 bool thoughts=false;
};
class HouseSourceInteractProgrammeCaller;
struct HouseSourceInteractProgramme {
 const HouseSourceInteractProgrammeCaller*caller=nullptr;
 const FieldInteractRuntime*source=nullptr;
 const FieldInteractData*data=nullptr;
 const FieldNodeTreeRuntime*tree=nullptr;
 const FieldNodeTreeData*tree_data=nullptr;
 const FieldGlobalRegistry*registry=nullptr;
 const HouseReentryData*reentry=nullptr;
 const FieldDoorData*doors=nullptr;
 HouseView house{};
 FieldObjectId object=0;
 uint32_t source_id=0,programme=house_no_index;
 std::string_view dialogue;
 bool thoughts=false;
};
// Reads the current InteractDialog invocation and its actual flag selection.
// A caller cannot replace this with a copied programme index or an NPC body.
class HouseSourceInteractProgrammeCaller {
public:
 virtual ~HouseSourceInteractProgrammeCaller()=default;
 virtual bool observe(const HouseSourceInteractProgramme&,
                      HouseSourceInteractProgramme&,std::string&)const=0;
};
enum class HouseProgrammePhase:uint8_t {Closed,Opening,WaitingReady,Starting,Running,Failed};
// Read from the actual native programme owner. A default receipt cannot admit
// a closed source frame or grant responsibility for the printer/VM.
struct HouseProgrammeState {
 const HouseRuntime*runtime=nullptr;
 const OpeningWorld*world=nullptr;
 const OpeningHouseProgrammeOwner*world_owner=nullptr;
 const HousePresentation*printer=nullptr;
 const DialogueChoices*choices=nullptr;
 RoomView room{};HouseView house{};
 HouseProgrammePhase phase=HouseProgrammePhase::Failed;
 uint32_t programme=kRoomNoIndex,original_npc=kRoomNoIndex;
 uint32_t request_generation=0,vm_generation=0;
 bool native_closed=false;
};
class HouseProgrammeOwner {
public:
 virtual ~HouseProgrammeOwner()=default;
 virtual bool request(uint32_t programme,uint32_t original_npc,std::string&)=0;
 virtual bool observe_house_programme(HouseProgrammeState&,std::string&)const=0;
};
// All item identities and sound bindings come from the admitted basement packs.
// The session host owns keybag mutations and the current-item text context.
struct HouseBasementHost {
 std::function<bool(const BasementKeyItem&,std::string&)> validate_key_item,select_key_item,remove_key_item,grant_key_item;
 std::function<bool(const BasementKeyItem&,bool&,std::string&)> key_owned;
 std::function<bool(const BasementActorData&,std::string&)> validate_present_sound;
 std::function<bool(const BasementActorData&,bool,std::string&)> present_sound;
};
enum class HousePhase:uint8_t {Idle,DoorAwaitIdle,DoorFadeIn,WarpAwaitIdle,DoorFadeOut,Dialogue,StoryBoundary,StoryRunning,Unsupported,Error,InspectionProgram,SceneDoorPending};
enum class HouseEventKind:uint8_t {Paused,DoorStarted,DoorEntered,PlayerMoved,FadeOutStarted,DoorDone,DialogueOpened,DialogueSeen,DialogueClosed,OpenableOpened,OpenableUnlocked,OpenableFlagWritten,OpenableNormal,DoorDialogueOpened,StoryRequested,SceneDoorRequested};
struct HouseEvent {HouseEventKind kind{};uint32_t object=0;uint64_t physics_tick=0,idle_frame=0;Vec2 position{};};
struct HouseSceneDoorRequest {uint32_t boundary=house_no_index,door=0;uint64_t player=0;};
struct HouseSoundRequest {uint32_t sound=0;};
struct HouseOpenableState {
 bool blocked=false,locked=false,unlocked=false,one_way=false,inside=false,sprite_visible=true,player_disabled=false,nonplayer_disabled=false;
 bool action=false,pending_action=false,pending_normal=false,animation_pending=false,collision_pending=false,collision_value=false,timer_running=false;
 double timer_remaining=0,animation_time=0;
};
class HouseRuntime : public PhoneFlagQuery, public PhoneSoundSink, private DrawerHost {
public:
 bool initialize(HouseView,OpeningWorld&,HousePresentation&);
 bool rebind_scene(OpeningWorld&,HousePresentation&);
 bool bind_programme_owner(HouseProgrammeOwner&,std::string&);
 // Release the core borrow only. Keep the native printer callback until the
 // same World owner unbinds; then rebind_scene restores the legacy receiver.
 bool unbind_programme_owner(HouseProgrammeOwner&,std::string&);
 const HouseProgrammeOwner*programme_owner()const{return programme_owner_;}
 bool observe_programme_owner(HouseProgrammeState&state,std::string&error)const{return programme_state(state,error);}
 // npc.gd's already selected ordinary programme enters the SAME request lease.
 // The caller has performed its source mark_seen/pauseForInteract before open.
 bool admit_source_npc_programme(const HouseSourceNpcProgramme&,std::string&)const;
 bool request_source_npc_programme(const HouseSourceNpcProgramme&,std::string&);
 bool admit_source_interact_programme(const HouseSourceInteractProgramme&,std::string&)const;
 bool request_source_interact_programme(const HouseSourceInteractProgramme&,std::string&);
 // Borrow the existing source effects; these getters execute no inventory or VM.
 DrawerProgramView drawer_source()const{return drawer_;}
 DrawerHost*drawer_effects()const{return drawer_effects_;}
 const OpeningWorld*world_owner()const{return world_;}
 std::string_view source_player_nickname()const{return player_nickname();}
 // Read-only Drawer admission borrows the SAME reviewed House/Room metadata.
 // Native Inventory consumers cannot call the private legacy Drawer executor.
 bool validate_text(uint32_t,std::string&)override;
 bool validate_flag(std::string_view,std::string&)override;
 bool flag(std::string_view,bool&,std::string&)override;
 bool restore_seen_dialogue(const std::set<uint32_t>&);
 bool set_player_nickname(std::string_view);
 bool bind_phone(PhoneRuntime&);
 // Requires the same actual Ready Door runtime and an actual player ObjectID.
 bool bind_scene_doors(const FieldDoorData&,FieldDoorRuntime&,std::function<bool(uint64_t&,std::string&)>,std::string&);
 // Resolve a lazy destination at the actual boundary contact, before routing
 // that contact. The callback must bind its checked Door or report failure;
 // unrecognized boundaries retain their existing unsupported behavior.
 void set_scene_door_preparer(std::function<bool(std::string_view,std::string&)> prepare){scene_door_preparer_=std::move(prepare);}
 std::vector<HouseSceneDoorRequest>take_scene_door_requests(){auto out=std::move(scene_door_requests_);scene_door_requests_.clear();return out;}
 // Cross-pack admission is transactional: a rejected candidate keeps the
 // previous inspection binding and current interaction intact.
 bool bind_inspections(HouseInspectionView);
 // Independent effect programme owns its source identity, inspector and text
 // refs. Bind before inspections; effects owns inventory/audio, never text/flags.
 bool bind_drawer(DrawerProgramView,DrawerHost& effects);
 bool bind_basement(const BasementProgressionData&,const BasementActorData&,HouseBasementHost,std::string&);
 bool basement_bound()const{return basement_!=nullptr;}
 const BasementActorPlayback&basement_present_playback()const{return basement_present_;}
 uint32_t basement_present_frame()const;
 bool basement_present_opened()const;
 const BasementActorResource*basement_present_resource()const;
 void bind_choices(const DialogueChoicesData&data,DialogueChoices&model){choices_data_=&data;choices_=&model;}
 bool select_story_option(uint32_t pc,uint32_t generation);
 bool close_story_submenu(uint32_t generation);
 bool get_phone_flag(uint32_t,bool&)const override;
 bool play_phone_sound(const PhoneSoundRequest&)override;
 const std::vector<PhoneSoundRequest>&phone_sounds()const{return phone_sounds_;}
 bool before_physics(WalkInput&);
 bool after_physics();
 bool idle_frame(double delta,bool accept,bool cancel);
 HousePhase phase()const{return phase_;}
 bool blocks_player()const;
 bool npc_interaction_supported(uint32_t)const;bool door_interaction_supported(uint32_t)const;bool phone_interaction_supported(uint32_t)const;
 bool inspection_interaction_supported(uint32_t)const;
 bool inspection_visible(uint32_t)const;
 Vec2 inspection_position(uint32_t)const;
 HouseInspectionView inspections()const{return inspections_;}
 bool entering_door()const;
 float fade_alpha()const;
 BattleValue fade_color()const;
 const char* error()const{return error_;}
 uint32_t active_object()const{return active_;}
 const HouseOpenableState& openable_state(uint32_t index)const{return openables_[index];}
 bool story_executing()const{return story_executing_;}
 bool story_pending()const{return story_index_!=house_no_index;}
 uint32_t story_index()const{return story_index_;}
 std::string_view story_path()const{return story_pending()?content_.string(content_.story_trigger(story_index_).dialogue):std::string_view{};}
 bool seen_dialogue(uint32_t key)const{return seen_.count(key)!=0;}
 const std::set<uint32_t>&seen_dialogue_keys()const{return seen_;}
 const std::vector<HouseEvent>&events()const{return events_;}
 std::vector<HouseSoundRequest>take_sounds(){auto result=std::move(sounds_);sounds_.clear();return result;}
private:
 bool fail(const char*);void event(HouseEventKind,uint32_t);
 std::string_view player_nickname()const;
 std::string nickname_;
 bool overlaps(Vec2 center,Vec2 extents,Vec2 player)const;
 bool overlaps_circle(Vec2 center,float radius,Vec2 player)const;
 bool update_openable_contacts(Vec2 player);
 bool advance_openables(double delta);
 bool open_openable(uint32_t index);
 bool normal_openable(uint32_t index);
 bool interact_openable(uint32_t index);
 bool interact_phone(uint32_t index);
 bool interact_basement_present();
 bool begin_basement_program(std::string_view,uint32_t);
 bool advance_basement_present(double);
 bool interact_inspection(uint32_t index);
 bool resolve_inspection_dialogue(uint32_t,uint32_t&)const;
 std::string_view inspection_path(uint32_t)const;
 bool drawer_selected(uint32_t)const;
 bool validate_item(DrawerItemTemplate t,std::string_view n,std::string&e)override{return drawer_effects_&&drawer_effects_->validate_item(t,n,e);}
 bool validate_sound(std::string_view n,std::string&e)override{return drawer_effects_&&drawer_effects_->validate_sound(n,e);}
 bool show_text(uint32_t,std::string&)override;
 bool inventory_space()const override{return drawer_effects_&&drawer_effects_->inventory_space();}
 bool grant_item(DrawerItemTemplate t,std::string_view n,std::string&e)override{return drawer_effects_&&drawer_effects_->grant_item(t,n,e);}
 bool play_sound(std::string_view n,std::string&e)override{return drawer_effects_&&drawer_effects_->play_sound(n,e);}
 bool set_flag(std::string_view,bool,std::string&)override;
 uint32_t program_for_path(std::string_view)const;
 bool story_conditions(uint32_t index)const;
 bool sync_npc_visibility();
 bool process_npc_restores();
 Vec2 npc_point(uint32_t index,Vec2 original)const;
 bool process_story_requests();
 bool deliver_area_contacts();
 bool sync_story_dialogue();bool advance_story_dialogue(bool automatic);bool text_finished();
 bool request_programme(uint32_t,uint32_t original_npc=house_no_index);
 bool programme_state(HouseProgrammeState&,std::string&)const;
 bool native_programme_complete(bool&);
 HouseProgrammeOwner*programme_owner_=nullptr;
 // A checked request lease, never a mirrored VM/coroutine state.
 uint32_t programme_lease_=0,requested_programme_=house_no_index;
 std::string programme_error_;
 bool resolve_npc_dialogue(uint32_t,uint32_t&first,uint32_t&count,uint32_t&program,uint32_t&seen)const;
 bool request_scene_door(uint32_t);
 const FieldDoorData*scene_door_data_=nullptr;FieldDoorRuntime*scene_door_runtime_=nullptr;
 std::function<bool(uint64_t&,std::string&)>scene_door_player_;std::vector<uint32_t>scene_door_ids_;
 std::function<bool(std::string_view,std::string&)>scene_door_preparer_;
 std::vector<HouseSceneDoorRequest>scene_door_requests_;std::string scene_door_error_;
 bool begin_door(uint32_t);bool interact();bool finish_door();
 const DialogueChoicesData*choices_data_=nullptr;DialogueChoices*choices_=nullptr;uint32_t choices_generation_=0;
 PhoneRuntime*phone_=nullptr;std::vector<PhoneSoundRequest>phone_sounds_;
 HouseInspectionView inspections_;
 DrawerProgramView drawer_;DrawerHost*drawer_effects_=nullptr;DrawerProgramRuntime drawer_runtime_;std::string drawer_error_;
 const BasementProgressionData*basement_=nullptr;const BasementActorData*basement_actors_=nullptr;HouseBasementHost basement_host_;
 uint32_t basement_door_=house_no_index;BasementActorPlayback basement_present_{};bool basement_sound_playing_=false,basement_sound_pending_=false;std::string basement_error_;
 HouseView content_;OpeningWorld*world_=nullptr;HousePresentation*presentation_=nullptr;
 struct AreaContact {uint8_t kind;uint32_t index;bool entered;};
 std::vector<AreaContact>pending_contacts_;
 std::vector<uint8_t>door_inside_,boundary_inside_,story_inside_,story_process_;
 std::vector<HouseOpenableState>openables_;
 struct RestoreWait {uint64_t frame_revision=0,ready_idle=0;uint8_t phase=0;};
 std::vector<RestoreWait>npc_restore_;
 std::vector<uint64_t>story_ready_idle_;
 Vec2 observed_physics_position_{};
 uint32_t story_original_npc_=house_no_index;
 uint32_t active_story_dialogue_=house_no_index;bool story_executing_=false;
 bool observed_battle_=false;
 uint32_t story_index_=house_no_index;std::set<uint32_t>seen_;
 std::vector<HouseEvent>events_;std::vector<HouseSoundRequest>sounds_;
 HousePhase phase_=HousePhase::Idle;uint32_t active_=house_no_index;double fade_time_=0;
 bool door_unpaused_=false;uint64_t physics_tick_=0,idle_frame_=0,await_idle_=0;
 Vec2 last_safe_position_{},last_safe_direction_{};const char*error_="";
};
}
