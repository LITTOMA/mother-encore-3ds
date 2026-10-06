#include "encore/player_visual_scripts.hpp"
#include "manual_require.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
                 "usage: player_visual_scripts_manual --reader|--expect-reject "
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
  PlayerVisualScriptsData data;
  bool accepted = data.load_file(argv[3], player, e);
  if (!std::strcmp(argv[1], "--expect-reject")) {
    MANUAL_REQUIRE(!accepted);
    MANUAL_REQUIRE(!data.valid());
    return 0;
  }
  MANUAL_REQUIRE(accepted);
  MANUAL_REQUIRE(data.valid());
  MANUAL_REQUIRE(data.shadow().animations.size() == 3);
  size_t count = 0;
  for (const auto &a : data.shadow().animations)
    count += a.frames.size();
  MANUAL_REQUIRE(count == 7);
  // These calls have no source-bound native owners: they must not grant Ready.
  PlayerVisualScriptsRuntime unavailable;
  MANUAL_REQUIRE(!unavailable.ready(1, e));
  MANUAL_REQUIRE(!unavailable.process(1, e));
  MANUAL_REQUIRE(
      !unavailable.apply_shadow_export(1, data.shadow().start_member, e));
  std::printf("Player visual source reader admitted; live "
              "Sprite/Fetcher/Player lifecycle not exercised.\n");
  return 0;
}
