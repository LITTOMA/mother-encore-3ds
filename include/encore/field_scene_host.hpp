#pragma once
#include "encore/field_scene_data.hpp"
#include "encore/field_geometry_space.hpp"
#include "encore/field_map.hpp"
#include "encore/field_runtime.hpp"
#include "encore/field_npc.hpp"
#include "encore/field_enemy.hpp"
#include "encore/field_tint.hpp"
#include "encore/field_sprite_bridge.hpp"
#include "encore/field_dandelion.hpp"
#include "encore/field_emotes.hpp"
#include "encore/field_door.hpp"
#include "encore/field_prompts.hpp"
#include "encore/field_dead_bush.hpp"
#include "encore/field_interact_dialog.hpp"
#include "encore/field_cutscene_area.hpp"
#include "encore/field_camera_arrows.hpp"
#include "encore/field_game_camera.hpp"
#include "encore/field_door_npc.hpp"
#include "encore/field_melody_background.hpp"
#include "encore/field_map_space.hpp"
#include "encore/field_scene_actions.hpp"
#include "encore/field_stepping_sounds.hpp"
#include "encore/field_player_transitions.hpp"

#include "encore/field_birds.hpp"
#include "encore/field_camera_area.hpp"
#include "encore/field_music_changer.hpp"

#include "encore/field_present.hpp"
#include "encore/field_dropped.hpp"
#include "encore/field_sparkles.hpp"
#include "encore/field_openable_door.hpp"
#include "encore/field_payphone.hpp"
#include "encore/field_butterfly.hpp"

#include <functional>
#include <unordered_map>

