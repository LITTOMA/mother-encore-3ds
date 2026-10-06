#pragma once
#include "encore/player_visual_scripts.hpp"
#include <citro2d.h>
namespace encore::ctr {
// The source texture loader owns the real GPU allocation. Borrowing never
// manufactures a texture or approves an unsupported source Resource.
class PodunkPlayerVisualTextures {
public:
  virtual ~PodunkPlayerVisualTextures() = default;
  virtual bool image(std::string_view source,
                     const std::array<uint8_t,32> &source_sha,
                     C2D_Image &, std::string &) const = 0;
};
// Native Sprite/AnimatedSprite state attached to the original Tree/ObjectDB.
// All fields come from the already checked Player native snapshot, and all
// frames come from the independently checked SpriteFrames source projection.
class PodunkPlayerVisualNative final : public upstream::PlayerVisualNativeOwner {
public:
  // Bind the actual domain before sibling allocation. This creates no Node,
  // state or Ready receipt; construct still runs at each real source cursor.
  bool prepare(const upstream::PlayerInitializationData &,
               const upstream::PlayerVisualScriptsData &,
               upstream::FieldNodeTreeRuntime &, upstream::FieldGlobalRegistry &,
               PodunkPlayerVisualTextures &, std::string &);
  bool construct(const upstream::PlayerInitializationData &,
                 const upstream::PlayerVisualScriptsData &,
                 upstream::FieldNodeTreeRuntime &, upstream::FieldGlobalRegistry &,
                 upstream::FieldObjectId, PodunkPlayerVisualTextures &, std::string &);
  const upstream::FieldGlobalRegistry *registry() const override { return registry_; }
  const upstream::FieldNodeTreeRuntime *tree() const override { return tree_; }
  upstream::FieldObjectId object() const override { return object_; }
  bool state(upstream::PlayerVisualNativeState &, std::string &) const override;
  bool sprite_frames(const std::vector<upstream::PlayerVisualAnimation> *&,
                     std::string &) const override;
  bool play(std::string_view, std::string &) override;
  bool set_behind_parent(bool, std::string &) override;
  bool set_visible(bool, std::string &) override;
  bool set_frame(uint32_t, std::string &) override;
  bool process_internal(float scaled_delta, bool tree_paused,
                        bool update_pending, std::string &);
  // Called by the compositor at the source canvas/YSort order position.
  // Accepts the actual shared viewport transform; no separate camera clock.
  bool draw(const upstream::FieldTransform &viewport, bool pixel_snap, std::string &) const;
  bool connect(std::string_view signal, upstream::FieldObjectId target,
               std::string method, uint32_t flags, std::string &);
private:
  struct Connection { std::string signal, method; upstream::FieldObjectId target=0; uint32_t flags=0; };
  bool live(std::string &) const;
  bool emit(std::string_view, std::string &);
  const upstream::PlayerVisualAnimation *clip() const;
  float duration() const;
  const upstream::PlayerInitializationData *player_ = nullptr;
  const upstream::PlayerVisualScriptsData *data_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkPlayerVisualTextures *textures_ = nullptr;
  upstream::FieldObjectId object_ = 0;
  upstream::PlayerVisualNativeState state_;
  upstream::Vec2 offset_{};
  bool centered_=false, flip_h_=false, flip_v_=false, shadow_=false, over_=false;
  float speed_scale_=0, timeout_=0;
  std::vector<Connection> connections_;
};
} // namespace encore::ctr
