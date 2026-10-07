#pragma once
#include "encore/field_global_constructor.hpp"
#include "encore/player_visual_scripts.hpp"
namespace encore::upstream {
struct PlayerFetcherRecord {
  uint32_t id = 0, target = 0, ready = 0;
  bool ignore = false, toggle_flip = false;
  double reflect_offset = 0;
  std::string path, target_path, sprite_path;
  std::array<uint8_t, 32> script_sha{};
};
struct PlayerFetcherExports {
  std::string path, ignore, flip, offset;
  bool default_ignore = false, default_flip = false, initial_has = false;
  double default_offset = 0;
};
struct PlayerFetcherGetter {
  std::string method, type, member;
};
class PlayerFetcherData {
public:
  bool load(const uint8_t *, size_t, const PlayerInitializationData &,
            const FieldNodeTreeData &, const FieldGlobalConstructorData &,
            std::string &);
  bool load_file(const char *, const PlayerInitializationData &,
                 const FieldNodeTreeData &, const FieldGlobalConstructorData &,
                 std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &scene_identity() const { return scene_identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &player_ir_sha256() const { return player_ir_; }
  const auto &global_ir_sha256() const { return global_ir_; }
  const auto &script() const { return script_; }
  const auto &script_sha256() const { return script_sha_; }
  const auto &current_scene_member() const { return current_scene_; }
  const auto &reflector_path() const { return reflector_; }
  const auto &records() const { return records_; }
  const auto &exports() const { return exports_; }
  const auto &getters() const { return getters_; }
  const auto &sprite_getter() const { return sprite_getter_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{}, scene_identity_{};
  std::array<uint8_t, 32> ir_{}, player_ir_{}, global_ir_{}, tree_ir_{},
      script_sha_{}, global_sha_{};
  std::string scene_, scene_source_, script_, global_script_, current_scene_,
      reflector_, sprite_getter_;
  PlayerFetcherExports exports_;
  std::vector<PlayerFetcherRecord> records_;
  std::vector<PlayerFetcherGetter> getters_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
struct PlayerFetcherSpriteState {
  FieldObjectId texture = 0;
  uint32_t columns = 0, rows = 0, frame = 0;
  bool visible = false;
};
// Reads the existing native Sprite owner; no duplicated animation clock/state.
class PlayerFetcherSpriteReader {
public:
  virtual ~PlayerFetcherSpriteReader() = default;
  virtual const FieldGlobalRegistry *registry() const = 0;
  virtual const FieldNodeTreeRuntime *tree() const = 0;
  virtual bool read(FieldObjectId, PlayerFetcherSpriteState &,
                    std::string &) const = 0;
};
struct PlayerFetcherState {
  bool export_applied = false, onready = false, has_reflection = false;
  FieldObjectId sprite = 0, parent = 0, reflector = 0, reflection = 0;
};
// One owner per actual recipe instance. Implicit onready is invoked only at
// ReadyScript; the normal Idle notification performs the source process.
class PlayerFetcherRuntime final : public PlayerVisualFetcherOwner {
public:
  bool rebind_tree(FieldNodeTreeRuntime &, std::string &);
  // Prepare dependencies at the actual root construction cursor; no Node or
  // Ready is created. Construct is called at each original script attachment.
  bool prepare(const PlayerFetcherData &, const PlayerInitializationData &,
               FieldNodeTreeRuntime &, FieldGlobalRegistry &,
               FieldGlobalConstructorRuntime &, PlayerFetcherSpriteReader &,
               std::string &);
  bool construct(FieldObjectId, const FieldNodeDescriptor &, std::string &);
  bool initialize(const PlayerFetcherData &, const PlayerInitializationData &,
                  FieldNodeTreeRuntime &, FieldGlobalRegistry &,
                  FieldGlobalConstructorRuntime &, FieldObjectId actual_player,
                  FieldObjectId actual_fetcher, PlayerFetcherSpriteReader &,
                  std::string &);
  bool apply_exports(std::string &);
  bool ready(FieldTreePhase, const FieldNodeBinding &, std::string &);
  bool process(FieldTreePhase, bool actual_tree_paused, std::string &);
  bool generate_reflection(std::string &);
  bool delete_reflection(std::string &);
  const FieldGlobalRegistry *registry() const override { return registry_; }
  const FieldNodeTreeRuntime *tree() const override { return tree_; }
  FieldObjectId object() const override { return object_; }
  bool onready_complete() const override { return state_.onready; }
  bool sprite_object(FieldObjectId &, std::string &) const override;
  bool frame(uint32_t &, std::string &) const override;
  bool texture(FieldObjectId &, std::string &) const;
  bool hframes(uint32_t &, std::string &) const;
  bool vframes(uint32_t &, std::string &) const;
  bool visibility(bool &, std::string &) const;
  const PlayerFetcherState &state() const { return state_; }
  const PlayerFetcherRecord *source_record() const { return row_; }

private:
  bool live(bool, std::string &) const;
  bool reflector(FieldObjectId &, std::string &) const;
  bool sample(PlayerFetcherSpriteState &, std::string &) const;
  const PlayerInitializationData *player_data_ = nullptr;
  const PlayerFetcherData *data_ = nullptr;
  const PlayerFetcherRecord *row_ = nullptr;
  FieldNodeTreeRuntime *tree_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  FieldGlobalConstructorRuntime *global_ = nullptr;
  PlayerFetcherSpriteReader *sprites_ = nullptr;
  FieldObjectId object_ = 0;
  PlayerFetcherState state_;
};
} // namespace encore::upstream
