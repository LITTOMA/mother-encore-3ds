#include "encore/field_global_registry.hpp"
#include <cassert>
namespace encore::upstream::manual {
// Run only on explicit manual verification, with the actual prepared Registry.
// A reserved handle cannot be observed or used as a substitute for an owner.
void pending_allocation_rejects_missing_owner(FieldGlobalRegistry &registry) {
  std::string error;
  FieldObjectId id = 0;
  assert(registry.allocate_object(id, error));
  assert(registry.allocation_pending(id));
  assert(!registry.object_exists(id));
  assert(!registry.tree_owner(id));
  assert(!registry.publish_allocated_node(nullptr, id, {}, error));
  assert(registry.allocation_pending(id));
  assert(!registry.object_exists(id));
  assert(registry.retire_object(id, error));
  assert(!registry.allocation_pending(id));
}
// Invoke from the source construction cursor of a real checked native node.
// Publication supplies native identity, never future parenting or Ready.
void actual_native_registration_precedes_script(
    FieldGlobalRegistry &registry,
    const std::shared_ptr<FieldNodeTreeRuntime> &tree, FieldObjectId id) {
  std::string error;
  assert(tree && registry.object_exists(id));
  assert(!registry.allocation_pending(id));
  assert(registry.tree_owner(id) == tree);
  const auto *node = tree->state(id);
  assert(node && node->alive && !node->inside && !node->ready_notified);
  assert(!node->bound && !node->parent && !node->owner && node->name.empty());
  assert(!registry.publish_allocated_node(tree, id, {}, error));
  assert(registry.object_exists(id) && registry.tree_owner(id) == tree);
}
} // namespace encore::upstream::manual
