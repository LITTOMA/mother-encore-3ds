#include "encore/house_runtime.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
bool HouseRuntime::bind_scene_doors(
    const FieldDoorData &data, FieldDoorRuntime &runtime,
    std::function<bool(uint64_t &, std::string &)> player, std::string &error) {
  auto reject = [&](const char *text) {
    error = text;
    return false;
  };
  if (!world_ || !content_.valid() || !data.valid() ||
      runtime.data() != &data || !player || phase_ != HousePhase::Idle)
    return reject("House external Door owner unavailable");
  const auto room = world_->content();
  auto scene = room.string(room.scene().source_scene_string);
  if (scene.substr(0, 6) == "res://")
    scene.remove_prefix(6);
  const auto identity = data.identity();
  const auto raw = room.bytes();
  // Room header stores its pinned source commit at 56, and the source closure
  // is independently checked by FieldDoorData before this binding.
  if (scene != data.source_scene() || room.byte_size() < 76 ||
      !std::equal(identity.upstream_commit.begin(),
                  identity.upstream_commit.end(), raw + 56))
    return reject("House external Door scene/pin differs");
  std::array<uint8_t, 32> source{};
  if (!data.source_hash(scene, source) || source != identity.source_sha256)
    return reject("House external Door source proof differs");
  std::vector<uint32_t> ids(content_.count(HouseSection::Boundaries), 0);
  for (uint32_t j = 0; j < data.door_count(); ++j) {
    const auto door = data.door(j);
    if (!runtime.source_ready(door.id) || data.string(door.target_path).empty())
      return reject("House external Door is not actual Ready");
    // The current House pack has axis-aligned RectangleShape2D boundaries. A
    // changed/sheared native source must get a different geometry consumer.
    const auto t = door.body_transform;
    if (t.x.y != 0 || t.y.x != 0 || t.x.x == 0 || t.y.y == 0)
      return reject("House external Door affine geometry unsupported");
    const Vec2 center{t.origin.x + t.x.x * door.shape_offset.x,
                      t.origin.y + t.y.y * door.shape_offset.y};
    const Vec2 extents{std::abs(t.x.x) * door.extents.x,
                       std::abs(t.y.y) * door.extents.y};
    uint32_t found = house_no_index;
    for (uint32_t i = 0; i < ids.size(); ++i) {
      const auto b = content_.boundary(i);
      if (content_.string(b.source_path) != data.string(door.node))
        continue;
      if (found != house_no_index ||
          b.kind != uint32_t(HouseBoundaryKind::UnsupportedScene) ||
          b.center.x != center.x || b.center.y != center.y ||
          b.extents.x != extents.x || b.extents.y != extents.y)
        return reject("House external Door boundary geometry differs");
      found = i;
    }
    if (found == house_no_index || ids[found])
      return reject("House external Door boundary identity missing/duplicate");
    ids[found] = door.id;
  }
  scene_door_data_ = &data;
  scene_door_runtime_ = &runtime;
  scene_door_player_ = std::move(player);
  scene_door_ids_ = std::move(ids);
  error.clear();
  return true;
}
bool HouseRuntime::request_scene_door(uint32_t boundary) {
  if (!scene_door_runtime_ || !scene_door_player_ ||
      boundary >= scene_door_ids_.size() || !scene_door_ids_[boundary])
    return fail("House external Door request not bound");
  uint64_t player = 0;
  scene_door_error_.clear();
  if (!scene_door_player_(player, scene_door_error_) || !player) {
    if (scene_door_error_.empty())
      scene_door_error_ = "House actual player ObjectID unavailable";
    return fail(scene_door_error_.c_str());
  }
  if (!scene_door_runtime_->body_entered(scene_door_ids_[boundary], player,
                                         scene_door_error_))
    return fail(scene_door_error_.c_str());
  if (scene_door_runtime_->phase() != FieldDoorPhase::BodyIdle)
    return true; // Source body guard ignored a nonplayer/entering body.
  active_ = boundary;
  phase_ = HousePhase::SceneDoorPending;
  scene_door_requests_.push_back({boundary, scene_door_ids_[boundary], player});
  event(HouseEventKind::SceneDoorRequested, boundary);
  return true;
}
} // namespace encore::upstream
