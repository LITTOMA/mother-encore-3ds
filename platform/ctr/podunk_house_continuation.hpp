#pragma once
#include "encore/field_scene_destination.hpp"
#include "encore/fresh_house.hpp"
#include "encore/house_status_effects.hpp"
#include "encore/player_named_sfx.hpp"
#include "house_ui_continuation.hpp"
#include "podunk_global_data_singleton.hpp"
#include "podunk_global_native.hpp"
#include "podunk_house_global_bridge.hpp"

namespace encore::ctr {
class AudioPlayer;
class PodunkNamedSfx;
class PodunkAudioServer;
class MusicRegionService;
// Borrow the actual running House models and gameplay entropy. The caller
// keeps these owners alive until the same UI has rebound to its new scene.
struct PodunkHouseContinuationInput {
  const upstream::FieldSceneDestinationData *destination = nullptr;
  std::string romfs_root;
  const upstream::FreshHouseState *house = nullptr;
  const upstream::BattleEntry *battle = nullptr;
  const upstream::BattleOutcome *outcome = nullptr;
  const upstream::FieldEquipmentMenu *commands = nullptr;
  const upstream::NativeSessionData *session = nullptr;
  upstream::RoomView room{};
  upstream::HouseView house_data{};
  upstream::DrawerProgramView drawer{};
  upstream::RoundView round{};
  upstream::ItemView legacy_items{};
  const upstream::SessionSnapshot *snapshot = nullptr;
  upstream::SourceRandom *played_random = nullptr;
  std::vector<uint32_t> *uid_ledger = nullptr;
  C3D_RenderTarget *target = nullptr;
  AudioPlayer *audio = nullptr;
  upstream::FieldObjectSignals *signals = nullptr;
  upstream::LoadRngClockProvider clock;
  std::function<bool(std::string &, std::string &)> locale;
  // An actual source signal consumer may be supplied by the complete scene
  // owner. If absent, later flag mutation rejects; constructor/adoption does
  // not pretend to emit a source signal.
  upstream::FieldGlobalFlagsRuntime::Emit flags_updated;
};
// Concrete existing-session ObjectDB composition. No cold LOAD, original
// project Ready, destination/player instantiation or scene activation occurs.
class PodunkHouseContinuation final : private PodunkUiExternalFactory {
public:
  PodunkHouseContinuation();
  ~PodunkHouseContinuation();
  PodunkHouseContinuation(const PodunkHouseContinuation &) = delete;
  PodunkHouseContinuation &operator=(const PodunkHouseContinuation &) = delete;
  bool initialize(PodunkHouseContinuationInput, std::string &);
  bool initialized() const;
  bool bind_named_sfx(std::shared_ptr<const upstream::PlayerNamedSfxData>,PodunkAudioServer&,MusicRegionService&,std::string&);
  PodunkNamedSfx *named_sfx();
  upstream::FieldGlobalRegistry *registry();
  PodunkNativeRoot *native_root();
  PodunkGlobalHost *global();
  PodunkGlobalDataHost *characters();
  HouseUiContinuation *ui();
  PodunkHouseGlobalBridge *bridge();
  const upstream::HouseGlobalBridgeData *bridge_data() const;
  const upstream::HouseUiContinuationData *ui_continuation_data() const;
  const upstream::FieldInventoryData *inventory_data() const;
  const upstream::FieldItemDefinitions *item_definitions() const;
  const upstream::FieldItemDefinitions *global_item_definitions() const;
  const upstream::FieldCharacterLoadData *character_data() const;
  const upstream::PlayerInitializationData *player_initialization() const;
  const upstream::PlayerReadyData *player_ready() const;
  std::shared_ptr<const upstream::PlayerInitializationData>
  player_initialization_owner() const;
  std::shared_ptr<const upstream::PlayerReadyData> player_ready_owner() const;
  const upstream::HouseStatusEffectsData *status_effects() const;
  const upstream::HouseStatusEffectsRuntime *status_runtime() const;
  AudioPlayer *audio() const;
  upstream::SourceRandom *random() const;
  std::vector<uint32_t> *uid_ledger() const;
  upstream::FieldObjectSignals *signals() const;
  bool inventory_snapshot(PodunkInventorySnapshot &, std::string &) const;
  bool signal_declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                          std::string &) const;
  // Extend only by an actual typed owner after this composition is live.
  bool
  bind_scene_signal_declarations(upstream::FieldObjectSignals::DeclarationQuery,
                                 std::string &);

private:
  struct State;
  std::unique_ptr<State> state_;
  bool attempted_ = false;
  bool construct(upstream::FieldObjectId,
                 const upstream::FieldGlobalExternalSpec &,
                 std::unique_ptr<upstream::FieldGlobalExternalObject> &,
                 std::string &) override;
};
} // namespace encore::ctr
