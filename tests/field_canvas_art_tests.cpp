#include "encore/field_canvas_art.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
void u32(std::vector<uint8_t> &b, size_t p, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    b[p + i] = uint8_t(v >> (i * 8));
}
void seal(std::vector<uint8_t> &b) {
  uint32_t c = ~0u;
  for (size_t i = 128; i < b.size(); ++i) {
    c ^= b[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  u32(b, 20, ~c);
}
} // namespace
// Manual source cases only. This is never an automatic PR/push test.
int main() {
  std::ifstream f("romfs/data/podunk.enccanvasart", std::ios::binary);
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
  assert(b.size() >= 128);
  FieldIdentity identity;
  identity.scene_id = u32(b.data() + 36);
  std::copy_n(b.data() + 40, 20, identity.upstream_commit.begin());
  std::copy_n(b.data() + 60, 32, identity.source_sha256.begin());
  FieldCanvasArtData data;
  std::string error;
  assert(data.load(b.data(), b.size(), identity, error));
  assert(!data.scene_admitted());
  size_t at = 128;
  auto text = [&]() {
    auto size = u32(b.data() + at);
    at += 4 + size;
  };
  text();
  at += 12 + 32;
  auto textures = u32(b.data() + at);
  at += 4;
  auto asset = [&]() {
    at += 20;
    text();
    text();
    at += 64;
  };
  for (uint32_t i = 0; i < textures; ++i)
    asset();
  asset();
  at += 4;
  const size_t record = at;
  for (const auto &change :
       std::vector<std::pair<size_t, uint32_t>>{{0, 0},
                                                {4, 99},
                                                {16, 0},
                                                {20, 0},
                                                {24, UINT32_MAX},
                                                {28, 2},
                                                {40, 99},
                                                {44, 99},
                                                {52, 99}}) {
    auto bad = b;
    u32(bad, record + change.first, change.second);
    seal(bad);
    assert(!data.load(bad.data(), bad.size(), identity, error));
  }
  assert(data.load(b.data(), b.size(), identity, error));
  for (const auto &r : data.records()) {
    assert(r.id);
    if (r.texture)
      assert(data.texture(r.texture));
    if (r.owner != FieldCanvasOwner::Native)
      assert(r.owner_id && !r.owner_script.empty());
  }
  for (size_t offset :
       {size_t(8), size_t(24), size_t(28), size_t(32), size_t(124)}) {
    auto bad = b;
    u32(bad, offset, 999);
    assert(!data.load(bad.data(), bad.size(), identity, error));
    assert(!data.valid());
  }
  auto changed = identity;
  changed.scene_id ^= 1;
  assert(!data.load(b.data(), b.size(), changed, error));
  changed = identity;
  changed.source_sha256[0] ^= 1;
  assert(!data.load(b.data(), b.size(), changed, error));
  auto trailing = b;
  trailing.push_back(0);
  u32(trailing, 16, uint32_t(trailing.size()));
  seal(trailing);
  assert(!data.load(trailing.data(), trailing.size(), identity, error));
  for (size_t size : {size_t(0), size_t(127), b.size() - 1})
    assert(!data.load(b.data(), size, identity, error));
  assert(data.load(b.data(), b.size(), identity, error));
  FieldNodeTreeData tree;
  assert(tree.load_file("romfs/data/podunk.encnodetree", identity, error));
  FieldNodeTreeRuntime uninitialized;
  FieldCanvasArtRuntime runtime;
  assert(!runtime.initialize(data, tree, uninitialized, {}, error));
  std::vector<FieldCanvasDraw> commands;
  assert(!runtime.collect(commands, error));
  return 0;
}
