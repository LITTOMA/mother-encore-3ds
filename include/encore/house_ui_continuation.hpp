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
  bool stack_continuation()const{return capability_==3;}
  bool key_indicator() const { return key_policy_; }
  const auto &key_member() const { return key_member_; }
  int64_t key_default_count() const { return key_default_; }
  const auto &enemy_member()const{return enemy_member_;}
  const auto &house_scene()const{return house_scene_;}
  bool key_initial_open() const { return key_open_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
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
