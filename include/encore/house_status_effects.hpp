#pragma once
#include "encore/global_yaml_caches.hpp"
#include "encore/house_global_bridge.hpp"
#include "encore/player_ready.hpp"
namespace encore::upstream {
enum class HouseStatusBoolean : uint32_t { Sweat = 1, Incapacitated = 2 };
struct HouseStatusEffectsPolicy {
  std::string id, source;
  std::array<uint8_t, 32> sha{};
  std::shared_ptr<const GlobalYamlValue> expected;
};
class HouseStatusEffectsData {
public:
  bool load(const uint8_t *, size_t, const HouseGlobalBridgeData &,
            const PlayerReadyData &, std::string &);
  bool load_file(const char *, const HouseGlobalBridgeData &,
                 const PlayerReadyData &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &policies() const { return policies_; }
  const auto &effects_key() const { return effects_; }
  const auto &any_case() const { return any_; }
  const std::string &query(HouseStatusBoolean r) const {
    return r == HouseStatusBoolean::Sweat ? sweat_ : incap_;
  }
  const HouseGlobalBridgeData *bridge() const { return bridge_; }
  const PlayerReadyData *ready() const { return ready_; }

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{};
  std::string effects_, any_, sweat_, incap_;
  std::vector<HouseStatusEffectsPolicy> policies_;
  const HouseGlobalBridgeData *bridge_ = nullptr;
  const PlayerReadyData *ready_ = nullptr;
};
struct HouseStatusActualData {
  const FieldGlobalRegistry *registry = nullptr;
  std::shared_ptr<FieldNodeTreeRuntime> tree;
  FieldObjectId object = 0;
  std::string ailment;
  int64_t times = 0;
  std::shared_ptr<GlobalYamlValue> data;
};
class HouseStatusEffectsRuntime {
public:
  using Getter =
      std::function<bool(FieldObjectId character, FieldObjectId status,
                         HouseStatusActualData &, std::string &)>;
  bool initialize(const HouseStatusEffectsData &,
                  const FieldGlobalDataRuntime &, const FieldGlobalRegistry &,
                  Getter, std::string &);
  bool binds(const FieldGlobalDataRuntime &, const FieldGlobalRegistry &) const;
  bool boolean_effect(FieldObjectId, HouseStatusBoolean, bool &,
                      std::string &) const;

private:
  const HouseStatusEffectsData *data_ = nullptr;
  const FieldGlobalDataRuntime *core_ = nullptr;
  const FieldGlobalRegistry *registry_ = nullptr;
  std::array<uint8_t, 32> ir_{};
  Getter getter_;
};
} // namespace encore::upstream
