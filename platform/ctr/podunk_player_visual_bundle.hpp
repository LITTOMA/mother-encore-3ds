#pragma once
#include "podunk_player_graphics.hpp"
#include "podunk_player_visual_scripts.hpp"
namespace encore::ctr {
// One owning GPU closure and two native sprites, attached while the original
// Player tree is being constructed. Construction never grants subtree Ready.
class PodunkPlayerVisualBundle {
public:
  bool rebind_tree(upstream::FieldNodeTreeRuntime &, std::string &);
  bool load(std::shared_ptr<const upstream::PlayerInitializationData>,
            std::shared_ptr<const upstream::PlayerVisualScriptsData>,
            std::shared_ptr<const upstream::PlayerGraphicsData>,
            const char *romfs_root, std::string &);
  bool prepare(upstream::FieldNodeTreeRuntime &, upstream::FieldGlobalRegistry &,
               upstream::FieldGlobalConstructorRuntime &,
               upstream::PlayerInitializationBody &,
               upstream::PlayerVisualFetcherOwner &, std::string &);
  bool owns_source(uint32_t) const;
  bool construct(upstream::FieldObjectId,
                 const upstream::FieldNodeDescriptor &, std::string &);
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, float delta,
             bool tree_paused, bool update_pending, std::string &);
  PodunkPlayerVisualNative *native(upstream::FieldObjectId);
  PodunkPlayerVisualScripts &scripts() { return scripts_; }
private:
  std::shared_ptr<const upstream::PlayerInitializationData> player_;
  std::shared_ptr<const upstream::PlayerVisualScriptsData> visual_;
  std::shared_ptr<const upstream::PlayerGraphicsData> graphics_data_;
  // Graphics dies after native borrowers, then checked data dies last.
  PodunkPlayerGraphics graphics_;
  PodunkPlayerVisualNative shadow_, bat_;
  PodunkPlayerVisualScripts scripts_;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  bool prepared_ = false;
};
} // namespace encore::ctr
