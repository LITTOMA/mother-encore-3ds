#include "encore/house_return_sources.hpp"
#include "manual_require.hpp"
#include <cstdlib>
using namespace encore::upstream;
namespace {
bool hex(const std::string &s, uint8_t *out, size_t n) {
  if (s.size() != 2 * n) return false;
  for (size_t i = 0; i < n; ++i) {
    unsigned value = 0;
    for (unsigned j = 0; j < 2; ++j) {
      const char c = s[2 * i + j];
      const unsigned digit = c >= '0' && c <= '9' ? unsigned(c - '0') :
                             c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10) : 99;
      if (digit > 15) return false;
      value = 16 * value + digit;
    }
    out[i] = uint8_t(value);
  }
  return true;
}
}
// Standalone and intentionally not run by default. Optional negative fixture
// pairs must be complete current bundles with individually valid typed packs
// but mismatched cross-bindings: native ID/path/class/local, geometry script
// SHA/parent/local/world, target identity, or TileSet source hash. Their complete
// Include Sprite missing receivers, conflicting kinds/script hashes and a
// Fetcher bound to a non-Sprite native target in the cross-binding fixtures.
// file SHA/header/CRC bindings must be resealed by fixture authoring first;
// otherwise they exercise file admission rather than the intended cross-check.
int main(int argc, char **argv) {
  if (argc < 8 || (argc - 8) % 2) {
    std::cerr << "bundle romfs room house scene-id pin source-sha [bad-bundle bad-romfs]...\n";
    return 2;
  }
  FieldIdentity identity;
  identity.scene_id = uint32_t(std::strtoul(argv[5], nullptr, 10));
  MANUAL_REQUIRE(hex(argv[6], identity.upstream_commit.data(), 20));
  MANUAL_REQUIRE(hex(argv[7], identity.source_sha256.data(), 32));
  std::string error;
  PodunkBundleData bundle;
  MANUAL_REQUIRE(bundle.load_file(argv[1], identity, error));
  FieldDoorData doors;
  const auto *door = bundle.entry(PodunkPackRole::Door);
  MANUAL_REQUIRE(door);
  std::vector<uint8_t> bytes;
  MANUAL_REQUIRE(bundle.read(PodunkPackRole::Door, argv[2], bytes, error));
  MANUAL_REQUIRE(doors.load(bytes.data(), bytes.size(), door->identity, error));
  RoomData room;
  HouseData house;
  MANUAL_REQUIRE(room.load_file(argv[3], error));
  MANUAL_REQUIRE(house.load_file(argv[4], error));
  HouseReturnSources owner;
  MANUAL_REQUIRE(owner.load(bundle, argv[2], doors, room.view(), house.view(), error));
  MANUAL_REQUIRE(owner.valid() && !owner.geometry().scene_admitted() &&
                 !owner.tree().scene_admitted());
  MANUAL_REQUIRE(owner.reentry().tilemaps().size() == 3);
  MANUAL_REQUIRE(owner.npcs().valid() && owner.npcs().npcs().size() == 6 &&
                 owner.npc_world().valid() && owner.npc_world().bodies().size() == 6);
  MANUAL_REQUIRE(owner.timers().valid() && owner.timers().records().size() == 11 &&
                 owner.visibility().valid() && owner.visibility().records().size() == 12 &&
                 owner.sprites().valid() && owner.sprites().records().size() == 16);
  const auto root_id = owner.tree().identity().scene_id;
  const auto *root = owner.tree().record(root_id);
  MANUAL_REQUIRE(root && root->id == owner.reentry().native_nodes().front().id);
  MANUAL_REQUIRE(!owner.tilemap_node(0));
  for (const auto &certificate : owner.reentry().tilemaps()) {
    const auto *node = owner.tilemap_node(certificate.id);
    MANUAL_REQUIRE(node && node->path == certificate.node &&
                   owner.tree().classes()[node->class_index] == "TileMap");
  }
  auto rejected = [&](const PodunkBundleData &candidate, const std::string &romfs,
                      const FieldDoorData &actual_door, RoomView actual_room,
                      HouseView actual_house) {
    HouseReturnSources empty;
    MANUAL_REQUIRE(!empty.load(candidate, romfs, actual_door, actual_room, actual_house, error));
    MANUAL_REQUIRE(!empty.valid() && !empty.reentry().valid() &&
                   !empty.geometry().valid() && !empty.tree().valid() &&
                   !empty.npcs().valid() && !empty.npc_world().valid() &&
                   !empty.timers().valid() && !empty.visibility().valid() &&
                   !empty.sprites().valid());
    MANUAL_REQUIRE(!owner.load(candidate, romfs, actual_door, actual_room, actual_house, error));
    MANUAL_REQUIRE(owner.valid() && owner.tree().record(root_id) == root &&
                   owner.reentry().valid() && owner.geometry().valid() &&
                   owner.npcs().npcs().size() == 6 && owner.sprites().records().size() == 16);
  };
  PodunkBundleData empty_bundle;
  FieldDoorData empty_doors;
  rejected(empty_bundle, argv[2], doors, room.view(), house.view());
  rejected(bundle, argv[2], empty_doors, room.view(), house.view());
  rejected(bundle, argv[2], doors, {}, house.view());
  rejected(bundle, argv[2], doors, room.view(), {});
  rejected(bundle, std::string(argv[2]) + "/__manual_missing_house_resources__",
           doors, room.view(), house.view());
  for (int i = 8; i < argc; i += 2) {
    PodunkBundleData mismatch;
    MANUAL_REQUIRE(mismatch.load_file(argv[i], identity, error));
    rejected(mismatch, argv[i + 1], doors, room.view(), house.view());
  }
  std::cout << "House resource ownership and failure preservation checked; no lifecycle executed\n";
}
