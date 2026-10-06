#pragma once
#include "encore/field_global_data.hpp"
#include "encore/global_yaml_caches.hpp"
namespace encore::upstream {
struct GlobalDataConstructorDeclaration {
  std::string name, type_hint, setter, getter;
  bool constant = false;
  // 0..6 are native YAML Variant values; 7 Vector2, 8 Object reference,
  // 9 ordered character reference Dictionary. Adapter denotes real owner.
  uint32_t kind = 0, adapter = 0, reference_id = 0, owner_role = 0;
  std::shared_ptr<const GlobalYamlValue> value;
  std::array<double, 2> vector{};
  std::vector<std::pair<std::string, uint32_t>> references;
};
struct GlobalDataConstructorObject {
  uint32_t id = 0, kind = 0, role = 0;
  std::string name, native, script;
  std::vector<FieldGlobalDataDefault> defaults;
  // Public setget properties also have actual native initialized backing.
  std::vector<std::pair<std::string, std::string>> default_getters;
};
struct GlobalDataConstructorStep {
  uint32_t kind = 0, role = 0;
  std::string method, member, argument;
};
struct GlobalDataTextSpeedPolicy {
  std::string member, constant_name, setter, getter;
  std::vector<double> speeds;
  double threshold = 0, closest_initial = 0, delta_initial = 0;
  uint32_t fallback_divisor = 0, comparison = 0;
};
class GlobalDataConstructorData {
public:
  bool load(const uint8_t *, size_t, const FieldGlobalDataData &,
            const GlobalYamlCachesData &, std::string &);
  bool load_file(const char *, const FieldGlobalDataData &,
                 const GlobalYamlCachesData &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &legacy_ir_sha256() const { return legacy_; }
  const auto &cache_ir_sha256() const { return cache_; }
  const auto &engine_review_sha256() const { return engine_; }
  const auto &owner_source() const { return owner_; }
  const auto &objects() const { return objects_; }
  const auto &declarations() const { return declarations_; }
  const auto &constructor_steps() const { return constructor_; }
  const auto &ready_steps() const { return ready_; }
  const auto &text_speed() const { return speed_; }
  const auto &menu_flavor_member() const { return menu_flavor_member_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, legacy_{}, cache_{}, engine_{};
  std::string owner_, menu_flavor_member_;
  std::vector<GlobalDataConstructorObject> objects_;
  std::vector<GlobalDataConstructorDeclaration> declarations_;
  std::vector<GlobalDataConstructorStep> constructor_, ready_;
  GlobalDataTextSpeedPolicy speed_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
} // namespace encore::upstream
