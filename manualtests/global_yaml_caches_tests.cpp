#include "../tests/manual_require.hpp"
#include "encore/crc32.hpp"
#include "encore/global_yaml_caches.hpp"
using namespace encore::upstream;
namespace {
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
void put(std::vector<uint8_t> &b, size_t at, uint32_t v) {
  MANUAL_REQUIRE(at + 4 <= b.size());
  for (unsigned i = 0; i < 4; ++i)
    b[at + i] = uint8_t(v >> (8 * i));
}
void refresh(std::vector<uint8_t> &b) {
  put(b, 20, encore::crc32(b.data() + 128, b.size() - 128));
}
size_t first_value(const std::vector<uint8_t> &b) {
  size_t at = 128;
  auto u = [&]() {
    MANUAL_REQUIRE(at + 4 <= b.size());
    auto v = word(b.data() + at);
    at += 4;
    return v;
  };
  auto t = [&]() {
    auto n = u();
    MANUAL_REQUIRE(at + n <= b.size());
    at += n;
  };
  t();
  auto policies = u();
  for (uint32_t i = 0; i < policies; ++i) {
    u();
    u();
    t();
    t();
    t();
    at += 32;
  }
  auto getters = u();
  for (uint32_t i = 0; i < getters; ++i) {
    t();
    u();
    u();
    u();
    t();
    t();
  }
  auto sources = u();
  for (uint32_t i = 0; i < sources; ++i) {
    t();
    at += 32;
  }
  auto records = u();
  MANUAL_REQUIRE(records);
  u();
  t();
  t();
  at += 32;
  MANUAL_REQUIRE(u() == 1);
  return at;
}
} // namespace
// Explicit manual invocation only; compiler checks this source, no auto-run.
void global_yaml_caches_manual_parser_negative(
    const std::vector<uint8_t> &original,
    const FieldGlobalExternalSpec &owner) {
  std::string error;
  GlobalYamlCachesData data;
  MANUAL_REQUIRE(data.load(original.data(), original.size(), owner, error));
  auto old = data.ir_sha256();
  for (size_t offset :
       {size_t(8), size_t(24), size_t(28), size_t(32), size_t(124)}) {
    auto bad = original;
    bad[offset] ^= 0x40;
    MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), owner, error) &&
                   data.ir_sha256() == old);
  }
  auto bad = original;
  bad[first_value(bad)] = 255;
  refresh(bad);
  MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), owner, error) &&
                 data.valid());
  bad = original;
  bad.at(128) ^= 1;
  MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), owner, error));
  MANUAL_REQUIRE(
      !data.load(original.data(), original.size() - 1, owner, error));
  auto wrong = owner;
  wrong.script_sha[0] ^= 1;
  MANUAL_REQUIRE(!data.load(original.data(), original.size(), wrong, error));
  GlobalYamlCachesRuntime absent;
  std::shared_ptr<GlobalYamlValue> result;
  MANUAL_REQUIRE(!absent.begin_directory(0, error));
  MANUAL_REQUIRE(!absent.finish_directory(0, error));
  MANUAL_REQUIRE(!absent.call("unknown", {}, result, error));
}
// Actual source caller supplies its original initialized owning core and real
// Directory enumeration trace. No sorted manifest is treated as that trace.
void global_yaml_caches_manual_owner_invalid_order(
    GlobalYamlCachesRuntime &cache) {
  std::string error;
  auto cursor = cache.next_role();
  MANUAL_REQUIRE(!cache.finish_directory(cursor, error));
  MANUAL_REQUIRE(!cache.begin_directory(cursor + 1, error));
  MANUAL_REQUIRE(!cache.init_caches_complete());
  MANUAL_REQUIRE(cache.next_role() == cursor && !cache.directory_open());
  std::shared_ptr<GlobalYamlValue> result;
  MANUAL_REQUIRE(!cache.call("unknown_getter", {}, result, error));
}
void global_yaml_caches_manual_shared_getter(
    GlobalYamlCachesRuntime &cache, const GlobalYamlGetter &getter,
    const std::string &actual_loaded_name) {
  MANUAL_REQUIRE(getter.action == 1 && !getter.mutation.empty());
  std::string error;
  std::shared_ptr<GlobalYamlValue> first, second;
  MANUAL_REQUIRE(cache.call(getter.method, {actual_loaded_name}, first, error));
  MANUAL_REQUIRE(first && first->kind == 6);
  auto id = first->get(getter.mutation);
  MANUAL_REQUIRE(id && id->kind == 4 && id->string == actual_loaded_name);
  MANUAL_REQUIRE(
      cache.call(getter.method, {actual_loaded_name}, second, error) &&
      first == second);
}
