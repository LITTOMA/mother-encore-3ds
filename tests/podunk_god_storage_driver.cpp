#include "manual_require.hpp"
#include "platform/ctr/podunk_global_data_host.hpp"
#include "platform/ctr/podunk_native_root.hpp"
#include <3ds.h>
#include <cerrno>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <mbedtls/sha256.h>
#include <set>
#include <sys/stat.h>
using namespace encore::upstream;
using encore::ctr::PodunkGlobalDataHost;
using encore::ctr::PodunkNativeRoot;
void podunk_god_storage_host_manual();
void podunk_god_storage_actual_owner_manual(
    PodunkGlobalDataHost &, const FieldItemDefinitions &, FieldGlobalRegistry &,
    SourceRandom &, std::vector<uint32_t> &, LoadRngClockProvider);
namespace {
using SourceRows = std::vector<std::pair<std::string, std::array<uint8_t, 32>>>;
bool failure(std::string &e, const std::string &s) {
  e = s;
  return false;
}
bool sha(const std::vector<uint8_t> &bytes, std::array<uint8_t, 32> &out,
         std::string &e) {
  return mbedtls_sha256_ret(bytes.data(), bytes.size(), out.data(), 0) == 0 ||
         failure(e, "Actual SDK SHA256 failed");
}
// Source Directory enumeration order is retained, never replaced with the
// definitions package's descriptor order or an alphabetical list.
bool enumerate(const std::string &directory, const std::string &relative,
               SourceRows &out, std::string &e) {
  const std::string path = directory + (relative.empty() ? "" : "/" + relative);
  DIR *dir = opendir(path.c_str());
  if (!dir)
    return failure(e, "Cannot enumerate actual source directory: " + path);
  bool ok = true;
  for (;;) {
    errno = 0;
    const auto *entry = readdir(dir);
    if (!entry) {
      if (errno)
        ok = failure(e, "Actual source directory read failed: " + path);
      break;
    }
    const std::string name = entry->d_name;
    if (name == "." || name == "..")
      continue;
    const std::string child = relative.empty() ? name : relative + "/" + name;
    struct stat info{};
    if (stat((directory + "/" + child).c_str(), &info) != 0) {
      ok = failure(e, "Source stat failed: " + child);
      break;
    }
    if (S_ISDIR(info.st_mode)) {
      if (!enumerate(directory, child, out, e)) {
        ok = false;
        break;
      }
    } else if (S_ISREG(info.st_mode) && name.size() >= 5 &&
               name.substr(name.size() - 5) == ".yaml") {
      std::ifstream file(directory + "/" + child, std::ios::binary);
      if (!file) {
        ok = failure(e, "Source YAML unreadable: " + child);
        break;
      }
      std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
      if (!file.eof() && file.fail()) {
        ok = failure(e, "Source YAML read failed: " + child);
        break;
      }
      std::array<uint8_t, 32> hash{};
      if (!sha(bytes, hash, e)) {
        ok = false;
        break;
      }
      out.emplace_back("Data/Items/" + child, hash);
    }
  }
  closedir(dir);
  return ok;
}
bool independent_closure(const FieldItemDefinitions &defs,
                         const SourceRows &rows, std::array<uint8_t, 32> &proof,
                         std::string &e) {
  if (rows.empty() || rows.size() != defs.definitions().size())
    return failure(e,
                   "Actual fixed upstream directory/package coverage differs");
  std::set<std::string> seen;
  std::vector<uint8_t> raw;
  for (const auto &row : rows) {
    std::array<uint8_t, 32> expected{};
    if (!seen.insert(row.first).second ||
        !defs.source_hash(row.first, expected) || expected != row.second)
      return failure(e, "Actual upstream path/SHA differs: " + row.first);
    const auto size = uint32_t(row.first.size());
    for (uint32_t n = 0; n < 4; ++n)
      raw.push_back(uint8_t(size >> (n * 8)));
    raw.insert(raw.end(), row.first.begin(), row.first.end());
    raw.insert(raw.end(), row.second.begin(), row.second.end());
  }
  for (const auto &definition : defs.definitions())
    if (!seen.count(definition.source))
      return failure(e, "Actual upstream YAML missing: " + definition.source);
  return sha(raw, proof, e);
}
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
    const int a = digit(s[n * 2]), b = digit(s[n * 2 + 1]);
    if (a < 0 || b < 0)
      return false;
    out[n] = uint8_t(a * 16 + b);
  }
  return true;
}
bool source_spec(const FieldGlobalRegistryData &registry,
                 const FieldGlobalDataData &data, FieldGlobalExternalSpec &out,
                 std::string &e) {
  for (const auto &row : registry.autoloads())
    if (row.script == data.owner_source()) {
      out = {registry.identity(), row.id,   3,          row.name,
             row.native_class,    row.path, row.script, row.source_sha,
             row.script_sha};
      return true;
    }
  return failure(e,
                 "Actual registry has no globalData source constructor slot");
}
bool construct(PodunkGlobalDataHost &host, const FieldGlobalDataData &data,
               const FieldGlobalExternalSpec &spec,
               FieldGlobalRegistry &registry, const FieldItemDefinitions &defs,
               std::string &e) {
  FieldObjectId reserved = 0;
  return registry.allocate_object(reserved, e) &&
         host.construct_members(data, spec, reserved, registry, e) &&
         host.initialize_items_cache(defs, e);
}
} // namespace
int main(int argc, char **argv) {
  std::string e;
  if (argc == 4 && std::string(argv[1]) == "--expect-reject") {
    std::ifstream file(argv[2], std::ios::binary);
    if (!file) {
      std::cerr << "Unreadable malformed package\n";
      return 2;
    }
    FieldItemDefinitions defs;
    SourceRows rows;
    // A missing source directory is a harness failure, not parser rejection.
    if (!enumerate(argv[3], "", rows, e)) {
      std::cerr << e << '\n';
      return 2;
    }
    std::array<uint8_t, 32> proof{};
    if (defs.load_file(argv[2], e) &&
        independent_closure(defs, rows, proof, e)) {
      std::cerr << "Invalid resource/source closure unexpectedly accepted\n";
      return 1;
    }
    std::cout << "Actual loader/source closure rejected: " << e << '\n';
    return 0;
  }
  if (argc != 9) {
    std::cerr
        << "Usage: podunk_god_storage_manual REGISTRY NATIVE_ROOT GLOBALDATA "
           "GLOBAL_ITEMS ITEMS_DIRECTORY SCENE_ID PIN_HEX SOURCE_SHA_HEX\n     "
           "  podunk_god_storage_manual --expect-reject PACK ITEMS_DIRECTORY\n";
    return 2;
  }
  FieldIdentity identity;
  char *end = nullptr;
  const auto scene = std::strtoull(argv[6], &end, 10);
  if (!end || *end || !scene || scene > UINT32_MAX ||
      !hex(argv[7], identity.upstream_commit) ||
      !hex(argv[8], identity.source_sha256)) {
    std::cerr << "Invalid independent expected registry identity\n";
    return 2;
  }
  identity.scene_id = uint32_t(scene);
  FieldGlobalRegistryData registry_data;
  FieldNativeRootData native_data;
  FieldGlobalDataData global_data;
  FieldItemDefinitions defs;
  if (!registry_data.load_file(argv[1], identity, e) ||
      !native_data.load_file(argv[2], registry_data, e) ||
      !global_data.load_file(argv[3], identity, e) ||
      !defs.load_file(argv[4], e)) {
    std::cerr << e << '\n';
    return 1;
  }
  SourceRows rows;
  std::array<uint8_t, 32> proof{};
  if (!enumerate(argv[5], "", rows, e) ||
      !independent_closure(defs, rows, proof, e)) {
    std::cerr << e << '\n';
    return 1;
  }
  FieldGlobalRegistry registry;
  PodunkNativeRoot native;
  MANUAL_REQUIRE(native.initialize(native_data, registry_data, registry,
                                   nullptr, nullptr, e));
  FieldGlobalRegistryHost registry_host;
  registry_host.construct = [&](auto id, const auto &spec, auto &out,
                                std::string &error) {
    return native.construct(id, spec, out, error);
  };
  MANUAL_REQUIRE(registry.initialize(registry_data, registry_host, e));
  FieldGlobalExternalSpec spec;
  MANUAL_REQUIRE(source_spec(registry_data, global_data, spec, e));
  const auto epoch = svcGetSystemTick();
  LoadRngClockProvider clock = [epoch](LoadRngClockSample &out,
                                       std::string &error) {
    const auto now = std::time(nullptr);
    if (now < 0)
      return failure(error, "Actual OS clock failed");
    const auto ticks = svcGetSystemTick() - epoch;
    out = {uint64_t(now),
           (ticks / SYSCLOCK_ARM11) * 1000000 +
               (ticks % SYSCLOCK_ARM11) * 1000000 / SYSCLOCK_ARM11};
    return true;
  };
  podunk_god_storage_host_manual();
  SourceRandom random(1);
  std::vector<uint32_t> ledger;
  // Test selected constructor slots using the real Registry allocator and
  // native root objects. No external autoload Ready or whole startup is forged.
  {
    PodunkGlobalDataHost missing;
    MANUAL_REQUIRE(construct(missing, global_data, spec, registry, defs, e));
    for (size_t n = 0; n + 1 < rows.size(); ++n)
      MANUAL_REQUIRE(
          missing.insert_loaded_item_yaml(rows[n].first, rows[n].second, e));
    MANUAL_REQUIRE(!missing.items_cache().definitions_loaded());
    MANUAL_REQUIRE(!missing.observe_items_directory_complete(rows, proof, e));
    auto truncated = rows;
    truncated.pop_back();
    MANUAL_REQUIRE(
        !missing.observe_items_directory_complete(truncated, proof, e));
    const auto state = random.state(), draws = random.raw_draw_count();
    MANUAL_REQUIRE(
        !missing.construct_god_storage("en", random, ledger, clock, e));
    MANUAL_REQUIRE(random.state() == state &&
                   random.raw_draw_count() == draws && ledger.empty());
    FieldObjectId unchanged = 123;
    MANUAL_REQUIRE(!missing.read_reference_member(
        missing.runtime().globaldata_object(), defs.god_storage_member(),
        unchanged, e));
    MANUAL_REQUIRE(unchanged == 123);
  }
  {
    PodunkGlobalDataHost host;
    MANUAL_REQUIRE(construct(host, global_data, spec, registry, defs, e));
    for (const auto &row : rows)
      MANUAL_REQUIRE(host.insert_loaded_item_yaml(row.first, row.second, e));
    MANUAL_REQUIRE(host.items_cache().definitions_loaded() &&
                   !host.items_cache().directory_admitted());
    const auto state = random.state(), draws = random.raw_draw_count();
    MANUAL_REQUIRE(!host.construct_god_storage("en", random, ledger, clock, e));
    MANUAL_REQUIRE(random.state() == state &&
                   random.raw_draw_count() == draws && ledger.empty());
    auto wrong = rows;
    wrong.front().second[0] ^= 1;
    MANUAL_REQUIRE(!host.observe_items_directory_complete(wrong, proof, e));
    auto truncated = rows;
    truncated.pop_back();
    MANUAL_REQUIRE(!host.observe_items_directory_complete(truncated, proof, e));
    std::array<uint8_t, 32> zero{};
    MANUAL_REQUIRE(!host.observe_items_directory_complete(rows, zero, e));
    MANUAL_REQUIRE(host.observe_items_directory_complete(rows, proof, e));
    MANUAL_REQUIRE(host.items_cache().directory_admitted());
    MANUAL_REQUIRE(!host.insert_loaded_item_yaml(rows.front().first,
                                                 rows.front().second, e));
    podunk_god_storage_actual_owner_manual(host, defs, registry, random, ledger,
                                           clock);
    MANUAL_REQUIRE(!native.viewport().inside && !native.viewport().ready);
  }
  std::cout
      << rows.size()
      << " actual source YAMLs; selected GodStorage constructor/Reference "
         "cases completed. Full global Ready was not invoked.\n";
  return 0;
}
