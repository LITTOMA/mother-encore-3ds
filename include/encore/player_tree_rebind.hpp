#pragma once
#include "encore/player_initialization.hpp"

namespace encore::upstream {
// Admission only: the caller transfers the actual subtree and ObjectDB owners
// first. Actual Exit clears ready_notified and retains ready_first=false.
// This never allocates, changes source state, or requests Ready.
inline bool player_rebind_node(const PlayerInitializationData &data,
                               const FieldGlobalRegistry &registry,
                               const FieldNodeTreeRuntime &next,
                               FieldObjectId object, uint32_t source,
                               std::string &error) {
  const auto *expected = data.recipe().record(source);
  const auto *descriptor = next.descriptor(object);
  const auto *state = next.state(object);
  FieldIdentity identity;
  const auto &pin = data.recipe().identity();
  if (!data.valid() || registry.poisoned() ||
      next.object_domain() != registry.kernel() ||
      !registry.object_exists(object) ||
      registry.tree_owner(object).get() != &next || !expected ||
      !descriptor || !state || !state->alive || state->inside ||
      state->queued || !state->bound || state->ready_first ||
      state->ready_notified || !next.object_identity(object, identity) ||
      identity.scene_id != pin.scene_id ||
      identity.source_sha256 != pin.source_sha256 ||
      identity.upstream_commit != pin.upstream_commit ||
      descriptor->id != expected->id ||
      descriptor->native_class != expected->native_class ||
      descriptor->script != expected->script ||
      descriptor->script_sha != expected->script_sha ||
      descriptor->class_index != expected->class_index) {
    error = "Player rebind requires transferred original Ready source object";
    return false;
  }
  return true;
}
} // namespace encore::upstream
