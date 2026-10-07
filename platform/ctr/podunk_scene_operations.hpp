#pragma once
#include "podunk_house_continuation.hpp"
#include "podunk_player_host.hpp"
#include "encore/field_scene_sources.hpp"
namespace encore::ctr {
// These names/IDs are borrowed from the already checked source adapters at
// composition, never filenames or game tuning encoded in this native owner.
struct PodunkSceneOperationBindings {
  uint32_t flyingman_declaration = 0;
};
struct PodunkSceneOperationInput {
  const upstream::FieldSceneSources *sources = nullptr;
  PodunkHouseContinuation *continuation = nullptr;
  upstream::FieldNodeTreeRuntime *tree = nullptr;
  PodunkPlayerHost *player = nullptr;
  const upstream::PlayerMotionData *motion = nullptr;
  const upstream::FieldSceneHost *lifecycle = nullptr;
  PodunkSceneOperationBindings bindings;
  bool actual_debug_build = false;
  // Optional existing concrete Inventory source query. Missing is an explicit
  // pending map query, and does not prevent AreaRoom's unrelated Ready body.
  std::function<bool(std::string_view, bool &, std::string &)> map_possessed;
};
class PodunkSceneOperations {
public:
  bool prepare(PodunkSceneOperationInput, std::string &);
  // Returns concrete fields/save/party/tree operations. Root then applies
  // the existing 0068 PodunkSceneSignalCallbacks to the same Owners.scene;
  // this owner never creates a second signal callback table.
  upstream::FieldSceneHostOps ops();
  bool observe_allocated(upstream::FieldObjectId,
                         const upstream::FieldNodeDescriptor &,
                         const upstream::FieldIdentity &, std::string &);
  // Source global.currentScene assignment precedes root.add_child/Ready.
  bool publish_current_scene_before_enter(std::string &);
  // Caller has materialized/migrated the actual continuation Player using the
  // same factory. This adds that exact object, not a cold _init_player replay.
  bool attach_existing_player(std::string &);
private:
  bool live(std::string &) const;
  bool source(uint32_t, upstream::FieldObjectId &, std::string &) const;
  bool area(upstream::FieldObjectId &, std::string &) const;
  bool flyingman(upstream::FieldObjectId &, std::string &) const;
  bool flyingman_present(bool &, std::string &) const;
  bool set_flyingman(bool, std::string &);
  bool debug_context(bool &, bool &, bool &, std::string &) const;
  bool teleport(upstream::Vec2, std::string &);
  PodunkSceneOperationInput input_;
  std::map<uint32_t, upstream::FieldObjectId> allocated_;
  bool prepared_ = false, published_ = false, attached_ = false,
       failed_ = false;
};
} // namespace encore::ctr
