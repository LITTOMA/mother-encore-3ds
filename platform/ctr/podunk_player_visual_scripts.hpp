#pragma once
#include "encore/player_visual_scripts.hpp"
namespace encore::ctr {
// Concrete script host for the same native Player tree. No native Sprite is
// manufactured here; normal constructor/Ready/render owners must be supplied.
class PodunkPlayerVisualScripts {
public:
  bool initialize(const upstream::PlayerVisualScriptsData &,
                  const upstream::PlayerInitializationData &,
                  upstream::FieldNodeTreeRuntime &,
                  upstream::FieldGlobalRegistry &,
                  upstream::FieldGlobalConstructorRuntime &,
                  upstream::PlayerInitializationBody &,
                  upstream::PlayerVisualNativeOwner &shadow,
                  upstream::PlayerVisualNativeOwner &bat,
                  upstream::PlayerVisualFetcherOwner &, std::string &);
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 std::string &);
  bool source_shadow_property(upstream::FieldObjectId, std::string_view,
                              std::string &);
  bool script_phase(upstream::FieldObjectId, upstream::FieldTreePhase,
                    std::string &);
  upstream::PlayerVisualScriptsRuntime &core() { return core_; }

private:
  const upstream::PlayerVisualScriptsData *data_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::PlayerVisualScriptsRuntime core_;
};
} // namespace encore::ctr
