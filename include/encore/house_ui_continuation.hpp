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
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, ui_ir_{};
  std::string script_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  std::vector<HouseUiSourceField> fields_;
  std::vector<HouseUiSourceMethod> methods_;
  std::vector<HouseUiSourceSignal> signals_;
};
} // namespace encore::upstream
