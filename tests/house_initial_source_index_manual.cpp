#include "encore/field_node_tree.hpp"
#include "manual_require.hpp"

using namespace encore::upstream;
// An explicit manual driver supplies its real ObjectDB/native factory. This
// wraps allocation only; it never supplies replacement source/native receipts.
void house_initial_source_index_manual(const FieldNodeTreeData &data,
    FieldNodeTreeHost actual_host) {
  MANUAL_REQUIRE(data.valid() && actual_host.native_allocated &&
                 actual_host.construct_source && actual_host.object_exists);
  FieldNodeTreeRuntime tree;
  const auto allocate = actual_host.native_allocated;
  const auto exists = actual_host.object_exists;
  size_t witnessed = 0;
  actual_host.native_allocated = [&](FieldObjectId object,
      const FieldNodeDescriptor &source, const FieldIdentity &identity,
      std::string &error) {
    MANUAL_REQUIRE(tree.source_object(source.id) == object);
    const auto *state = tree.state(object);
    MANUAL_REQUIRE(state && state->alive && !state->bound && !state->inside &&
                   !state->ready_notified && state->source == source.id);
    MANUAL_REQUIRE(tree.descriptor(object)->id == source.id);
    FieldIdentity published;
    MANUAL_REQUIRE(tree.object_identity(object, published));
    MANUAL_REQUIRE(published.scene_id == identity.scene_id &&
                   published.upstream_commit == identity.upstream_commit &&
                   published.source_sha256 == identity.source_sha256);
    if (!allocate(object, source, identity, error)) return false;
    MANUAL_REQUIRE(exists(object));
    ++witnessed;
    return true;
  };
  std::string error;
  MANUAL_REQUIRE(tree.initialize(data, std::move(actual_host), error));
  MANUAL_REQUIRE(witnessed >= data.records().size());
  MANUAL_REQUIRE(!tree.state(tree.root())->inside &&
                 !tree.state(tree.root())->ready_notified);
  // Unknown IDs still resolve to no actual object after factory completion.
  MANUAL_REQUIRE(tree.source_object(0) == 0);
}
