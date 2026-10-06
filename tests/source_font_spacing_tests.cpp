#include "encore/crc32.hpp"
#include "encore/source_font.hpp"
#include <cassert>
#include <cstring>
#include <vector>
namespace {
void u(std::vector<uint8_t> &b, uint32_t v) {
  for (unsigned j = 0; j < 4; ++j)
    b.push_back(uint8_t(v >> (8 * j)));
}
void f(std::vector<uint8_t> &b, float value) {
  uint32_t v;
  std::memcpy(&v, &value, 4);
  u(b, v);
}
void s(std::vector<uint8_t> &b, const char *value) {
  u(b, uint32_t(std::strlen(value)));
  b.insert(b.end(), value, value + std::strlen(value));
}
void patch(std::vector<uint8_t> &b, size_t off, uint32_t value) {
  for (unsigned j = 0; j < 4; ++j)
    b.at(off + j) = uint8_t(value >> (8 * j));
}
std::vector<uint8_t> pack(uint32_t version) {
  std::vector<uint8_t> b(32);
  std::memcpy(b.data(), "ENCFONT\0", 8);
  patch(b, 8, version);
  patch(b, 20, 1);
  patch(b, 24, 1);
  patch(b, 28, 1);
  s(b, "Fonts/Fixture.tres");
  u(b, 0);
  u(b, 0);
  u(b, 1);
  u(b, 0);
  u(b, 1);
  f(b, 11);
  f(b, 0);
  f(b, 11);
  if (version == 2) {
    f(b, -1);
    f(b, 3);
  }
  s(b, "fixture.t3x");
  for (auto v : {256u, 256u, 131072u, 131072u, 1u})
    u(b, v);
  for (auto v : {48u, 0u, 1u, 1u, 1u, 1u})
    u(b, v);
  f(b, 7);
  f(b, 0);
  f(b, 0);
  patch(b, 12, uint32_t(b.size() - 32));
  patch(b, 16, encore::crc32(b.data() + 32, b.size() - 32));
  return b;
}
} // namespace
// Parser fixture only, manually invoked; not an upstream gameplay substitute.
int main() {
  std::string error;
  encore::SourceFontCatalog c;
  float width = 0;
  auto old = pack(1);
  assert(c.load_bytes(old.data(), old.size(), error));
  assert(c.select_font("Fonts/Fixture.tres", error));
  assert(c.measure("00", width, error) && width == 14);
  auto now = pack(2);
  assert(c.load_bytes(now.data(), now.size(), error));
  assert(c.select_font("Fonts/Fixture.tres", error));
  assert(c.measure("00", width, error) && width == 13);
  assert(c.measure("0", width, error) && width == 7);
  assert(c.measure("0\n0", width, error) && width == 7);
  assert(c.following_spacing(32, true) == 0);
  auto bad = now;
  patch(bad, 8, 99);
  assert(!c.load_bytes(bad.data(), bad.size(), error));
  bad = now;
  bad.push_back(0);
  assert(!c.load_bytes(bad.data(), bad.size(), error));
  bad = now;
  const size_t spacing = 32 + 4 + std::strlen("Fonts/Fixture.tres") + 20 + 12;
  patch(bad, spacing, 0x7fc00000);
  patch(bad, 16, encore::crc32(bad.data() + 32, bad.size() - 32));
  assert(!c.load_bytes(bad.data(), bad.size(), error));
  assert(c.measure("00", width, error) && width == 13);
}
