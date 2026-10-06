#pragma once
#include "encore/field_scene_sources.hpp"
#include "podunk_house_continuation.hpp"
#include "podunk_player_host.hpp"
#include "podunk_scene_animated_leaves.hpp"
#include "podunk_scene_native.hpp"
#include "podunk_scene_scripts.hpp"
namespace encore::ctr {
// Existing actual mechanism owners supply only operations they truly own.
// Common ObjectDB/Tree/save operations are bound below to the real owners.
// An absent operation rejects at its exact consumer boundary, never succeeds.
struct PodunkSceneMechanismOwners {
  upstream::FieldNpcHost npc;
  upstream::FieldEnemyHost enemy;
  upstream::FieldTintHost tint;
  upstream::FieldSpriteHost sprite;
  upstream::FieldEmoteHost emote;
  upstream::FieldDandelionHost dandelion;
  upstream::FieldDoorHost door;
  upstream::FieldPromptHost prompt;
  upstream::FieldBushHost bush;
  upstream::FieldInteractHost interact;
  upstream::FieldPresentHost present;
  upstream::FieldDroppedHost dropped;
  upstream::FieldSparklesHost sparkles;
  upstream::FieldOpenableHost openable;
  upstream::FieldPayphoneHost payphone;
  upstream::FieldButterflyHost butterfly;
  upstream::FieldCutsceneAreaHost cutscene;
  upstream::FieldBirdHost birds;
  upstream::FieldCameraAreaHost camera_area;
  upstream::FieldMusicChangerHost music;
  upstream::FieldCameraArrowsHost arrows;
  upstream::FieldSceneActionsHost actions;
  upstream::FieldSteppingSoundsHost stepping;
  upstream::FieldPlayerTransitionsHost transitions;
  upstream::FieldGameCameraHost camera;
  upstream::FieldDoorNpcHost door_npc;
  upstream::FieldMelodyBackgroundHost melody;
  upstream::FieldVendingHost vending;
  upstream::FieldSceneHostOps scene;
};
struct PodunkSceneConsumerInput {
  const upstream::FieldSceneSources *sources = nullptr;
  PodunkHouseContinuation *continuation = nullptr;
  upstream::FieldNodeTreeRuntime *tree = nullptr;
  PodunkSceneNative *native = nullptr;
  PodunkPlayerHost *player = nullptr;
  PodunkSceneAnimatedLeaves *animated = nullptr;
  upstream::FieldMapSpace *map = nullptr;
  upstream::FieldGeometrySpace *geometry = nullptr;
  const upstream::FieldShopData *shop = nullptr;
  uint64_t scene_epoch = 0;
};
class PodunkSceneConsumers {
public:
  PodunkSceneConsumers();
  ~PodunkSceneConsumers();
  PodunkSceneConsumers(const PodunkSceneConsumers &) = delete;
  PodunkSceneConsumers &operator=(const PodunkSceneConsumers &) = delete;
  // Prepare exposes unique typed core addresses only for the concrete native
  // owners' preloads. It executes no constructor/Ready/RNG operation.
  bool prepare(PodunkSceneConsumerInput, std::string &);
  upstream::FieldSceneConsumers runtime_instances() const;
  bool initialize_owners(PodunkSceneMechanismOwners, std::string &);
  bool initialize(PodunkSceneConsumerInput, PodunkSceneMechanismOwners,
                  std::string &);
  bool bind_scripts(PodunkSceneScripts &, std::string &);
  bool initialized() const;
  upstream::FieldSceneConsumers consumers() const;
  upstream::FieldSceneHostOps ops() const;
  bool source_object(uint32_t, upstream::FieldObjectId &, std::string &) const;
  bool sprite_appearance(const upstream::FieldCanvasRecord &,
                         upstream::FieldObjectId, upstream::FieldObjectId,
                         upstream::FieldCanvasAppearance &,
                         std::string &) const;
  // Actual initializer failures, including missing concrete mechanism owners;
  // this list grants no source lifecycle admission.
  const std::vector<std::string> &initialization_failures() const;

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace encore::ctr
