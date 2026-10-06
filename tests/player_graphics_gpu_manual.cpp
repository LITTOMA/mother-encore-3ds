#include "manual_require.hpp"
#include "podunk_player_graphics.hpp"
// Explicit manual GPU assertion endpoint. The caller initializes real C3D/C2D
// and loads the checked production data; no Node Ready or texture receipt
// stands in for actual GPU allocations. This source is compiled, not executed
// by CI.
bool player_graphics_gpu_manual(
    const encore::upstream::PlayerGraphicsData &data, const char *root) {
  std::string e;
  encore::ctr::PodunkPlayerGraphics graphics;
  C2D_Image out{};
  for (const auto &a : data.assets())
    MANUAL_REQUIRE(!graphics.image(a.source, a.source_sha, out, e));
  MANUAL_REQUIRE(graphics.load(data, root, e));
  MANUAL_REQUIRE(graphics.loaded());
  for (const auto &a : data.assets()) {
    MANUAL_REQUIRE(graphics.image(a.source, a.source_sha, out, e));
    MANUAL_REQUIRE(out.tex && out.tex->data && out.subtex);
    MANUAL_REQUIRE(out.subtex->width == a.width &&
                   out.subtex->height == a.height);
    auto wrong = a.source_sha;
    wrong[0] ^= 1;
    MANUAL_REQUIRE(!graphics.image(a.source, wrong, out, e));
  }
  graphics.free();
  MANUAL_REQUIRE(!graphics.loaded());
  for (const auto &a : data.assets())
    MANUAL_REQUIRE(!graphics.image(a.source, a.source_sha, out, e));
  return true;
}

using namespace encore::upstream;
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
  if (argc != 7 || (std::strcmp(argv[1], "--gpu") &&
                    std::strcmp(argv[1], "--expect-gpu-reject"))) {
    std::fprintf(stderr,
                 "usage: player_graphics_gpu_manual --gpu|--expect-gpu-reject "
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
  PlayerGraphicsData data;
  bool accepted = data.load_file(argv[3], visual, e);
  MANUAL_REQUIRE(accepted);
  gfxInitDefault();
  MANUAL_REQUIRE(C3D_Init(C3D_DEFAULT_CMDBUF_SIZE));
  MANUAL_REQUIRE(C2D_Init(C2D_DEFAULT_MAX_OBJECTS));
  C2D_Prepare();
  {
    std::string prefix = root + "/";
    if (!std::strcmp(argv[1], "--expect-gpu-reject")) {
      encore::ctr::PodunkPlayerGraphics graphics;
      MANUAL_REQUIRE(!graphics.load(data, prefix.c_str(), e));
      MANUAL_REQUIRE(!graphics.loaded());
    } else
      MANUAL_REQUIRE(player_graphics_gpu_manual(data, prefix.c_str()));
  }
  C2D_Fini();
  C3D_Fini();
  gfxExit();
  std::printf("Explicit manual real GPU texture admission completed; no Node "
              "Ready granted.\n");
  return 0;
}
