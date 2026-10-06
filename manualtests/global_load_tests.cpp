#include "../tests/manual_require.hpp"
#include "encore/crc32.hpp"
#include "encore/global_load.hpp"
#include <algorithm>
namespace encore::upstream {
namespace {
uint32_t word(const std::vector<uint8_t> &b, size_t at) {
  uint32_t v = 0;
  for (size_t i = 0; i < 4; ++i)
    v |= uint32_t(b.at(at + i)) << (8 * i);
  return v;
}
void put(std::vector<uint8_t> &b, size_t at, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(at + i) = uint8_t(v >> (8 * i));
}
void seal(std::vector<uint8_t> &b) {
  put(b, 20, crc32(b.data() + 128, b.size() - 128));
}
} // namespace
// These explicit manual cases are compiled but never invoked automatically.
void global_load_format_manual(const std::vector<uint8_t> &pack,
                               const GlobalDataConstructorData &constructor,
                               const FieldCharacterLoadData &characters,
                               const FieldGlobalFlagsData &flags,
                               const FieldItemDefinitions &definitions) {
  MANUAL_REQUIRE(pack.size() > 256);
  std::string error;
  GlobalLoadData data;
  MANUAL_REQUIRE(data.load(pack.data(), pack.size(), constructor, characters,
                           flags, definitions, error));
  MANUAL_REQUIRE(!data.new_game_admitted() && !data.goto_game_admitted());
  MANUAL_REQUIRE(data.documents().size() == 3);
  auto original = data.ir_sha256();
  auto reject = [&](const std::vector<uint8_t> &bad) {
    MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), constructor, characters,
                              flags, definitions, error));
    MANUAL_REQUIRE(data.valid() && data.ir_sha256() == original &&
                   !error.empty());
  };
  for (size_t at :
       {size_t(8), size_t(12), size_t(24), size_t(28), size_t(32)}) {
    auto bad = pack;
    put(bad, at, word(bad, at) + 1);
    reject(bad);
  }
  auto bad = pack;
  bad.at(40) ^= 1;
  reject(bad);
  bad = pack;
  bad.at(60) ^= 1;
  reject(bad);
  bad = pack;
  bad.at(124) = 1;
  reject(bad);
  bad = pack;
  bad.at(bad.size() - 1) ^= 1;
  reject(bad);
  bad = pack;
  bad.pop_back();
  reject(bad);
  bad = pack;
  bad.push_back(0);
  put(bad, 16, uint32_t(bad.size()));
  seal(bad);
  reject(bad);
  // Eight source string fields precede independent constructor/characters/flags
  // proofs. A valid CRC must not allow switching any actual owning resource.
  size_t proof = 128;
  for (size_t i = 0; i < 8; ++i) {
    proof += 4 + word(pack, proof);
    MANUAL_REQUIRE(proof < pack.size());
  }
  for (size_t i = 0; i < 4; ++i) {
    bad = pack;
    bad.at(proof + 32 * i) ^= 1;
    seal(bad);
    reject(bad);
  }
  bad = pack;
  bad.at(132) = 0xff;
  seal(bad);
  reject(bad);
  // Original UTF-8 document bytes must remain bound to their independently
  // checked source hash even after a caller repairs transport CRC.
  for (const auto &document : data.documents()) {
    MANUAL_REQUIRE(!document.file.bytes.empty());
    bad = pack;
    auto first =
        std::search(bad.begin() + 128, bad.end(), document.file.bytes.begin(),
                    document.file.bytes.end());
    MANUAL_REQUIRE(first != bad.end());
    *first ^= 1;
    seal(bad);
    reject(bad);
  }
  FieldItemDefinitions limited;
  GlobalDataConstructorData absent;
  MANUAL_REQUIRE(!data.load(pack.data(), pack.size(), constructor, characters,
                            flags, limited, error));
  MANUAL_REQUIRE(!data.load(pack.data(), pack.size(), absent, characters, flags,
                            definitions, error));
  MANUAL_REQUIRE(!data.load(nullptr, pack.size(), constructor, characters,
                            flags, definitions, error));
  MANUAL_REQUIRE(data.ir_sha256() == original);
}
} // namespace encore::upstream
