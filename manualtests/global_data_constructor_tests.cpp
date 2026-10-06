#include "../tests/manual_require.hpp"
#include "encore/crc32.hpp"
#include "encore/global_data_constructor.hpp"
namespace encore::upstream {
namespace {
void put(std::vector<uint8_t> &b, size_t at, uint32_t x) {
  for (size_t i = 0; i < 4; ++i)
    b.at(at + i) = uint8_t(x >> (i * 8));
}
uint32_t get(const std::vector<uint8_t> &b, size_t at) {
  uint32_t x = 0;
  for (size_t i = 0; i < 4; ++i)
    x |= uint32_t(b.at(at + i)) << (i * 8);
  return x;
}
void seal(std::vector<uint8_t> &b) {
  put(b, 20, crc32(b.data() + 128, b.size() - 128));
}
} // namespace
// Explicit manual format cases. They remain active under NDEBUG and are not
// executed by automatic builds or by production source conversion.
void global_data_constructor_format_manual(const std::vector<uint8_t> &source,
                                           const FieldGlobalDataData &legacy,
                                           const GlobalYamlCachesData &cache) {
  MANUAL_REQUIRE(source.size() > 256);
  std::string error;
  GlobalDataConstructorData data;
  MANUAL_REQUIRE(data.load(source.data(), source.size(), legacy, cache, error));
  auto original = data.ir_sha256();
  auto rejected = [&](const std::vector<uint8_t> &bad) {
    MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), legacy, cache, error));
    MANUAL_REQUIRE(!error.empty());
    MANUAL_REQUIRE(data.valid() && data.ir_sha256() == original);
  };
  for (size_t at :
       {size_t(8), size_t(12), size_t(24), size_t(28), size_t(32)}) {
    auto bad = source;
    put(bad, at, get(bad, at) + 1);
    rejected(bad);
  }
  auto bad = source;
  bad.at(40) ^= 1;
  rejected(bad);
  bad = source;
  bad.at(60) ^= 1;
  rejected(bad);
  bad = source;
  bad.at(124) = 1;
  rejected(bad);
  bad = source;
  bad.at(bad.size() - 1) ^= 1;
  rejected(bad);
  bad = source;
  bad.pop_back();
  rejected(bad);
  bad = source;
  bad.push_back(0);
  put(bad, 16, uint32_t(bad.size()));
  seal(bad);
  rejected(bad);
  // Independent real dependencies must match even with a valid resource CRC.
  size_t legacy_offset = 132 + get(source, 128);
  bad = source;
  bad.at(legacy_offset) ^= 1;
  seal(bad);
  rejected(bad);
  bad = source;
  bad.at(legacy_offset + 32) ^= 1;
  seal(bad);
  rejected(bad);
  // No original declaration can be interpreted from a malformed UTF-8 owner.
  bad = source;
  bad.at(132) = 0xff;
  seal(bad);
  rejected(bad);
  FieldGlobalDataData absent;
  GlobalYamlCachesData absent_cache;
  MANUAL_REQUIRE(
      !data.load(source.data(), source.size(), absent, cache, error));
  MANUAL_REQUIRE(
      !data.load(source.data(), source.size(), legacy, absent_cache, error));
  MANUAL_REQUIRE(!data.load(nullptr, source.size(), legacy, cache, error));
  MANUAL_REQUIRE(!data.load(source.data(), 127, legacy, cache, error));
  MANUAL_REQUIRE(data.ir_sha256() == original);
}
} // namespace encore::upstream
