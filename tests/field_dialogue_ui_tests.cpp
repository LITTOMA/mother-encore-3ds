// Manual-only parser/provenance cases; deliberately not registered or executed.
#include "encore/field_dialogue_ui.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
std::vector<uint8_t> read(const char *path) {
  std::ifstream f(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
uint32_t u(const std::vector<uint8_t> &b, size_t i) {
  return uint32_t(b[i]) | (uint32_t(b[i + 1]) << 8) |
         (uint32_t(b[i + 2]) << 16) | (uint32_t(b[i + 3]) << 24);
}
void crc(std::vector<uint8_t> &b) {
  uint32_t c = ~0u;
  for (size_t i = 128; i < b.size(); ++i) {
    c ^= b[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int32_t(c & 1)));
  }
  c = ~c;
  for (unsigned i = 0; i < 4; ++i)
    b[20 + i] = uint8_t(c >> (i * 8));
}
} // namespace
int main() {
  auto bytes = read("romfs/data/podunk-dialogue-ui.encdui");
  assert(bytes.size() > 128);
  FieldIdentity expected;
  expected.scene_id = u(bytes, 36);
  std::copy_n(bytes.data() + 40, 20, expected.upstream_commit.begin());
  std::copy_n(bytes.data() + 60, 32, expected.source_sha256.begin());
  FieldNodeRecipeData recipe;
  std::string error;
  assert(
      recipe.load_file("romfs/data/dialogue.encnoderecipe", expected, error));
  FieldDialogueUiData data;
  assert(data.load(bytes.data(), bytes.size(), recipe.identity(), error));
  assert(data.nodes().size() == 47 && data.controls().size() == 17 &&
         data.animations().size() == 6 && !data.scene_admitted());
  size_t supported = 0;
  for (const auto &n : data.nodes())
    supported += n.kind != FieldDialogueUiKind::Pending;
  assert(supported == 20);
  for (size_t offset : {8u, 24u, 28u, 32u, 124u}) {
    auto bad = bytes;
    bad[offset] ^= 1;
    crc(bad);
    assert(!data.load(bad.data(), bad.size(), expected, error) && data.valid());
  }
  auto other = expected;
  other.source_sha256[0] ^= 1;
  assert(!data.load(bytes.data(), bytes.size(), other, error));
  auto bad = bytes;
  bad.back() ^= 1;
  assert(!data.load(bad.data(), bad.size(), expected, error));
  assert(!data.load(nullptr, bytes.size(), expected, error));
  assert(!data.load(bytes.data(), bytes.size() - 1, expected, error));
  bad = bytes;
  bad.push_back(0);
  assert(!data.load(bad.data(), bad.size(), expected, error));
  // Repair CRC after changing first CanvasLayer's declared native kind to
  // Control.
  size_t at = 128 + 64 + 32;
  for (unsigned j = 0; j < 7; ++j)
    at += 4 + u(bytes, at);
  at += 4;
  bad = bytes;
  bad[at + 16] = uint8_t(FieldDialogueUiKind::Control);
  crc(bad);
  assert(!data.load(bad.data(), bad.size(), expected, error));
  FieldDialogueUiRuntime runtime;
  WorldDialoguePose pose;
  assert(!runtime.pose(1, pose, error));
  assert(!runtime.ready_native(1, error));
  assert(!runtime.play(1, "unknown", error));
  assert(!runtime.animation_process(1, -1, error));
  assert(!runtime.sort_children(1, error));
  FieldDialogueStep step;
  step.op = FieldDialogueOp::InputRelease;
  assert(!runtime.native_step(step, 1, error)); // No fake Ready/draw owner.
}
