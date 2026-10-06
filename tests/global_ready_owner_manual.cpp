#include "manual_require.hpp"
#include "podunk_global_ready.hpp"
namespace encore::ctr {
// Application-owned positive lifecycle needs the actual source constructor,
// root, settings files, every Player child and full cold LOAD owner. This
// explicit manual hook verifies rejection at an actual wrong cursor instead
// of creating a proxy global or manufacturing a successful Ready receipt.
void global_ready_wrong_cursor_manual(
    PodunkGlobalReady &ready, upstream::FieldNodeTreeRuntime &tree,
    upstream::FieldObjectId object, const upstream::FieldNodeBinding &binding) {
  const auto *state = tree.state(object);
  MANUAL_REQUIRE(state);
  MANUAL_REQUIRE(!state->inside || !state->ready_notified ||
                 state->ready_first);
  auto cursor = ready.cursor();
  std::string error;
  MANUAL_REQUIRE(!ready.source_ready(tree, object, binding, error));
  MANUAL_REQUIRE(ready.cursor() == cursor);
  MANUAL_REQUIRE(!ready.complete());
  MANUAL_REQUIRE(!error.empty());
}
} // namespace encore::ctr