namespace encore::upstream {
using FieldSceneSignalSlot=std::function<bool(std::string&)>;
using FieldSceneAreaSlot=std::function<bool(bool,std::string&)>;
struct FieldSceneHostOps {
 // Read/write the authoritative save-backed dictionaries. Write never emits:
 // this consumer applies the source setter's registered-key and emit policy.
 std::function<bool(bool object,std::string_view key,bool&present,bool&value,std::string&)>read_flag;
 std::function<bool(bool object,std::string_view key,bool value,std::string&)>write_flag;
 // The actual shared signal bus retains connection order and synchronous
 // callback dispatch. Host must disconnect these borrowed callbacks on exit.
 std::function<bool(uint32_t,FieldSceneSignalSlot,std::string&)>connect_flags;
 std::function<bool(std::string&)>emit_flags;
 std::function<bool(uint32_t,FieldSceneAreaSlot,std::string&)>connect_area_left;
 std::function<bool(bool,std::string&)>emit_area_left;
 std::function<bool(uint32_t,std::function<void(bool)>,std::string&)>connect_switches;
 std::function<bool(uint32_t,bool,std::string&)>visibility;
 std::function<bool(uint32_t,std::string&)>queue_free;
 std::function<bool(std::string_view,bool&,std::string&)>map_possessed;
 // Query then append/erase the real flyingman party member; no fake presence.
 std::function<bool(bool&,std::string&)>flyingman_present;
 std::function<bool(bool,std::string&)>set_flyingman_present;
 std::function<bool(bool&debug_build,bool&tree_current_is_area,bool&player_paused,std::string&)>debug_context;
 std::function<bool(Vec2,std::string&)>teleport_player;
 std::function<bool(const FieldSceneData*&,std::string&)>current_scene;
};
struct FieldSceneConsumers {
 const FieldData*grass_data=nullptr;FieldRuntime*grass=nullptr;SourceRandom*random=nullptr;
 const FieldNpcData*npc_data=nullptr;FieldNpcRuntime*npc=nullptr;
 const FieldEnemyData*enemy_data=nullptr;FieldEnemyRuntime*enemy=nullptr;
 const FieldTintData*tint_data=nullptr;FieldTintRuntime*tint=nullptr;
 const FieldSpriteData*sprite_data=nullptr;FieldSpriteRuntime*sprite=nullptr;
 const FieldEmoteData*emote_data=nullptr;FieldEmoteRuntime*emote=nullptr;
 const FieldDandelionData*dandelion_data=nullptr;FieldDandelionRuntime*dandelion=nullptr;
 const FieldDoorData*door_data=nullptr;FieldDoorRuntime*door=nullptr;
 const FieldPromptData*prompt_data=nullptr;FieldPromptRuntime*prompt=nullptr;
 const FieldBushData*bush_data=nullptr;FieldBushRuntime*bush=nullptr;
 const FieldInteractData*interact_data=nullptr;FieldInteractRuntime*interact=nullptr;
 const FieldPresentData*present_data=nullptr;FieldPresentRuntime*present=nullptr;
 const FieldDroppedData*dropped_data=nullptr;FieldDroppedRuntime*dropped=nullptr;
 const FieldSparklesData*sparkles_data=nullptr;FieldSparklesRuntime*sparkles=nullptr;
 const FieldOpenableDoorData*openable_data=nullptr;FieldOpenableDoorRuntime*openable=nullptr;
 const FieldPayphoneData*payphone_data=nullptr;FieldPayphoneRuntime*payphone=nullptr;
 const FieldButterflyData*butterfly_data=nullptr;FieldButterflyRuntime*butterfly=nullptr;
 const FieldCutsceneAreaData*cutscene_data=nullptr;FieldCutsceneAreaRuntime*cutscene=nullptr;
 const FieldBirdData*birds_data=nullptr;FieldBirdRuntime*birds=nullptr;
 const FieldCameraAreaData*camera_area_data=nullptr;FieldCameraAreaRuntime*camera_area=nullptr;
 const FieldMusicChangerData*music_data=nullptr;FieldMusicChangerRuntime*music=nullptr;
 const FieldCameraArrowsData*arrows_data=nullptr;FieldCameraArrowsRuntime*arrows=nullptr;
 const FieldSceneActionsData*actions_data=nullptr;FieldSceneActionsRuntime*actions=nullptr;
 const FieldSteppingSoundsData*stepping_data=nullptr;FieldSteppingSoundsRuntime*stepping=nullptr;
 const FieldPlayerTransitionsData*transitions_data=nullptr;FieldPlayerTransitionsRuntime*transitions=nullptr;
 const FieldGameCameraData*camera_data=nullptr;FieldGameCameraRuntime*camera=nullptr;
 const FieldDoorNpcData*door_npc_data=nullptr;FieldDoorNpcRuntime*door_npc=nullptr;
 const FieldMelodyBackgroundData*melody_data=nullptr;FieldMelodyBackgroundRuntime*melody=nullptr;
 FieldMapSpace*map_space=nullptr;
 const FieldMapView*map=nullptr;const FieldGeometryView*geometry_data=nullptr;FieldGeometrySpace*geometry=nullptr;
};
struct FieldSceneScriptAdmission {uint32_t id=0,family=0,capability=0;std::array<uint8_t,32>source_sha{};};
// Lifecycle owner for the complete source script roster. There is no generic
// "approve script" callback. A typed role must execute its real Ready method;
// unknown leaf scripts retain the cursor and block the scene. Partial callback
// failures poison the startup, preventing retries from re-consuming RNG.
class FieldSceneHost {
public:
 bool configure(const FieldSceneData&,FieldSceneConsumers,FieldSceneHostOps,std::string&);
 bool ready_next(std::string&);bool ready_to_boundary(std::string&);
 uint32_t ready_cursor()const{return cursor_;}bool scene_ready()const;
 const FieldSceneReady*blocked()const{return blocked_valid_?&blocked_:nullptr;}
 bool script_admission(uint32_t,FieldSceneScriptAdmission&)const;
 FieldMapGateState gate(uint32_t)const;
 // FlagLandmark _check_flags registered on the real flags_updated signal.
 bool recheck_landmark(uint32_t,std::string&);
 // FlaggableObject base class methods remain callable, but do not approve
 // ItemHolder/Present/DroppedItem's separate inventory/animation mechanisms.
 bool flag_status(uint32_t,bool&,std::string&);bool set_flag_status(uint32_t,bool,std::string&);
 bool leave_area(uint32_t,bool region_changed,std::string&);
 bool set_flag(bool object,std::string_view,bool value,bool emit,std::string&);
 bool map_name(bool only_if_possessed,bool with_override,std::string&,std::string&)const;
 bool leave_for(const FieldSceneData&,std::string&);
 // Checked adapters for the existing FieldEmoteHost. Resolving onready object
 // uses immutable descriptors: child Ready precedes CharacterSprite Ready.
 bool emote_object(const FieldEmoteDescriptor&,bool&,uint32_t&,std::string&)const;
 bool emote_direction_source(const FieldEmoteDescriptor&,uint32_t&,std::string&)const;
 bool emote_texture_geometry(uint32_t,uint32_t&,uint32_t&,std::string&)const;
 bool emote_direction(uint32_t,Vec2&,std::string&)const;
 bool emote_sprite_changed(uint32_t,std::string&);
 bool idle_emotes(float,std::string&);
 bool idle_dead_bushes(float,std::string&);
 bool switches_state()const{return switches_;}
 // Called only after actual source SceneTree deferred deletion has committed.
 bool commit_deleted(uint32_t,std::string&);
private:
 struct Gate {bool ready=false,visible=true,queued=false,deleted=false;};
 const FieldSceneData*data_=nullptr;FieldSceneConsumers consumers_;FieldSceneHostOps host_;
 uint32_t cursor_=0;bool switches_=false,poisoned_=false,blocked_valid_=false,base_done_=false;
 FieldSceneReady blocked_{};std::unordered_map<uint32_t,Gate>gates_;
 std::unordered_map<uint32_t,FieldSceneScriptAdmission>admissions_;
 bool read_flag(bool,std::string_view,bool&,std::string&)const;
 bool dispatch(const FieldSceneReady&,bool&pending,std::string&);
 bool bind_geometry(const FieldSceneReady&,uint32_t family,uint32_t capability,std::string&);
 bool flag_key(uint32_t,bool&,std::string&,uint32_t&,std::string&)const;
 bool area_ready(std::string&);
};
}
