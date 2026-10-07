#include "encore/player_resources.hpp"
#include "manual_require.hpp"
#include "platform/ctr/podunk_player_resources.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
using namespace encore::upstream;
namespace {
template <size_t N> bool hex(const char *s, std::array<uint8_t, N> &out) {
  if (std::strlen(s) != N * 2)
    return false;
  for (size_t i = 0; i < N; ++i) {
    char b[3] = {s[2 * i], s[2 * i + 1], 0};
    char *end = nullptr;
    auto v = std::strtoul(b, &end, 16);
    if (*end)
      return false;
    out[i] = uint8_t(v);
  }
  return true;
}
} // namespace
int main(int argc, char **argv) {
  if (argc != 7 || (std::strcmp(argv[1], "--reader") &&
                    std::strcmp(argv[1], "--expect-reject"))) {
    std::fprintf(stderr,
                 "usage: player_graphics_manual --reader|--expect-reject "
                 "ROOT PACK GLOBALDATA_ID PIN GLOBALDATA_SHA\n");
    return 2;
  }
  FieldIdentity i;
  i.scene_id = uint32_t(std::strtoul(argv[4], nullptr, 0));
  MANUAL_REQUIRE(hex(argv[5], i.upstream_commit));
  MANUAL_REQUIRE(hex(argv[6], i.source_sha256));
  std::string root = argv[2], e;
  FieldGlobalDataData legacy;
  MANUAL_REQUIRE(
      legacy.load_file((root + "/data/global.encdata").c_str(), i, e));
  FieldGlobalExternalSpec spec;
  spec.role = 3;
  spec.identity = i;
  spec.script = legacy.owner_source();
  spec.script_sha = i.source_sha256;
  GlobalYamlCachesData caches;
  MANUAL_REQUIRE(
      caches.load_file((root + "/data/global.encyamlcaches").c_str(), spec, e));
  GlobalDataConstructorData ctor;
  MANUAL_REQUIRE(ctor.load_file((root + "/data/global.encconstructor").c_str(),
                                legacy, caches, e));
  PlayerInitializationData player;
  MANUAL_REQUIRE(player.load_file(
      (root + "/data/player.encinitialization").c_str(), ctor, e));
  PlayerVisualScriptsData visual;
  MANUAL_REQUIRE(visual.load_file(
      (root + "/data/player.encvisualscripts").c_str(), player, e));
  PlayerGraphicsData graphics;
  MANUAL_REQUIRE(graphics.load_file((root + "/data/player.encgraphics").c_str(),
                                    visual, e));
  PlayerResourcesData data;
  bool accepted = data.load_file(argv[3], player, graphics, e);
  if(accepted && data.capability()==2) {
    PlayerEffectsData effects;
    accepted=effects.load_file((root+"/data/player.enceffects").c_str(),player,e) && data.bind_effects(effects,e);
  }
  if (!std::strcmp(argv[1], "--expect-reject")) {
    MANUAL_REQUIRE(!accepted);
    // A cross-binding failure preserves a valid reader object but cannot
    // obtain the independent effect factory capability.
    MANUAL_REQUIRE(!data.valid() || !data.effects_bound());
    return 0;
  }
  MANUAL_REQUIRE(accepted);
  std::ifstream file(argv[3], std::ios::binary | std::ios::ate);
  auto size = file.tellg();
  MANUAL_REQUIRE(size > 128);
  file.seekg(0);
  std::vector<uint8_t> raw(size_t(size), 0);
  MANUAL_REQUIRE(bool(file.read(reinterpret_cast<char *>(raw.data()), size)));
  for (size_t offset :
       {size_t(8), size_t(24), size_t(28), size_t(32), size_t(36)}) {
    auto changed = raw;
    changed[offset] ^= 0xff;
    PlayerResourcesData bad;
    MANUAL_REQUIRE(
        !bad.load(changed.data(), changed.size(), player, graphics, e));
    MANUAL_REQUIRE(!bad.valid());
  }
  auto trailing = raw;
  trailing.push_back(0);
  PlayerResourcesData bad;
  MANUAL_REQUIRE(
      !bad.load(trailing.data(), trailing.size(), player, graphics, e));
  encore::ctr::PodunkPlayerResources owner;
  FieldObjectId object = 123;
  MANUAL_REQUIRE(!owner.construct_resource(0, 0, object, e) && object == 123);
  MANUAL_REQUIRE(!owner.construct_audio(0, object, e) && object == 123);
  bool exists = true;
  MANUAL_REQUIRE(!owner.resource_exists("unknown", exists, e) && exists);
  std::printf("Manual Player resources reader/negative cases complete; no "
              "GPU/lifecycle executed.\n");
  return 0;
}
