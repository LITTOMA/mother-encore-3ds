#include "encore/player_initialization.hpp"
#include <cassert>
using namespace encore::upstream;
namespace encore::upstream::player_initialization_detail {
bool admit_dynamic_ready_subtree(const PlayerInitializationData &,
                                 const FieldNodeTreeRuntime &, FieldObjectId,
                                 std::string &);
}
// Manual cases only. No automatic registration or execution.
int main() {
  std::string error;
  GlobalDataConstructorData constructor;
  PlayerInitializationData data;
  std::vector<uint8_t> truncated(128, 0);
  assert(!data.load(nullptr, 0, constructor, error));
  assert(!data.load(truncated.data(), truncated.size(), constructor, error));
  assert(!data.valid() && !data.ready_admitted());
  PlayerInitializationRuntime runtime;
  assert(!runtime.run(error) && !runtime.complete() && !runtime.player());
  PlayerInitializationBody body;
  PlayerInitializationMember before;
  before.object = 42;
  assert(!body.member("", before, error) && before.object == 42);
}
// Given real format-admitted 0053 and candidate bytes: unknown capabilities,
// damaged CRC, trailing data and a cut payload must reject transactionally.
void player_initialization_parser_manual(
    const std::vector<uint8_t> &bytes, const GlobalDataConstructorData &ctor) {
  assert(bytes.size() > 128);
  std::string error;
  PlayerInitializationData data;
  assert(data.load(bytes.data(), bytes.size(), ctor, error));
  const auto prior = data.ir_sha256();
  for (auto offset : {size_t(8), size_t(20), size_t(28), bytes.size() - 1}) {
    auto damaged = bytes;
    damaged[offset] ^= 0x80;
    assert(!data.load(damaged.data(), damaged.size(), ctor, error));
    assert(data.valid() && data.ir_sha256() == prior);
  }
  assert(!data.load(bytes.data(), bytes.size() - 1, ctor, error));
  assert(data.valid() && data.ir_sha256() == prior);
}
// Supply actual checked dynamic factories: one complete source Player whose
// native/script Ready has not executed, and a different actual source root.
// Neither is authorized by sharing stable IDs with a static scene index.
void player_initialization_dynamic_roots_manual(
    const PlayerInitializationData &data, const FieldNodeTreeRuntime &tree,
    FieldObjectId player_before_ready, FieldObjectId different_source_root) {
  std::string error;
  using player_initialization_detail::admit_dynamic_ready_subtree;
  assert(data.valid() && tree.state(player_before_ready));
  assert(!admit_dynamic_ready_subtree(data, tree, 0, error));
  assert(!admit_dynamic_ready_subtree(data, tree, player_before_ready, error));
  assert(
      !admit_dynamic_ready_subtree(data, tree, different_source_root, error));
}
// After the real factory/add_child has completed every native/script owner,
// a renamed player root and an identical recipe in another actual instance
// must be checked relative to the supplied root, never source_object().
void player_initialization_dynamic_ready_manual(
    const PlayerInitializationData &data, const FieldNodeTreeRuntime &tree,
    FieldObjectId actual_player) {
  std::string error;
  assert(player_initialization_detail::admit_dynamic_ready_subtree(
      data, tree, actual_player, error));
}
