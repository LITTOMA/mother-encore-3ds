#pragma once
#include "encore/house_data.hpp"
#include "encore/world.hpp"
#include "encore/phone_runtime.hpp"
#include "encore/dialogue_choices.hpp"
#include "encore/house_inspection_data.hpp"
#include "encore/drawer_program.hpp"
#include "encore/house_presents.hpp"
#include "encore/world_links.hpp"
#include <set>
namespace encore::upstream {
class HousePresentation;
enum class HousePhase:uint8_t {Idle,DoorAwaitIdle,DoorFadeIn,WarpAwaitIdle,DoorFadeOut,Dialogue,StoryBoundary,StoryRunning,Unsupported,Error,InspectionProgram,SceneDoorAwaitIdle,SceneTransition,PresentProgram};
enum class HouseEventKind:uint8_t {Paused,DoorStarted,DoorEntered,PlayerMoved,FadeOutStarted,DoorDone,DialogueOpened,DialogueSeen,DialogueClosed,OpenableOpened,OpenableUnlocked,OpenableFlagWritten,OpenableNormal,DoorDialogueOpened,StoryRequested};
struct HouseEvent {HouseEventKind kind{};uint32_t object=0;uint64_t physics_tick=0,idle_frame=0;Vec2 position{};};
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
 bool restore_seen_dialogue(const std::set<uint32_t>&);
 bool set_player_nickname(std::string_view);
 bool bind_phone(PhoneRuntime&);
 // Cross-pack admission is transactional: a rejected candidate keeps the
 // previous inspection binding and current interaction intact.
 bool bind_inspections(HouseInspectionView);
 // Independent effect programme owns its source identity, inspector and text
 // refs. Bind before inspections; effects owns inventory/audio, never text/flags.
 bool bind_drawer(DrawerProgramView,DrawerHost& effects);
 void bind_choices(const DialogueChoicesData&data,DialogueChoices&model){choices_data_=&data;choices_=&model;}
 // Present text and flags are owned here; effects own key-item grants and audio.
 // Transactional: a rejected pack keeps the previous binding. Bind before scene ready.
 bool bind_presents(PresentRuntime&,PresentView,PresentEffects&);
 bool present_interaction_supported(uint32_t i)const{return presents_&&presents_->supported(i);}
 // Boundaries of kind UnsupportedScene whose source door has a reviewed route
 // become cross-scene doors; every other boundary keeps the development stop.
 bool bind_scene_routes(const WorldLinksData&links,uint32_t scene_id);
 // Route index once the door's idle frame has passed; the platform owns the swap.
 uint32_t take_scene_route(){const auto r=scene_route_;scene_route_=WorldLinksData::kNotFound;return r;}
 // A rejected destination keeps this House paused at an explicit stop; B returns
 // to the last safe point. The message must have static storage duration.
 bool abort_scene_transition(const char*message){
  if(phase_!=HousePhase::SceneTransition&&phase_!=HousePhase::SceneDoorAwaitIdle)return false;
  scene_route_=WorldLinksData::kNotFound;phase_=HousePhase::Unsupported;error_=message;return true;
 }
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
 bool interact_inspection(uint32_t index);
 bool resolve_inspection_dialogue(uint32_t,uint32_t&)const;
 std::string_view inspection_path(uint32_t)const;
 bool drawer_selected(uint32_t)const;
 bool validate_text(uint32_t,std::string&)override;
 bool validate_flag(std::string_view,std::string&)override;
 bool validate_item(DrawerItemTemplate t,std::string_view n,std::string&e)override{return drawer_effects_&&drawer_effects_->validate_item(t,n,e);}
 bool validate_sound(std::string_view n,std::string&e)override{return drawer_effects_&&drawer_effects_->validate_sound(n,e);}
 bool show_text(uint32_t,std::string&)override;
 bool flag(std::string_view,bool&,std::string&)override;
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
 bool resolve_npc_dialogue(uint32_t,uint32_t&first,uint32_t&count,uint32_t&program,uint32_t&seen)const;
 bool begin_door(uint32_t);bool interact();bool finish_door();
 bool interact_present(uint32_t);
 class PresentAdapter final:public PresentHost {
 public:
  HouseRuntime*owner=nullptr;PresentEffects*effects=nullptr;
  bool validate_text(uint32_t,std::string_view,std::string&)override;
  bool validate_flag(std::string_view n,std::string&e)override{return owner->validate_flag(n,e);}
  bool validate_item(PresentTemplate t,std::string_view n,std::string&e)override{return effects->validate_item(t,n,e);}
  bool validate_sound(std::string_view n,std::string&e)override{return effects->validate_sound(n,e);}
  bool show_text(uint32_t,std::string&)override;
  bool flag(std::string_view n,bool&v,std::string&e)override{return owner->flag(n,v,e);}
  bool set_flag(std::string_view n,bool v,std::string&e)override{return owner->set_flag(n,v,e);}
  bool grant_item(PresentTemplate t,std::string_view n,std::string&e)override{return effects->grant_item(t,n,e);}
  bool play_sound(std::string_view n,std::string&e)override{return effects->play_sound(n,e);}
 };
 PresentAdapter present_host_;PresentRuntime*presents_=nullptr;std::string present_error_;
 const DialogueChoicesData*choices_data_=nullptr;DialogueChoices*choices_=nullptr;uint32_t choices_generation_=0;
 PhoneRuntime*phone_=nullptr;std::vector<PhoneSoundRequest>phone_sounds_;
 HouseInspectionView inspections_;
 DrawerProgramView drawer_;DrawerHost*drawer_effects_=nullptr;DrawerProgramRuntime drawer_runtime_;std::string drawer_error_;
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
 std::vector<uint32_t>boundary_routes_;uint32_t scene_route_=WorldLinksData::kNotFound;
 Vec2 last_safe_position_{},last_safe_direction_{};const char*error_="";
};
}
