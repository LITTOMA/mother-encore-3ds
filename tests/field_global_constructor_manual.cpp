#include "encore/field_global_constructor.hpp"
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
  if (argc != 6 || (std::strcmp(argv[1], "--reader") &&
                    std::strcmp(argv[1], "--expect-reject"))) {
    std::fprintf(stderr,
                 "usage: field_global_constructor_manual "
                 "--reader|--expect-reject PACK SCENE_ID PIN SCRIPT_SHA\n");
    return 2;
  }
  FieldIdentity i;
  i.scene_id = uint32_t(std::strtoul(argv[3], nullptr, 0));
  MANUAL_REQUIRE(hex(argv[4], i.upstream_commit));
  MANUAL_REQUIRE(hex(argv[5], i.source_sha256));
  FieldGlobalConstructorData d;
  std::string e;
  bool accepted = d.load_file(argv[2], i, e);
  if (!std::strcmp(argv[1], "--expect-reject")) {
    MANUAL_REQUIRE(!accepted);
    MANUAL_REQUIRE(!d.valid());
    return 0;
  }
  MANUAL_REQUIRE(accepted);
  MANUAL_REQUIRE(d.valid());
  MANUAL_REQUIRE(d.recipe().records().size() == 4);
  MANUAL_REQUIRE(d.transition_node().name.empty());
  MANUAL_REQUIRE(d.transition_node().index == -1);
  MANUAL_REQUIRE(
      d.member(FieldGlobalMemberRole::SceneTransition)->string_value ==
      d.transition_node().script);
  MANUAL_REQUIRE(d.member(FieldGlobalMemberRole::Party)->strings.empty());
  std::printf("Reader accepted source constructor only; native lifecycle/Ready "
              "were not executed.\n");
  return 0;
}
