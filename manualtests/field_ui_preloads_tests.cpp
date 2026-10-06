#include "encore/field_ui_preloads.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
static uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
static void put(std::vector<uint8_t> &b, size_t at, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    b[at + i] = uint8_t(v >> (8 * i));
}
static void crc(std::vector<uint8_t> &b) {
  uint32_t c = ~0u;
  for (size_t p = 128; p < b.size(); ++p) {
    c ^= b[p];
    for (unsigned i = 0; i < 8; ++i)
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0u);
  }
  put(b, 20, ~c);
}
static size_t first_graph(const std::vector<uint8_t> &b,
                          size_t *recipe_at = nullptr,
                          uint32_t *recipe_bytes = nullptr) {
  size_t p = 128;
  auto u = [&]() {
    assert(p + 4 <= b.size());
    auto v = word(b.data() + p);
    p += 4;
    return v;
  };
  auto t = [&]() {
    auto n = u();
    assert(n <= b.size() - p);
    p += n;
  };
  t();
  auto count = u();
  for (uint32_t i = 0; i < count; ++i) {
    t();
    p += 32;
  }
  assert(u() == 12);
  u();
  assert(u() == 0);
  u();
  u();
  t();
  t();
  t();
  p += 64;
  auto recipe = u();
  if (recipe_at)
    *recipe_at = p;
  if (recipe_bytes)
    *recipe_bytes = recipe;
  assert(recipe <= b.size() - p);
  p += recipe;
  auto graph = u();
  assert(graph && graph <= b.size() - p);
  return p;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
  assert(b.size() > 128);
  FieldIdentity id;
  id.scene_id = word(b.data() + 36);
  std::copy_n(b.data() + 40, 20, id.upstream_commit.begin());
  std::copy_n(b.data() + 60, 32, id.source_sha256.begin());
  FieldUiPreloadsData data;
  std::string e;
  assert(data.load(b.data(), b.size(), id, e));
  assert(data.entries().size() == 12);
  size_t recipe_start = 0;
  uint32_t recipe_size = 0;
  auto graph = first_graph(b, &recipe_start, &recipe_size);
  auto cap_bad = b;
  put(cap_bad, recipe_start + 28, 4);
  crc(cap_bad);
  assert(!data.load(cap_bad.data(), cap_bad.size(), id, e));
  auto graph_bad = b;
  graph_bad[graph] = 255;
  crc(graph_bad);
  assert(!data.load(graph_bad.data(), graph_bad.size(), id, e));
  std::string unknown = "WorldEnvironment";
  auto found =
      std::search(b.begin() + graph, b.end(), unknown.begin(), unknown.end());
  assert(found != b.end());
  graph_bad = b;
  graph_bad[size_t(found - b.begin())] ^= 0x20;
  crc(graph_bad);
  assert(!data.load(graph_bad.data(), graph_bad.size(), id, e));
  for (size_t at :
       {size_t(8), size_t(24), size_t(28), size_t(32), size_t(124)}) {
    auto bad = b;
    put(bad, at, 99);
    assert(!data.load(bad.data(), bad.size(), id, e));
  }
  auto wrong = id;
  wrong.scene_id ^= 1;
  assert(!data.load(b.data(), b.size(), wrong, e));
  auto bad = b;
  bad.push_back(0);
  put(bad, 16, uint32_t(bad.size()));
  crc(bad);
  assert(!data.load(bad.data(), bad.size(), id, e));
  for (size_t n = 0; n < 128; ++n)
    assert(!data.load(b.data(), n, id, e));
  assert(data.valid());
}
