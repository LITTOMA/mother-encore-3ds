#pragma once
#include "podunk_scene_consumers.hpp"
#include "podunk_scene_loop.hpp"
#include "podunk_scene_operations.hpp"
#include "podunk_player_camera.hpp"
#include "podunk_scene_audio.hpp"
#include "encore/scene_leaf_native.hpp"
#include "podunk_programme_inventory.hpp"
#include "podunk_programme_state.hpp"
#include "encore/blackbars.hpp"
#include "encore/house_return_sources.hpp"
#include "encore/house_return_ladder.hpp"

class SourceFontRenderer;
class HouseRenderer;
class BattleRenderer;
namespace encore::ctr {
class PodunkHouseDoorFade;
// Borrow only the actual running House models/backends. Native source owners
// below share this one Registry, Tree, Player and gameplay random stream.
struct PodunkHouseExitInput {
  PodunkHouseContinuationInput continuation;
  std::function<bool(upstream::Vec2 &, std::string &)> controls;
  std::function<bool(std::string_view, upstream::PlayerInputQuery, bool &, std::string &)> query_input;
  upstream::FieldMusicChangerHost music_service;
  PodunkPlayerCameraPorts camera_ports;
  PodunkCameraViewport viewport;
  upstream::FieldEquipmentView equipment;
  SourceFontRenderer *font = nullptr;
  const upstream::FieldProgrammeData *programme = nullptr;
  const upstream::FieldPsiData *psi = nullptr;
  const upstream::BasementProgressionData *basement = nullptr;
  const upstream::DialogueChoicesData *choices = nullptr;
  upstream::DialogueChoices *choice_runtime = nullptr;
  upstream::HousePresentation *printer = nullptr;
  upstream::SessionSnapshot *mutable_session = nullptr;
  HouseRenderer *house_renderer = nullptr;
  BattleRenderer *text_renderer = nullptr;
  const upstream::LocaleSelection *locale = nullptr;
  const upstream::AudioBank *inventory_audio_bank = nullptr;
  upstream::Blackbars *bars = nullptr;
  PodunkHouseDoorFade *fade = nullptr;
  upstream::FieldDoorHost house_door;
  PodunkSceneAudioHost audio;
  MusicRegionService *music = nullptr;
  float geometry_grid_size = 0;
  uint64_t scene_epoch = 0;
  bool actual_debug_build = false;
};
class PodunkHouseExit {
public:
  PodunkHouseExit();
  ~PodunkHouseExit();
  PodunkHouseExit(const PodunkHouseExit &) = delete;
  PodunkHouseExit &operator=(const PodunkHouseExit &) = delete;
  // Execute while the old House is Idle, before its unsupported-scene fallback.
  // Source packs, continuation and actual native owners are prepared here.
  bool prepare(PodunkHouseExitInput, std::string &);
  // The dynamic grass factory borrows this exact runtime/Tree after preparation.
  // It is supplied by its concrete source factory owner, not by a success flag.
  bool construct(std::string &);
  bool bind_house(upstream::HouseRuntime &, std::string &);
  bool request(uint32_t source_door, std::string &);
  bool enter(uint32_t source_door, std::string &);
  bool fade_in_done(std::string &);
  bool deferred_commit(std::string &);
  bool tree_changed(std::string &);
  bool fade_out_mostly_done(std::string &);
  bool door_idle(std::string &);
  bool physics_frame(uint64_t, float, bool paused, std::string &);
  bool idle_frame(uint64_t, float, bool paused, bool update_pending,
                  std::string &);
  bool input(uint32_t, const upstream::PlayerInputEvent &, bool accept,
             bool paused, std::string &);
  bool draw(uint64_t, float delta, float global_shader_time, std::string &);
  bool end(std::string &);
  bool ready() const;
  upstream::FieldObjectId player_object() const;
  const upstream::PlayerMotionData *motion_data()const;
  bool player_view_position(upstream::Vec2 &,std::string &)const;
  PodunkHouseContinuation *continuation();
  const upstream::FieldSceneSources *sources() const;
  const upstream::HouseReturnSources *house_return_sources() const;
  const upstream::HouseReturnLadderData *house_return_ladder() const;
  upstream::FieldSceneConsumers consumers() const;
  const std::shared_ptr<upstream::FieldNodeTreeRuntime> &tree() const;
  upstream::FieldGeometrySpace *geometry();
  upstream::FieldDoorRuntime *house_door();
  // These are the same source-state owners used by NPC programmes and saves.
  // They are exposed after actual construction, without granting Scene Ready.
  PodunkInventoryHost *inventory();
  PodunkProgrammeState *programme_state();
  upstream::FieldSceneHost *scene_lifecycle();
  PodunkPlayerHost *player_host();
  const std::string &failure() const;
private:
  struct State;
  std::unique_ptr<State> state_;
  bool scene_step(upstream::FieldDoorSceneStep,
                  const upstream::FieldDoorCandidate &,
                  uint64_t, upstream::Vec2, upstream::Vec2, std::string &);
};
} // namespace encore::ctr
