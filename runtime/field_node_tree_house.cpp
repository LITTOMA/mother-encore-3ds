#include "encore/field_node_tree.hpp"
#include "encore/house_reentry.hpp"
#include <algorithm>
namespace encore::upstream {
bool FieldNodeTreeRuntime::initialize_house_continuation(
    const HouseReentryData &data, FieldNodeTreeHost host, std::string &e) {
  const auto &native = data.native_nodes();
  if (!data.valid() || native.size() != 2 || root_ || !nodes_.empty() ||
      poisoned_ || !host.construct_source || !host.native_allocated ||
      !host.allocate_object || !host.allocate_fast_name || !host.bind ||
      !host.dispatch || !host.deferred || !host.object_exists ||
      !host.input_registration || !host.external_pause_process ||
      !host.release || !host.object_domain ||
      bool(host.enqueue_global) != bool(host.flush_global)) {
    e = "Native House continuation source/factory rejected";
    return false;
  }
  const auto &root = native[0];
  const auto &objects = native[1];
  const auto identity = data.identity();
  std::array<uint8_t, 32> script{}, scene{}, inherited{};
  if (!root.id || !objects.id || root.id == objects.id || root.parent ||
      root.owner_role != 1 || root.node != "." ||
      root.name != data.target_root_name() || root.native_class != "Node2D" ||
      root.script.empty() || objects.parent != root.id ||
      objects.owner_role != 2 || objects.node != data.player_parent() ||
      objects.name != data.player_parent() || objects.native_class != "TileMap" ||
      !objects.script.empty() ||
      std::any_of(objects.script_sha.begin(), objects.script_sha.end(),
                  [](uint8_t v) { return v != 0; }) ||
      !data.source_hash(data.target_scene(), scene) ||
      scene != identity.source_sha256 ||
      !data.source_hash(root.script, script) || script != root.script_sha ||
      !data.source_hash(data.ready().script, inherited) ||
      source_index_.count(root.id) || source_index_.count(objects.id)) {
    e = "Native House continuation checked root/container proof differs";
    return false;
  }
  // These are Godot native Node2D/TileMap constructor defaults, not House
  // tuning. TileMap enables transform notification in its constructor. Its
  // cell_y_sort affects tiles, and does not make this node a YSort instance.
  // The actual House owner retains tile content and rendering.
  const FieldColor white = {1, 1, 1, 1};
  auto descriptor = [&](const HouseReentryNative &source, bool child) {
    FieldNodeDescriptor r;
    r.id = source.id;
    r.parent = source.parent;
    r.owner = r.canvas_parent = child ? root.id : 0;
    r.index = child ? 0 : -1;
    r.class_index = child ? 19 : 1; // Existing native class schema.
    r.path = source.node;
    r.name = source.name;
    r.native_class = source.native_class;
    r.script = source.script;
    r.script_sha = source.script_sha;
    r.script_methods = child ? 0 : 1; // Reviewed inherited _ready inventory bit.
    r.flags = 1u | 2u | 64u | (child ? 256u : 0u);
    r.light_mask = 1;
    r.local = {source.local.x, source.local.y, source.local.origin};
    r.world = r.local;
    r.modulate = r.self_modulate = white;
    return r;
  };
  auto root_record = descriptor(root, false);
  auto objects_record = descriptor(objects, true);
  const auto &a = root_record.local;
  const auto &b = objects_record.local;
  objects_record.world = {
      Vec2{a[0].x * b[0].x + a[1].x * b[0].y,
           a[0].y * b[0].x + a[1].y * b[0].y},
      Vec2{a[0].x * b[1].x + a[1].x * b[1].y,
           a[0].y * b[1].x + a[1].y * b[1].y},
      Vec2{a[0].x * b[2].x + a[1].x * b[2].y + a[2].x,
           a[0].y * b[2].x + a[1].y * b[2].y + a[2].y}};
  host_ = std::move(host);
  FieldObjectId out = 0;
  if (!instantiate_records(identity, {root_record, objects_record}, root.id,
                           true, out, e))
    return false;
  root_ = out;
  for (auto stable : {root.id, objects.id}) {
    auto i = std::find_if(nodes_.begin(), nodes_.end(), [&](const auto &entry) {
      const auto &owned = sources_.at(entry.first).identity;
      return entry.second.source == stable &&
             owned.scene_id == identity.scene_id &&
             owned.source_sha256 == identity.source_sha256 &&
             owned.upstream_commit == identity.upstream_commit;
    });
    if (i == nodes_.end() || !source_index_.emplace(stable, i->first).second) {
      poisoned_ = true;
      e = "Native House continuation complete source index rejected";
      return false;
    }
  }
  e.clear();
  return true;
}
} // namespace encore::upstream
