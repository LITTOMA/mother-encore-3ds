#include "encore/house_return_sources.hpp"
#include "encore/audio_data.hpp"
#include "manual_require.hpp"
#include <algorithm>

using namespace encore::upstream;
namespace {
void word(std::vector<uint8_t> &bytes, size_t offset, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = uint8_t(value >> (8 * i));
}
}
// Called only by an explicit manual driver with the actual admitted files.
// No scene is instantiated, no source Ready is fabricated, and no RNG is used.
void house_npc_world_identity_manual(const HouseReturnSources &sources,
    const std::vector<uint8_t> &world_bytes,
    const std::vector<uint8_t> &npc_bytes,
    const std::vector<uint8_t> &geometry_bytes) {
  MANUAL_REQUIRE(sources.valid());
  const auto &tree = sources.tree();
  const auto &geometry = sources.geometry();
  const auto &npcs = sources.npcs();
  MANUAL_REQUIRE(tree.identity().scene_id != geometry.identity().scene_id);
  std::string error;
  FieldNpcWorldData actual;
  MANUAL_REQUIRE(actual.load(world_bytes.data(), world_bytes.size(), tree,
                             npcs, geometry, error));
  MANUAL_REQUIRE(actual.npc(npcs.npcs().front().id));
  const auto *kept = actual.npc(npcs.npcs().front().id);
  auto rejected = [&](const std::vector<uint8_t> &bytes,
                      const FieldNpcData &npc, const FieldGeometryView &geo) {
    FieldNpcWorldData empty;
    MANUAL_REQUIRE(!empty.load(bytes.data(), bytes.size(), tree, npc, geo, error));
    MANUAL_REQUIRE(!empty.valid());
    MANUAL_REQUIRE(!actual.load(bytes.data(), bytes.size(), tree, npc, geo, error));
    MANUAL_REQUIRE(actual.valid() && actual.npc(npcs.npcs().front().id) == kept);
  };
  auto wrong_world = world_bytes;
  word(wrong_world, 36, geometry.identity().scene_id);
  rejected(wrong_world, npcs, geometry);
  // Individually well-formed geometry still cannot borrow a different pin/SHA.
  for (size_t offset : {size_t(40), size_t(60)}) {
    auto foreign = geometry_bytes;
    foreign.at(offset) ^= 1;
    auto identity = geometry.identity();
    if (offset == 40) identity.upstream_commit[0] ^= 1;
    else identity.source_sha256[0] ^= 1;
    FieldGeometryView other;
    MANUAL_REQUIRE(other.load(foreign.data(), foreign.size(), identity, error));
    rejected(world_bytes, npcs, other);
  }
  // ENCNPC01 owns the complete native-tree stable IDs, independently of Geo.
  auto foreign_npcs = npc_bytes;
  word(foreign_npcs, 52, geometry.identity().scene_id);
  word(foreign_npcs, 16, 0);
  word(foreign_npcs, 16, audio_crc32(foreign_npcs.data(), foreign_npcs.size()));
  FieldNpcData other_npcs;
  MANUAL_REQUIRE(other_npcs.load(foreign_npcs.data(), foreign_npcs.size(), error));
  rejected(world_bytes, other_npcs, geometry);
  wrong_world = world_bytes;
  wrong_world.at(60) ^= 1;
  rejected(wrong_world, npcs, geometry);
  FieldNpcData missing_npcs;
  rejected(world_bytes, missing_npcs, geometry);
}
