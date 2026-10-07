#pragma once
#include "encore/battle_data.hpp"
#include <map>
#include <array>
#include <functional>
namespace encore::upstream {
struct FieldCutsceneAreaConnection { uint32_t role=0;std::string signal,method; };
struct FieldCutsceneAreaPolicy {
 std::array<bool,4>close{};std::array<bool,3>pause{};
 std::string completion,battle_signal,battle_method;std::vector<FieldCutsceneAreaConnection>connections;
};
struct FieldCutsceneAreaBinding {
 uint32_t id=0,ready_ordinal=0,shape_id=0,collision_layer=0,collision_mask=0,flags=0;
 std::string node,dialog,programme,appear,disappear;std::array<uint8_t,32>programme_sha{};
 Vec2 centre{},half_extents{};
 bool monitoring()const{return flags&1;}bool monitorable()const{return flags&2;}
};
class FieldCutsceneAreaData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}const std::array<uint8_t,20>&source_pin()const{return pin_;}
 const std::string&scene()const{return scene_;}const std::string&script()const{return script_;}
 const FieldCutsceneAreaPolicy&policy()const{return policy_;}
 const std::vector<FieldCutsceneAreaBinding>&bindings()const{return bindings_;}
 const FieldCutsceneAreaBinding*binding(uint32_t)const;
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 std::map<std::string,std::array<uint8_t,32>>sources_;
 bool valid_=false;std::array<uint8_t,20>pin_{};std::string scene_,script_;
 FieldCutsceneAreaPolicy policy_;std::vector<FieldCutsceneAreaBinding>bindings_;
};
struct FieldCutsceneAreaUi { bool cutscene=false,battle=false,pause=false; };
struct FieldCutsceneAreaHost {
 // Admission proves actual Area/body shape/filter and signal endpoints. Godot
 // source mask filtering is bilateral OR; body polygon, not a point/ray.
 std::function<bool(const FieldCutsceneAreaBinding&,const FieldCutsceneAreaPolicy&,std::string&)>admit_ready;
 std::function<bool(uint32_t,std::string_view,std::string&)>connect_battle_to_overworld;
 std::function<bool(uint32_t,bool&,std::string&)>body_is_current_player;
 std::function<bool(FieldCutsceneAreaUi&,std::string&)>query_ui;
 // Absent flags return false. Preserve appear/disappear short-circuit ordering.
 std::function<bool(std::string_view,bool&,std::string&)>read_flag;
 // Check existing Room Programme identity/hash AND required typed effects before
 // closing UI or pausing. This adapter never executes YAML or fabricates opcodes.
 std::function<bool(const FieldCutsceneAreaBinding&,std::string&)>admit_programme;
 std::function<bool(const std::array<bool,4>&,std::string&)>close_commands;
 std::function<bool(const std::array<bool,3>&,std::string&)>pause_player;
 // Match open_dialogue_box_and_unpause: actual source ID, no NPC, actual current
 // player completion funcref. The Room/Ui owner retains the yielded continuation
 // independently of Area lifetime and calls player completion after dialogue done.
 std::function<bool(const FieldCutsceneAreaBinding&,std::string_view completion,std::string&)>open_room_and_unpause;
};
struct FieldCutsceneAreaState { uint32_t id=0;bool ready=false,alive=true,processing=false; };
class FieldCutsceneAreaRuntime {
public:
 bool initialize(const FieldCutsceneAreaData&,FieldCutsceneAreaHost,std::string&);
 bool ready(uint32_t,std::string&);bool body_enter(uint32_t,uint32_t body,std::string&);
 bool body_exit(uint32_t,uint32_t body,std::string&);bool check_start(uint32_t,std::string&);
 // Dispatch ordinary source _process in actual scene process order, respecting
 // Tree pause/parent processing. No synthetic retry/rearm after flag or battle.
 bool idle_process(uint32_t,bool tree_can_process,std::string&);
 bool battle_to_overworld(uint32_t,std::string&);bool exit_tree(uint32_t,std::string&);
 const FieldCutsceneAreaState*state(uint32_t)const;
 const FieldCutsceneAreaData*content()const{return data_;}
private:
 FieldCutsceneAreaState*active(uint32_t,std::string&);
 bool check_flags(const FieldCutsceneAreaBinding&,bool&,std::string&);
 const FieldCutsceneAreaData*data_=nullptr;FieldCutsceneAreaHost host_;
 std::vector<FieldCutsceneAreaState>states_;size_t ready_index_=0;
};
}
