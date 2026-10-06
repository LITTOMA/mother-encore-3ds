#include "encore/global_yaml_file.hpp"
#include "manual_require.hpp"
#include <cstdio>
using namespace encore::upstream;
// Manual source only; no automatic CI registration or fixture scene.
void global_yaml_file_format_manual(const char *path,
                                    const GlobalYamlCachesData &actual_cache) {
  GlobalYamlFileData data;
  std::string error;
  MANUAL_REQUIRE(data.load_file(path, actual_cache, error));
  MANUAL_REQUIRE(data.records().size() == actual_cache.records().size());
  const auto admitted = data.ir_sha256();
  auto *f = std::fopen(path, "rb");
  MANUAL_REQUIRE(f);
  MANUAL_REQUIRE(std::fseek(f, 0, SEEK_END) == 0);
  auto n = std::ftell(f);
  MANUAL_REQUIRE(n >= 128);
  std::rewind(f);
  std::vector<uint8_t> bytes(static_cast<size_t>(n));
  MANUAL_REQUIRE(std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size());
  std::fclose(f);
  for (size_t offset :
       {size_t(0), size_t(8), size_t(12), size_t(16), size_t(20), size_t(24),
        size_t(28), size_t(32), size_t(40), size_t(60), size_t(124),
        bytes.size() - 1}) {
    auto broken = bytes;
    broken.at(offset) ^= 1;
    MANUAL_REQUIRE(
        !data.load(broken.data(), broken.size(), actual_cache, error));
    MANUAL_REQUIRE(data.valid() && data.ir_sha256() == admitted);
  }
  GlobalYamlCachesData foreign;
  MANUAL_REQUIRE(!data.load(bytes.data(), bytes.size(), foreign, error));
  MANUAL_REQUIRE(
      !data.load(bytes.data(), bytes.size() - 1, actual_cache, error));
  MANUAL_REQUIRE(data.ir_sha256() == admitted);
}
