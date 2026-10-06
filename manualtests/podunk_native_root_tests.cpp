#include "encore/crc32.hpp"
#include "podunk_native_root.hpp"
#include <cassert>
using namespace encore::ctr;
using namespace encore::upstream;
// Explicit manual invocation only. This source is compiled, never auto-run.
void podunk_native_root_manual_negative_cases() {
  PodunkNativeRoot host;
  std::string error;
  FieldGlobalExternalState state;
  FieldMapRect rectangle;
  std::vector<FieldObjectId> objects;
  FieldGlobalExternalSpec spec;
  std::unique_ptr<FieldGlobalExternalObject> owner;
  FieldDeferredMessage message;
  assert(!host.initialize_tree(error));
  assert(!host.initialize_project_tree(error));
  assert(!host.stage_child(1, error));
  assert(!host.stage_current_scene(1, error));
  assert(!host.viewport().ready_notified && host.viewport().blocked == 0);
  assert(!host.bind_child_notifications({}, error));
  assert(!host.finalize_tree(error));
  assert(!host.construct(1, spec, owner, error) && !owner);
  assert(!host.snapshot(true, state, error));
  assert(!host.snapshot(false, state, error));
  assert(!host.add_child(1, error));
  assert(!host.remove_child(1, error));
  assert(!host.move_child(1, -1, error));
  assert(!host.set_current_scene(1, error));
  assert(!host.set_canvas_transform({}, error));
  assert(!host.clear_target(error));
  assert(!host.begin_draw(error));
  assert(!host.world_rect(rectangle, error));
  assert(!host.input_registration(1, 0, true, error));
  assert(!host.input_objects(3, objects, error));
  assert(!host.native_group("_viewports", objects, error));
  assert(!host.enqueue(message, error));
  assert(!host.deferred(false, message, error));
  assert(!host.node_notification(1, FieldTreePhase::ReadyNative, error));
  assert(!host.bind_parent_observer({}, error));
  assert(host.external_parent(1) == 0);
  FieldNativeRootData data;
  FieldGlobalRegistryData registry;
  const uint8_t invalid[128]{};
  assert(!data.load(invalid, sizeof invalid, registry, error) && !data.valid());
  assert(!host.pending_native().empty());
}

// Caller supplies the actual checked original pack and reviewed Registry data.
// No fixture scene, fabricated upstream or synthesized successful owner.
void podunk_native_root_manual_parser_negative_cases(
    const std::vector<uint8_t> &original,
    const FieldGlobalRegistryData &registry) {
  assert(original.size() >= 128);
  std::string error;
  FieldNativeRootData data;
  assert(data.load(original.data(), original.size(), registry, error));
  auto bad = original;
  for (size_t offset :
       {size_t(8), size_t(12), size_t(24), size_t(28), size_t(32), size_t(36),
        size_t(40), size_t(60), size_t(124)}) {
    bad = original;
    bad[offset] ^= 0x40;
    assert(!data.load(bad.data(), bad.size(), registry, error) && data.valid());
  }
  bad = original;
  bad.back() ^= 1;
  assert(!data.load(bad.data(), bad.size(), registry, error));
  assert(!data.load(original.data(), original.size() - 1, registry, error));
  bad = original;
  bad.push_back(0);
  assert(!data.load(bad.data(), bad.size(), registry, error));
  bad = original;
  bad[128] = 255;
  bad[129] = 255;
  bad[130] = 255;
  bad[131] = 127;
  auto crc = encore::crc32(bad.data() + 128, bad.size() - 128);
  for (unsigned i = 0; i < 4; ++i)
    bad[20 + i] = uint8_t(crc >> (8 * i));
  assert(!data.load(bad.data(), bad.size(), registry, error));
}

// Caller must construct and stage all actual source singleton owners and the
// checked original main PackedScene; unknown source bodies cannot satisfy this.
// This manual case is not part of an automatic suite and is never auto-run.
void podunk_native_root_manual_actual_cold_start(
    PodunkNativeRoot &root, const FieldGlobalRegistryData &source) {
  std::string error;
  const auto before = root.viewport();
  assert(!before.inside && !before.ready && !before.ready_notified);
  assert(before.children.size() == source.autoloads().size() + 1);
  assert(before.current_scene == before.children.back());
  assert(root.initialize_project_tree(error));
  assert(root.viewport().inside && root.viewport().ready &&
         root.viewport().ready_notified && root.viewport().blocked == 0);
  const auto children = root.viewport().children;
  assert(!root.initialize_project_tree(error));
  assert(root.viewport().children == children);
}
void podunk_native_root_manual_split_invalid_order() {
  FieldNodeTreeRuntime uninitialized;
  std::string error;
  assert(!uninitialized.ready_entered_branch(error));
  assert(!uninitialized.enter_branch_only(error));
  PodunkNativeRoot uninitialized_root;
  assert(!uninitialized_root.stage_current_scene(0, error));
  assert(!uninitialized_root.initialize_project_tree(error));
  assert(!uninitialized_root.viewport().initialized &&
         !uninitialized_root.viewport().ready_notified);
}
