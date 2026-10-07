#include "encore/scene_clip_native.hpp"
#include "manual_require.hpp"
#include <cstdio>
#include <cstring>
// Manual resource-reader driver. No scene, callback, RNG or Ready is
// fabricated. Arguments: native-pack tree openable present emotes bush
// [--expect-reject].
int main(int argc, char **argv) {
  using namespace encore::upstream;
  if (argc != 7 && argc != 8)
    return 2;
  bool reject = argc == 8;
  if (reject && std::strcmp(argv[7], "--expect-reject"))
    return 2;
  // Derive the dependency identity from the separately supplied actual tree,
  // rather than trusting a corrupt candidate's header.
  FILE *f = std::fopen(argv[2], "rb");
  std::array<uint8_t, 128> h{};
  MANUAL_REQUIRE(f && std::fread(h.data(), 1, h.size(), f) == h.size());
  MANUAL_REQUIRE(std::fclose(f) == 0);
  FieldIdentity id;
  id.scene_id = uint32_t(h[36]) | uint32_t(h[37]) << 8 | uint32_t(h[38]) << 16 |
                uint32_t(h[39]) << 24;
  std::memcpy(id.upstream_commit.data(), h.data() + 40, 20);
  std::memcpy(id.source_sha256.data(), h.data() + 60, 32);
  FieldNodeTreeData t;
  FieldOpenableDoorData o;
  FieldPresentData p;
  FieldEmoteData em;
  FieldBushData b;
  SceneClipNativeData native;
  std::string e;
  MANUAL_REQUIRE(t.load_file(argv[2], id, e));
  MANUAL_REQUIRE(o.load_file(argv[3], id, e));
  MANUAL_REQUIRE(p.load_file(argv[4], e));
  MANUAL_REQUIRE(em.load_file(argv[5], e));
  MANUAL_REQUIRE(b.load_file(argv[6], e));
  const bool accepted = native.load_file(argv[1], t, o, p, em, b, e);
  MANUAL_REQUIRE(accepted != reject);
  if (reject) {
    MANUAL_REQUIRE(!native.valid() && !e.empty());
    return 0;
  }
  MANUAL_REQUIRE(native.records().size() ==
                 o.records().size() + p.bindings().size() +
                     em.records().size() + b.records().size());
  for (const auto &r : native.records())
    MANUAL_REQUIRE(t.record(r.id) && native.owner(r.kind, r.owner) == &r);
  return 0;
}
