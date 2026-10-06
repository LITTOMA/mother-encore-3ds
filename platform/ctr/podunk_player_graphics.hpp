#pragma once
#include "encore/player_graphics.hpp"
#include "loading_texture.hpp"
#include "podunk_player_visual_native.hpp"
namespace encore::ctr {
class PodunkPlayerGraphics final : public PodunkPlayerVisualTextures {
public:
  PodunkPlayerGraphics() = default;
  PodunkPlayerGraphics(const PodunkPlayerGraphics &) = delete;
  PodunkPlayerGraphics &operator=(const PodunkPlayerGraphics &) = delete;
  ~PodunkPlayerGraphics() { free(); }
  bool load(const upstream::PlayerGraphicsData &, const char *root,
            std::string &);
  bool image(std::string_view, const std::array<uint8_t, 32> &, C2D_Image &,
             std::string &) const override;
  bool loaded() const { return data_ != nullptr; }
  void free();

private:
  struct Asset {
    upstream::PlayerGraphicsAsset binding;
    LoadingSpriteSheet sheet = nullptr;
  };
  const upstream::PlayerGraphicsData *data_ = nullptr;
  std::vector<Asset> assets_;
};
} // namespace encore::ctr
