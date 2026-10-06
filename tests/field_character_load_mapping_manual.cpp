#include "encore/crc32.hpp"
#include "encore/field_character_load.hpp"
#include "manual_require.hpp"
#include <cerrno>
#include <fstream>
using namespace encore::upstream;
namespace {
template <size_t N>
bool hex(const std::string &s, std::array<uint8_t, N> &out) {
  if (s.size() != N * 2)
    return false;
  auto digit = [](char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  };
  for (size_t n = 0; n < N; ++n) {
    auto a = digit(s[n * 2]), b = digit(s[n * 2 + 1]);
    if (a < 0 || b < 0)
      return false;
    out[n] = uint8_t(a * 16 + b);
  }
  return true;
}
uint32_t word(const std::vector<uint8_t> &b, size_t at) {
  MANUAL_REQUIRE(at <= b.size() && b.size() - at >= 4);
  return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 |
         uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}
void put(std::vector<uint8_t> &b, size_t at, uint32_t x) {
  MANUAL_REQUIRE(at <= b.size() && b.size() - at >= 4);
  for (unsigned n = 0; n < 4; ++n)
    b[at + n] = uint8_t(x >> (n * 8));
}
void refresh(std::vector<uint8_t> &b) {
  MANUAL_REQUIRE(b.size() >= 128);
  put(b, 20, encore::crc32(b.data() + 128, b.size() - 128));
}
struct Targets {
  size_t default_kind = 0, step = 0;
};
Targets locate(const std::vector<uint8_t> &b) {
  size_t at = 128;
  Targets out;
  auto skip = [&](size_t n) {
    MANUAL_REQUIRE(at <= b.size() && n <= b.size() - at);
    at += n;
  };
  auto u = [&]() {
    auto x = word(b, at);
    at += 4;
    return x;
  };
  auto text = [&]() {
    auto x = u();
    skip(x);
  };
  // Fixed wire-schema counts only; all game member values remain external.
  for (unsigned n = 0; n < 35; ++n)
    text();
  skip(12);
  auto stats = u();
  while (stats--)
    text();
  for (unsigned group = 0; group < 7; ++group) {
    auto count = u();
    while (count--) {
      text();
      if (!out.default_kind)
        out.default_kind = at;
      u();
      text();
      skip(8);
    }
  }
  MANUAL_REQUIRE(u());
  out.step = at;
  return out;
}
void cases(const std::vector<uint8_t> &raw, const FieldIdentity &identity) {
  FieldCharacterLoadData data;
  std::string e;
  MANUAL_REQUIRE(data.load(raw.data(), raw.size(), identity, e));
  const auto saved = data.ir_sha256();
  const auto points = locate(raw);
  for (const auto at :
       {size_t(8), size_t(24), size_t(28), size_t(32), size_t(124)}) {
    auto bad = raw;
    put(bad, at, 99);
    MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), identity, e));
    MANUAL_REQUIRE(data.valid() && data.ir_sha256() == saved);
  }
  for (const auto at : {points.default_kind, points.step}) {
    auto bad = raw;
    put(bad, at, 99);
    refresh(bad);
    MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), identity, e));
  }
  auto bad = raw;
  put(bad, points.step, 2);
  put(bad, points.step + 4, 1);
  refresh(bad);
  MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), identity, e));
  bad = raw;
  bad.at(132) = '!';
  refresh(bad);
  MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), identity, e));
  bad = raw;
  put(bad, 20, word(bad, 20) ^ 1);
  MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), identity, e));
  MANUAL_REQUIRE(!data.load(raw.data(), raw.size() - 1, identity, e));
  auto wrong = identity;
  wrong.source_sha256[0] ^= 1;
  MANUAL_REQUIRE(!data.load(raw.data(), raw.size(), wrong, e));
  MANUAL_REQUIRE(data.valid() && data.ir_sha256() == saved);
  const auto &b = data.source_bindings();
  MANUAL_REQUIRE(!b.inventory_getter.empty() &&
                 !b.enemy_remaining_field.empty() &&
                 b.enemy_skill_defaults.size() == 4);
  for (const auto &row : data.rows())
    for (size_t n = 0; n < data.stats().size(); ++n) {
      int64_t value = 0;
      for (const auto &field : row.permanent_fields)
        if (field.first == data.stats()[n])
          value = field.second;
      MANUAL_REQUIRE(value == row.permanent[n]);
    }
}
} // namespace
int main(int argc, char **argv) {
  if (argc != 6 || (std::string(argv[1]) != "--reader" &&
                    std::string(argv[1]) != "--expect-reject")) {
    std::cerr
        << "Usage: field_character_load_mapping_manual MODE PACK SCENE_ID "
           "PIN_HEX SOURCE_SHA_HEX\nModes: --reader / --expect-reject. Only "
           "source parser/mapping cases; no LOAD or Ready lifecycle.\n";
    return 2;
  }
  FieldIdentity identity;
  char *end = nullptr;
  errno = 0;
  auto id = std::strtoull(argv[3], &end, 10);
  if (errno || !end || *end || !id || id > UINT32_MAX ||
      !hex(argv[4], identity.upstream_commit) ||
      !hex(argv[5], identity.source_sha256)) {
    std::cerr << "Invalid explicit source identity\n";
    return 2;
  }
  identity.scene_id = uint32_t(id);
  std::ifstream file(argv[2], std::ios::binary);
  if (!file) {
    std::cerr << "Unreadable explicit resource\n";
    return 2;
  }
  std::vector<uint8_t> raw((std::istreambuf_iterator<char>(file)), {});
  if (file.bad()) {
    std::cerr << "Resource read failed\n";
    return 2;
  }
  FieldCharacterLoadData data;
  std::string e;
  if (std::string(argv[1]) == "--expect-reject") {
    if (data.load(raw.data(), raw.size(), identity, e)) {
      std::cerr << "Invalid source resource unexpectedly accepted\n";
      return 1;
    }
    std::cout << "Actual loader rejected: " << e << '\n';
    return 0;
  }
  if (!data.load_file(argv[2], identity, e)) {
    std::cerr << e << '\n';
    return 1;
  }
  cases(raw, identity);
  std::cout << "Manual mapping/format cases completed; no constructor or "
               "gameplay test was run by the build.\n";
  return 0;
}
