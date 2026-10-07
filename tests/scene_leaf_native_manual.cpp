#include "encore/scene_leaf_native.hpp"
#include "manual_require.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
int main(int argc, char **argv) {
  bool reject = argc == 10 && std::strcmp(argv[9], "--expect-reject") == 0;
  if (argc != 9 && !reject)
    return 2;
  using namespace encore::upstream;
  FieldIdentity id;
  FILE *f = std::fopen(argv[2], "rb");
  std::array<unsigned char, 128> h{};
  MANUAL_REQUIRE(f && std::fread(h.data(), 1, h.size(), f) == h.size());
  MANUAL_REQUIRE(std::fclose(f) == 0);
  id.scene_id = uint32_t(h[36]) | uint32_t(h[37]) << 8 | uint32_t(h[38]) << 16 |
                uint32_t(h[39]) << 24;
  std::memcpy(id.upstream_commit.data(), h.data() + 40, 20);
  std::memcpy(id.source_sha256.data(), h.data() + 60, 32);
  FieldNodeTreeData tree;
  FieldGameCameraData camera;
  FieldPlayerTransitionsData transitions;
  FieldBirdData birds;
  FieldDroppedData dropped;
  FieldPayphoneData phone;
  FieldMelodyBackgroundData melody;
  std::string e;
  MANUAL_REQUIRE(tree.load_file(argv[2], id, e));
  MANUAL_REQUIRE(camera.load_file(argv[3], id, e));
  MANUAL_REQUIRE(transitions.load_file(argv[4], e));
  MANUAL_REQUIRE(birds.load_file(argv[5], id, e));
  MANUAL_REQUIRE(dropped.load_file(argv[6], e));
  MANUAL_REQUIRE(phone.load_file(argv[7], e));
  MANUAL_REQUIRE(melody.load_file(argv[8], e));
  SceneLeafNativeSources sources{tree,    camera, transitions, birds,
                                 dropped, phone,  melody};
  SceneLeafNativeData data;
  bool accepted = data.load_file(argv[1], sources, e);
  if (reject) {
    MANUAL_REQUIRE(!accepted);
    MANUAL_REQUIRE(!data.valid());
    std::printf("Rejected: %s\n", e.c_str());
    return 0;
  }
  if (!accepted) {
    std::fprintf(stderr, "%s\n", e.c_str());
    return 1;
  }
  std::printf("Production typed SceneLeafNative format accepted %zu leaves\n",
              data.records().size());
  return 0;
}
