#pragma once
#include "encore/field_ui_manager.hpp"
namespace encore::upstream {
struct HouseUiSourceField {
  uint32_t role = 0;
  bool initial = false;
  std::string member;
};
struct HouseUiSourceMethod {
  uint32_t role = 0;
  std::string name;
  std::array<uint8_t, 32> sha{};
};
struct HouseUiSourceSignal {
  uint32_t role = 0, arity = 0;
  std::string name;
};
struct HouseUiDialoguePolicy {
  std::string stack_member, dialogue_member, canvas_member;
  std::string dialogue_scene, dialogue_script, abstract_script;
  std::string add_child_method, close_method, queue_free_method;
  std::string actors_member,queued_battle_member,set_respawn_member;
  std::string input_sound_node,open_sound_name,open_sound_source;
  uint32_t actor_count=0;
  bool queued_battle=false,set_respawn=false;
};
struct HouseUiClosedWidget {
  uint32_t role=0;
  std::string scene,script,member,close_method;
  bool initial_open=false;
};
struct HouseUiBusinessPolicy {
  std::vector<HouseUiClosedWidget> widgets;
  std::string timer_member,fade_scene,fade_script,cut_signal,global_script,phone_member,end_signal;
  std::string close_sound_source,close_sound_name;
  double restore_target=0,restore_duration=0,spin_stop_unit_offset=0;
  double effect_target=0,effect_duration=0,spin_speed=0,spin_length=0,spin_from=0,spin_to=0;
  float curve_length=0,curve_interval=0;
  Vec2 screen_size{},initial_path_position{};
  std::array<float,4> effect_color{};
  std::vector<Vec2> curve_points;
  uint32_t effect_type=0,effect_ease=0;
  bool curve_cubic=false,curve_loop=false;
  uint32_t fade_type=0,transition=0,ease=0;
  bool initial_timer_null=false;
};
class HouseUiContinuationData {
public:
  bool load(const uint8_t *, size_t, const FieldUiManagerData &, std::string &);
  bool load_file(const char *, const FieldUiManagerData &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &source_script() const { return script_; }
  const auto &fields() const { return fields_; }
  const auto &signals() const { return signals_; }
  const HouseUiSourceMethod *method(uint32_t) const;
  const HouseUiSourceSignal *signal(uint32_t) const;
  uint32_t capability()const{return capability_;}
  bool stack_continuation()const{return capability_>=3;}
  bool dialogue_continuation()const{return capability_>=4;}
  const HouseUiDialoguePolicy &dialogue_policy()const{return dialogue_policy_;}
  const HouseUiBusinessPolicy &business_policy()const{return business_;}
  bool key_indicator() const { return key_policy_; }
  const auto &key_member() const { return key_member_; }
  int64_t key_default_count() const { return key_default_; }
  const auto &enemy_member()const{return enemy_member_;}
  const auto &house_scene()const{return house_scene_;}
  bool key_initial_open() const { return key_open_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  HouseUiDialoguePolicy dialogue_policy_;
  HouseUiBusinessPolicy business_;
  uint32_t capability_=0;
  bool valid_ = false, key_policy_=false,key_open_=false;
  std::string key_member_,key_scene_,key_script_,enemy_member_,house_scene_;
  int64_t key_default_=0;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, ui_ir_{};
  std::string script_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  std::vector<HouseUiSourceField> fields_;
  std::vector<HouseUiSourceMethod> methods_;
  std::vector<HouseUiSourceSignal> signals_;
};
} // namespace encore::upstream
