#include "encore/field_native_timer.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
void put(std::vector<uint8_t> &b, size_t p, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    b[p + i] = uint8_t(v >> (8 * i));
}
void seal(std::vector<uint8_t> &b) {
  put(b, 16, crc(b.data() + 32, b.size() - 32));
}
} // namespace
// Manual-only source resource checks. No gameplay or hardware result is
// implied.
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
  if (b.size() < 56)
    return 2;
  std::string e;
  FieldNativeTimerData d;
  assert(d.load(b.data(), b.size(), e));
  assert(d.records().size() == 314);
  auto first = d.records().front();
  assert(d.record(first.identity, first.id));
  auto foreign = first.identity;
  foreign.source_sha256[0] ^= 1;
  assert(!d.record(foreign, first.id));
  for (auto patch :
       std::vector<std::pair<size_t, uint32_t>>{{8, 2},
                                                {20, 0},
                                                {24, 2},
                                                {28, 2},
                                                {52, 8193},
                                                {56 + 8, 2},
                                                {56 + 12, 4},
                                                {56 + 16, 0},
                                                {56 + 16, 0x7fc00000}}) {
    auto n = b;
    put(n, patch.first, patch.second);
    seal(n);
    assert(!d.load(n.data(), n.size(), e));
    assert(d.records().size() == 314);
  }
  auto bad = b;
  bad.back() ^= 1;
  assert(!d.load(bad.data(), bad.size(), e));
  assert(!d.load(b.data(), b.size() - 1, e));
  assert(!d.load(nullptr, b.size(), e));
  FieldNativeTimers runtime;
  unsigned count = 0;
  assert(runtime.initialize(
      d,
      [&](FieldObjectId, std::string &) {
        ++count;
        return true;
      },
      [](FieldObjectId) -> FieldNodeTreeRuntime * { return nullptr; }, e));
  assert(!runtime.start(1, -1, e));
  assert(!runtime.process(1, FieldTreePhase::ReadyNative, 0, false, e));
  assert(count == 0);
}
