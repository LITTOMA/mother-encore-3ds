// Manual only; consumes actual generated resources. No scene/Ready/RNG run.
#include "encore/field_npc.hpp"
#include "encore/field_npc_world.hpp"
#include "encore/crc32.hpp"
#include "manual_require.hpp"
#include <algorithm>

using namespace encore::upstream;
namespace {
void word(std::vector<uint8_t> &b, size_t at, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) b.at(at + i) = uint8_t(value >> (8 * i));
}
void npc_crc(std::vector<uint8_t> &b) {
  word(b, 16, 0); word(b, 16, encore::crc32(b.data(), b.size()));
}
void world_crc(std::vector<uint8_t> &b) {
  word(b, 20, encore::crc32(b.data() + 128, b.size() - 128));
}
}

// The explicit caller supplies admitted complete House Tree/Geometry and
// real current NPC/NPCWorld bytes; no Podunk resource may stand in for House.
void house_return_npc_resource_manual(const std::vector<uint8_t> &npc_bytes,
    const std::vector<uint8_t> &world_bytes, const FieldNodeTreeData &tree,
    const FieldGeometryView &geometry) {
  std::string error;
  FieldNpcData npc;
  MANUAL_REQUIRE(npc.load(npc_bytes.data(), npc_bytes.size(), error));
  MANUAL_REQUIRE(npc.npcs().size() == 6 && npc.scene_id() == tree.identity().scene_id);
  FieldNpcWorldData world;
  MANUAL_REQUIRE(world.load(world_bytes.data(), world_bytes.size(), tree, npc,
                            geometry, error));
  MANUAL_REQUIRE(world.bodies().size() == 6 && world.rays().size() == 6 &&
                 world.callbacks().size() == 13);
  const auto kept_npc = npc.npcs().front().id;
  const auto *kept_ray = world.ray(world.rays().front().id);
  auto reject_npc = [&](std::vector<uint8_t> bad) {
    FieldNpcData empty;
    MANUAL_REQUIRE(!empty.load(bad.data(), bad.size(), error) && !empty.valid());
    MANUAL_REQUIRE(!npc.load(bad.data(), bad.size(), error));
    MANUAL_REQUIRE(npc.valid() && npc.npcs().front().id == kept_npc);
  };
  // Header version/capability/rules/count and first record flags.
  for (size_t at : {size_t(8), size_t(20), size_t(24), size_t(28),
                    size_t(172)}) {
    auto bad = npc_bytes; word(bad, at, UINT32_MAX); npc_crc(bad); reject_npc(bad);
  }
  auto missing_id = npc_bytes; word(missing_id, 164, 0);
  npc_crc(missing_id); reject_npc(missing_id);
  for (size_t end : {size_t(63), size_t(164), npc_bytes.size() - 1}) {
    auto bad = npc_bytes; bad.resize(end); reject_npc(bad);
  }
  // A well-formed foreign pin or scene ID still cannot bind this House tree.
  for (size_t at : {size_t(32), size_t(52), size_t(164)}) {
    auto bad = npc_bytes; bad.at(at) ^= 1; npc_crc(bad);
    FieldNpcData foreign; MANUAL_REQUIRE(foreign.load(bad.data(), bad.size(), error));
    MANUAL_REQUIRE(!world.load(world_bytes.data(), world_bytes.size(), tree,
                              foreign, geometry, error));
    MANUAL_REQUIRE(world.valid() && world.ray(kept_ray->id) == kept_ray);
  }
  auto reject_world = [&](std::vector<uint8_t> bad) {
    FieldNpcWorldData empty;
    MANUAL_REQUIRE(!empty.load(bad.data(), bad.size(), tree, npc, geometry, error));
    MANUAL_REQUIRE(!empty.valid());
    MANUAL_REQUIRE(!world.load(bad.data(), bad.size(), tree, npc, geometry, error));
    MANUAL_REQUIRE(world.valid() && world.ray(kept_ray->id) == kept_ray);
  };
  for (size_t at : {size_t(8), size_t(24), size_t(28), size_t(32), size_t(36)}) {
    auto bad = world_bytes; word(bad, at, UINT32_MAX); reject_world(bad);
  }
  // Payload: six bodies, native body identity and zero-cast/nonfinite schema.
  for (size_t at : {size_t(136), size_t(148), size_t(128)}) {
    auto bad = world_bytes; word(bad, at, UINT32_MAX); world_crc(bad); reject_world(bad);
  }
}
