#pragma once
#include "encore/player_visual_scripts.hpp"
namespace encore::upstream {
struct PlayerGraphicsAsset {
  uint32_t id = 0, width = 0, height = 0, bytes = 0;
  bool nearest = false, repeat = false;
  std::string source, path;
  std::array<uint8_t, 32> source_sha{}, import_sha{}, output_sha{};
};
class PlayerGraphicsData {
public:
  bool load(const uint8_t *, size_t, const PlayerVisualScriptsData &,
            std::string &);
  bool load_file(const char *, const PlayerVisualScriptsData &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &visual_ir_sha256() const { return visual_ir_; }
  const auto &assets() const { return assets_; }
  const PlayerGraphicsAsset *asset(std::string_view,
                                   const std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, visual_ir_{};
  std::vector<PlayerGraphicsAsset> assets_;
};
} // namespace encore::upstream
