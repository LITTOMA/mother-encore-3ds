#pragma once
#include "encore/player_initialization.hpp"
namespace encore::upstream {
struct PlayerPreloadScene {
  std::shared_ptr<const GlobalYamlValue> row, native_source, connections;
  FieldNodeRecipeData recipe;
};
class PlayerPreloadScenesData {
public:
  bool load(const uint8_t *, size_t, const PlayerInitializationData &,
            std::string &);
  bool load_file(const char *, const PlayerInitializationData &, std::string &);
  bool valid() const { return valid_; }
  const auto &entries() const { return entries_; }
  const auto &player_ir_sha256() const { return player_ir_; }
  const auto &ir_sha256() const { return ir_; }
  const PlayerPreloadScene *entry(const GlobalYamlValue &) const;

private:
  bool valid_ = false;
  std::array<uint8_t, 32> player_ir_{}, ir_{};
  std::vector<PlayerPreloadScene> entries_;
};
} // namespace encore::upstream
