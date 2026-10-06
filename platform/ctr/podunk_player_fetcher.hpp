#pragma once
#include "encore/player_fetcher.hpp"
#include "podunk_player_animation.hpp"
namespace encore::ctr {
// Source resource loader resolves Bat's original texture ID to the actual
// same-Registry Texture owner. It grants no script/native lifecycle.
class PodunkPlayerFetcherTextures {
public:
  virtual ~PodunkPlayerFetcherTextures() = default;
  virtual const upstream::FieldGlobalRegistry *registry() const = 0;
  virtual bool texture(upstream::FieldObjectId sprite, uint32_t source_resource,
                       upstream::FieldObjectId &, std::string &) const = 0;
};
class PodunkPlayerFetcherSprites final
    : public upstream::PlayerFetcherSpriteReader {
public:
  bool initialize(upstream::FieldNodeTreeRuntime &,
                  upstream::FieldGlobalRegistry &, PodunkPlayerAnimation &,
                  PodunkPlayerVisualNative &bat, PodunkPlayerFetcherTextures &,
                  std::string &);
  const upstream::FieldGlobalRegistry *registry() const override {
    return registry_;
  }
  const upstream::FieldNodeTreeRuntime *tree() const override { return tree_; }
  bool read(upstream::FieldObjectId, upstream::PlayerFetcherSpriteState &,
            std::string &) const override;

private:
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkPlayerAnimation *animation_ = nullptr;
  PodunkPlayerVisualNative *bat_ = nullptr;
  PodunkPlayerFetcherTextures *textures_ = nullptr;
};
} // namespace encore::ctr
