#pragma once
#include "encore/player_child_scripts.hpp"
#include "podunk_player_animation.hpp"
#include "podunk_player_native_media.hpp"
namespace encore::ctr {
// Only maps the checked Player recipe keys to actual same-instance ObjectIDs.
// The media/AnimationPlayer services retain their unique native clocks/state.
class PodunkPlayerArrowNative final
    : public upstream::FieldCameraArrowsNativeOwner {
public:
  bool prepare(const upstream::PlayerChildScriptsData &,
               const upstream::PlayerInitializationData &,
               upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, upstream::FieldObjectId,
               PodunkPlayerAnimation &, PodunkPlayerNativeMedia &,
               std::string &);
  bool bind(const upstream::FieldCameraArrowsData &, std::string &) override;
  bool rebind_tree(upstream::FieldNodeTreeRuntime &, std::string &);
  bool root(uint32_t, bool &, upstream::Vec2 &, std::string &) const override;
  bool sprite(uint32_t, upstream::FieldArrowSpriteState &,
              std::string &) const override;
  bool play(uint32_t, std::string_view, float, bool, std::string &) override;
  bool assigned(uint32_t, std::string &, std::string &) const override;
  bool frame(uint32_t, int32_t, std::string &) override;
  bool visible(uint32_t, bool, std::string &) override;
  bool actual(uint32_t, upstream::FieldObjectId &, std::string &) const;

private:
  const upstream::PlayerChildScriptsData *data_ = nullptr;
  const upstream::PlayerInitializationData *player_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkPlayerAnimation *animation_ = nullptr;
  PodunkPlayerNativeMedia *media_ = nullptr;
  std::map<uint32_t, upstream::FieldObjectId> objects_;
};
} // namespace encore::ctr
