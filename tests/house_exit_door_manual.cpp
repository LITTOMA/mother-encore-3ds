#include "encore/field_door.hpp"
#include "manual_require.hpp"
#include <algorithm>
#include <iostream>
#include <string>
using namespace encore::upstream;
static bool hex(std::string s, uint8_t *out, size_t n) {
  if (s.size() != n * 2)
    return false;
  for (size_t i = 0; i < n; ++i) {
    unsigned v = 0;
    for (unsigned j = 0; j < 2; ++j) {
      char c = s[i * 2 + j];
      unsigned d = c >= '0' && c <= '9'   ? unsigned(c - '0')
                   : c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10)
                                          : 99;
      if (d > 15)
        return false;
      v = v * 16 + d;
    }
    out[i] = uint8_t(v);
  }
  return true;
}
int main(int argc, char **argv) {
  if (argc != 5 && argc != 6) {
    std::cerr << "resource scene-id pin source-sha [--expect-reject]\n";
    return 2;
  }
  FieldIdentity id;
  id.scene_id = uint32_t(std::strtoul(argv[2], nullptr, 10));
  MANUAL_REQUIRE(hex(argv[3], id.upstream_commit.data(), 20));
  MANUAL_REQUIRE(hex(argv[4], id.source_sha256.data(), 32));
  FieldDoorData data;
  std::string error;
  const bool loaded = data.load_file(argv[1], id, error);
  if (argc == 6) {
    MANUAL_REQUIRE(std::string(argv[5]) == "--expect-reject");
    MANUAL_REQUIRE(!loaded);
    MANUAL_REQUIRE(!data.valid());
    return 0;
  }
  MANUAL_REQUIRE(loaded);
  MANUAL_REQUIRE(data.door_count() == 1);
  const auto door = data.door(0);
  MANUAL_REQUIRE(!data.string(door.target_path).empty());
  MANUAL_REQUIRE(!data.scene_admitted());
  FieldDoorRuntime runtime;
  MANUAL_REQUIRE(!runtime.source_ready(door.id));
  MANUAL_REQUIRE(runtime.data() == nullptr);
  MANUAL_REQUIRE(!runtime.body_entered(door.id, 1, error));
  MANUAL_REQUIRE(runtime.phase() == FieldDoorPhase::Idle);
  std::cout << "Source Door reader admitted; lifecycle/target activation not "
               "executed\n";
  return 0;
}
