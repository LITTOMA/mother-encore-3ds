#include "encore/field_global_flags.hpp"
#include "encore/house_status_effects.hpp"
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
    char b[3] = {s[i * 2], s[i * 2 + 1], 0};
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
  if (argc != 9 || (std::strcmp(argv[1], "--reader") &&
                    std::strcmp(argv[1], "--expect-reject"))) {
    std::fprintf(stderr,
                 "usage: house_status_effects_manual --reader|--expect-reject "
                 "ROOT PACK GLOBALDATA_ID PIN GLOBALDATA_SHA "
                 "CHARACTER_ID CHARACTER_SHA\n");
    return 2;
  }
  std::string root = argv[2], e;
  FieldIdentity gd, character;
  gd.scene_id = uint32_t(std::strtoul(argv[4], nullptr, 0));
  character.scene_id = uint32_t(std::strtoul(argv[7], nullptr, 0));
  MANUAL_REQUIRE(hex(argv[5], gd.upstream_commit));
  character.upstream_commit = gd.upstream_commit;
  MANUAL_REQUIRE(hex(argv[6], gd.source_sha256));
  MANUAL_REQUIRE(hex(argv[8], character.source_sha256));
  FieldGlobalDataData legacy;
  MANUAL_REQUIRE(
      legacy.load_file((root + "/data/global.encdata").c_str(), gd, e));
  FieldGlobalExternalSpec spec;
  spec.role = 3;
  spec.identity = gd;
  spec.script = legacy.owner_source();
  spec.script_sha = gd.source_sha256;
  GlobalYamlCachesData caches;
  MANUAL_REQUIRE(
      caches.load_file((root + "/data/global.encyamlcaches").c_str(), spec, e));
  GlobalDataConstructorData constructor;
  MANUAL_REQUIRE(constructor.load_file(
      (root + "/data/global.encconstructor").c_str(), legacy, caches, e));
  FieldCharacterLoadData characters;
  MANUAL_REQUIRE(characters.load_file(
      (root + "/data/global.enccharacterload").c_str(), character, e));
  FieldGlobalFlagsData flags;
  MANUAL_REQUIRE(flags.load_file((root + "/data/global.encflags").c_str(), e));
  FieldItemDefinitions definitions;
  MANUAL_REQUIRE(
      definitions.load_file((root + "/data/global.encfielditems").c_str(), e));
  GlobalLoadData cold;
  MANUAL_REQUIRE(cold.load_file((root + "/data/global.encload").c_str(),
                                constructor, characters, flags, definitions,
                                e));
  NativeSessionData session;
  MANUAL_REQUIRE(
      session.load_file((root + "/data/opening.encsession").c_str(), e));
  HouseGlobalBridgeData bridge;
  MANUAL_REQUIRE(
      bridge.load_file((root + "/data/house.encglobalbridge").c_str(),
                       characters, cold, session, e));
  PlayerInitializationData init;
  MANUAL_REQUIRE(init.load_file(
      (root + "/data/player.encinitialization").c_str(), constructor, e));
  PlayerReadyData ready;
  MANUAL_REQUIRE(
      ready.load_file((root + "/data/player.encready").c_str(), init, e));
  HouseStatusEffectsData data;
  bool accepted = data.load_file(argv[3], bridge, ready, e);
  if (!std::strcmp(argv[1], "--expect-reject")) {
    MANUAL_REQUIRE(!accepted);
    MANUAL_REQUIRE(!data.valid());
    return 0;
  }
  if (!accepted) {
    std::fprintf(stderr, "%s\n", e.c_str());
    return 1;
  }
  MANUAL_REQUIRE(data.valid());
  MANUAL_REQUIRE(data.bridge() == &bridge && data.ready() == &ready);
  std::printf(
      "Status effects resource format admitted; no source getters, UID, "
      "RNG, source LOAD or Node Ready executed.\n");
  return 0;
}
