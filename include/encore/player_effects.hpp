#pragma once
#include "encore/field_native_timer.hpp"
#include "encore/player_initialization.hpp"
#include "encore/source_random.hpp"
namespace encore::upstream {
struct PlayerEffectCreator {
  uint32_t kind = 0, id = 0, parent_hops = 0;
  std::string path, native, script, resource_member, scene, timer_path,
      sprite_path;
  std::string scene_member, objects_path, party_member, animation_path,
      animation;
  std::string finished_signal, finished_method, created_member, timeout_method;
  std::array<uint8_t, 32> script_sha{};
  std::array<double, 2> rng_bounds{};
  double party_depth = 0;
  std::vector<std::string> methods;
};
struct PlayerEffectPolicy {
  std::string after_animation, after_animation_path, after_finished_method,
      after_finished_signal, tint_targets_member, tint_script;
  std::vector<std::string> copied_members, tint_paths;
  FieldColor tint_default{};
};
class PlayerEffectsData {
public:
  bool load(const uint8_t *, size_t, const PlayerInitializationData &,
            std::string &);
  bool load_file(const char *, const PlayerInitializationData &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &player_ir_sha256() const { return player_ir_; }
  const auto &creators() const { return creators_; }
  const auto &policy() const { return policy_; }
  const auto &timer_data() const { return timer_; }
  const FieldNodeRecipeData *recipe(uint32_t) const;
  std::shared_ptr<const GlobalYamlValue> native(uint32_t) const;
  std::shared_ptr<const GlobalYamlValue> signals() const { return signals_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, player_ir_{};
  std::vector<PlayerEffectCreator> creators_;
  PlayerEffectPolicy policy_;
  FieldNativeTimerData timer_;
  std::vector<FieldNodeRecipeData> recipes_;
  std::vector<std::shared_ptr<const GlobalYamlValue>> native_;
  std::shared_ptr<const GlobalYamlValue> signals_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
// These ports are actual native owners, not existence/Ready acknowledgements.
// In particular duplicate_sprite copies the complete source Node subtree and
// allocates its real ObjectIDs; copying a Sprite property snapshot is rejected.
class PlayerEffectsNative {
public:
  virtual ~PlayerEffectsNative() = default;
  virtual bool preload(uint32_t, const FieldNodeRecipeData &,
                       const GlobalYamlValue &, FieldObjectId &,
                       std::string &) = 0;
  virtual bool instance(uint32_t, FieldNodeTreeRuntime &, FieldObjectId &,
                        std::string &) = 0;
  virtual bool duplicate_sprite(FieldObjectId, FieldObjectId &,
                                std::string &) = 0;
  virtual bool copy_sprite(FieldObjectId, FieldObjectId,
                           const std::vector<std::string> &, std::string &) = 0;
  virtual bool global_position(FieldObjectId, Vec2 &, std::string &) const = 0;
  virtual bool set_global_position(FieldObjectId, Vec2, std::string &) = 0;
  virtual bool current_scene(FieldObjectId &, std::string &) const = 0;
  virtual bool party_size(size_t &, std::string &) const = 0;
  virtual bool timer_wait(FieldObjectId, double, std::string &) = 0;
  virtual bool timer_start(FieldObjectId, std::string &) = 0;
  virtual bool timer_stop(FieldObjectId, std::string &) = 0;
  virtual bool animation_play(FieldObjectId, std::string_view,
                              std::string &) = 0;
  // Owns the real signal connection table. Callback is invoked synchronously
  // with the actual animation_finished name; one_shot is disconnected by that
  // same table after the emission's callbacks, as in the native implementation.
  virtual bool connect_finished(
      FieldObjectId, std::string_view, FieldObjectId, std::string_view,
      FieldObjectId, bool,
      std::function<bool(std::string_view, FieldObjectId, std::string &)>,
      std::string &) = 0;
};
struct PlayerEffectCreatorState {
  FieldObjectId object = 0, player = 0, resource = 0, timer = 0, sprite = 0;
  uint32_t kind = 0;
  bool ready = false, failed = false;
  std::vector<FieldObjectId> created_dusts;
};
class PlayerEffectsRuntime {
public:
  bool initialize(const PlayerEffectsData &, const PlayerInitializationData &,
                  FieldGlobalRegistry &, SourceRandom &, PlayerEffectsNative &,
                  std::string &);
  // Actual source script onready, invoked once from the same Tree ReadyScript.
  bool ready(FieldObjectId player, FieldObjectId creator, std::string &);
  bool create_after_image(FieldObjectId, std::string &);
  bool set_interval(FieldObjectId, double, std::string &);
  bool start_creating(FieldObjectId, std::string &);
  bool stop_creating(FieldObjectId, std::string &);
  bool timeout(FieldObjectId timer, std::string &);
  bool create_dust(FieldObjectId, std::string &);
  bool destroy_dust(FieldObjectId, std::string_view, FieldObjectId,
                    std::string &);
  bool after_image_ready(FieldObjectId, std::string &);
  bool after_image_finished(FieldObjectId, std::string_view, std::string &);
  bool tint_ready(FieldObjectId, std::string &);
  bool release(FieldObjectId, std::string &);
  const PlayerEffectCreatorState *state(FieldObjectId) const;

private:
  PlayerEffectCreatorState *creator(FieldObjectId, uint32_t, std::string &);
  bool poison(PlayerEffectCreatorState &, std::string &);
  const PlayerEffectsData *data_ = nullptr;
  const PlayerInitializationData *player_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  SourceRandom *random_ = nullptr;
  PlayerEffectsNative *native_ = nullptr;
  std::map<FieldObjectId, PlayerEffectCreatorState> states_;
  std::map<FieldObjectId, std::vector<FieldObjectId>> tint_targets_;
};
} // namespace encore::upstream
