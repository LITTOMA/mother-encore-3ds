#pragma once
#include "encore/field_door.hpp"
#include "encore/house_data.hpp"
#include "encore/room_data.hpp"
#include <map>
namespace encore::upstream {
struct HouseReentryFlag {
  uint32_t index = 0, id = 0;
  std::string name;
};
struct HouseReentryActor {
  uint32_t house_index = 0, room_index = 0, id = 0, body = 0;
  std::string node;
  Vec2 position{}, direction{};
};
struct HouseReentryBody {
  uint32_t index = 0, id = 0;
  std::string node;
};
struct HouseReentryLandmark {
  std::string node, appear, disappear;
  bool delete_if_hidden = false;
};
struct HouseReentryNative {
  uint32_t id = 0, parent = 0, owner_role = 0;
  std::string node, name, native_class, script;
  std::array<uint8_t, 32> script_sha{};
  FieldGeometryTransform local{};
};
struct HouseReentryReady {
  std::string script, visit_method, flying_method, visit_flag, magicant_region,
      flying_flag, flying_character, party_npcs_member;
  std::string switch_signal, switch_method;
};
struct HouseReentryTile {
  uint32_t id = 0;
  bool present = false;
};
// Complete placed-cell/native TileSet proof. This bounded continuation only
// supports source TileMaps with no native collision parts; bodies are owned by
// the separately checked House geometry resource.
struct HouseReentryTileMap {
  uint32_t id = 0, layer = 0, mask = 0, cell_count = 0;
  std::string node, resource, resource_source;
  std::array<uint8_t, 32> cell_sha{}, resource_sha{};
  std::vector<HouseReentryTile> tiles;
};
// A bounded continuation resource. Loading binds actual source doors and the
// existing original House/Room consumers. It never initializes gameplay,
// evaluates flags, allocates nodes, grants Ready or consumes the random stream.
class HouseReentryData {
public:
  bool load(const uint8_t *, size_t, const FieldDoorData &, RoomView, HouseView,
            std::string &);
  bool load_file(const char *, const FieldDoorData &, RoomView, HouseView,
                 std::string &);
  bool matches(const FieldDoorData &, RoomView, HouseView, std::string &) const;
  bool valid() const { return valid_; }
  const FieldIdentity &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  uint32_t door_id() const { return door_; }
  const std::string &source_scene() const { return source_; }
  const std::string &target_scene() const { return target_; }
  const std::string &target_root_name() const { return root_; }
  const std::string &source_region() const { return source_region_; }
  const std::string &target_region() const { return region_; }
  const std::string &player_parent() const { return player_parent_; }
  Vec2 position() const { return position_; }
  Vec2 direction() const { return direction_; }
  bool empty_target_params() const { return valid_; }
  const std::string &body_signal() const { return body_signal_; }
  const std::string &body_method() const { return body_method_; }
  const std::string &door_signal(FieldDoorSignal s) const {
    return door_signals_[uint32_t(s)];
  }
  const std::string &scene_changed_signal() const { return changed_; }
  const std::string &deferred_method() const { return deferred_; }
  const std::string &area_left_signal() const { return left_; }
  uint32_t area_left_arguments() const { return left_arguments_; }
  const std::string &party_changed_signal() const { return party_changed_; }
  const std::string &party_changed_wait_signal() const { return party_idle_; }
  const HouseReentryReady &ready() const { return ready_; }
  const auto &flags() const { return flags_; }
  const auto &actors() const { return actors_; }
  const auto &bodies() const { return bodies_; }
  const auto &landmarks() const { return landmarks_; }
  const auto &native_nodes() const { return native_; }
  const auto &tilemaps() const { return tilemaps_; }
  const auto &steps() const { return steps_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{};
  uint32_t door_ = 0;
  uint32_t left_arguments_ = 0;
  std::string source_, target_, root_, source_region_, region_, player_parent_;
  std::string body_signal_, body_method_, changed_, deferred_, left_;
  std::string party_changed_, party_idle_;
  std::array<std::string, 3> door_signals_;
  Vec2 position_{}, direction_{};
  HouseReentryReady ready_;
  std::vector<HouseReentryFlag> flags_;
  std::vector<HouseReentryActor> actors_;
  std::vector<HouseReentryBody> bodies_;
  std::vector<HouseReentryLandmark> landmarks_;
  std::vector<HouseReentryNative> native_;
  std::vector<HouseReentryTileMap> tilemaps_;
  std::vector<FieldDoorSceneStep> steps_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  std::array<uint8_t, 32> room_sha_{}, house_sha_{}, door_ir_{};
  uint32_t room_size_ = 0, house_size_ = 0;
};
} // namespace encore::upstream
