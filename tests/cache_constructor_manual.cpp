// Explicit manual production-host driver; no automatic test execution.
#include "manual_require.hpp"
#include "platform/ctr/podunk_global_data_host.hpp"
#include "platform/ctr/podunk_native_root.hpp"
#include <cerrno>
#include <fstream>
using namespace encore::upstream;
using encore::ctr::PodunkGlobalDataHost;
using encore::ctr::PodunkNativeRoot;
void global_yaml_file_format_manual(const char *, const GlobalYamlCachesData &);
void global_yaml_caches_manual_parser_negative(const std::vector<uint8_t> &,
                                               const FieldGlobalExternalSpec &);
namespace encore::upstream {
void manual_packed_directory_parser(const std::vector<uint8_t> &,
                                    const GlobalYamlCachesData &,
                                    const FieldGlobalRegistryData &);
}
namespace {
template <size_t N> bool hex(std::string_view s, std::array<uint8_t, N> &out) {
  if (s.size() != N * 2)
    return false;
  auto digit = [](char c) {
    if (c >= '0' && c <= '9')
      return int(c - '0');
    if (c >= 'a' && c <= 'f')
      return int(c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
      return int(c - 'A' + 10);
    return -1;
  };
  for (size_t i = 0; i < N; ++i) {
    auto a = digit(s[i * 2]), b = digit(s[i * 2 + 1]);
    if (a < 0 || b < 0)
      return false;
    out[i] = uint8_t(a * 16 + b);
  }
  return true;
}
bool read(const char *path, std::vector<uint8_t> &out, std::string &e) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    e = "Manual source resource unreadable: " + std::string(path);
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(f), {});
  if (!f.eof() && f.fail()) {
    e = "Manual source resource read failed";
    return false;
  }
  return true;
}
bool source_spec(const FieldGlobalRegistryData &registry,
                 const FieldGlobalDataData &data,
                 FieldGlobalExternalSpec &out) {
  for (const auto &row : registry.autoloads())
    if (row.script == data.owner_source()) {
      out = {registry.identity(), row.id,   3,          row.name,
             row.native_class,    row.path, row.script, row.source_sha,
             row.script_sha};
      return true;
    }
  return false;
}
} // namespace
int main(int argc, char **argv) {
  if (argc != 12) {
    std::cerr << "Usage: cache_constructor_manual REGISTRY NATIVE_ROOT "
                 "GLOBALDATA FLAGS GLOBAL_ITEMS YAML_CACHES PACKED_DIRECTORY "
                 "YAML_FILES SCENE_ID PIN_HEX SOURCE_SHA_HEX\n";
    return 2;
  }
  FieldIdentity expected;
  errno = 0;
  char *end = nullptr;
  auto scene = std::strtoull(argv[9], &end, 10);
  if (errno || !end || end == argv[9] || *end || !scene || scene > UINT32_MAX ||
      !hex(argv[10], expected.upstream_commit) ||
      !hex(argv[11], expected.source_sha256)) {
    std::cerr << "Invalid independent expected Registry identity\n";
    return 2;
  }
  expected.scene_id = uint32_t(scene);
  std::string error;
  FieldGlobalRegistryData registry_data;
  FieldNativeRootData native_data;
  FieldGlobalDataData global_data;
  FieldGlobalFlagsData flags_data;
  FieldItemDefinitions item_data;
  if (!registry_data.load_file(argv[1], expected, error) ||
      !native_data.load_file(argv[2], registry_data, error) ||
      !global_data.load_file(argv[3], expected, error) ||
      !flags_data.load_file(argv[4], error) ||
      !item_data.load_file(argv[5], error)) {
    std::cerr << error << '\n';
    return 1;
  }
  FieldGlobalExternalSpec spec;
  MANUAL_REQUIRE(source_spec(registry_data, global_data, spec));
  GlobalYamlCachesData caches_data;
  GlobalPackedDirectoryData directory_data;
  GlobalYamlFileData files_data;
  if (!caches_data.load_file(argv[6], spec, error) ||
      !directory_data.load_file(argv[7], caches_data, registry_data, error) ||
      !files_data.load_file(argv[8], caches_data, error)) {
    std::cerr << error << '\n';
    return 1;
  }
  std::vector<uint8_t> cache_bytes, directory_bytes;
  MANUAL_REQUIRE(read(argv[6], cache_bytes, error));
  MANUAL_REQUIRE(read(argv[7], directory_bytes, error));
  global_yaml_caches_manual_parser_negative(cache_bytes, spec);
  global_yaml_file_format_manual(argv[8], caches_data);
  manual_packed_directory_parser(directory_bytes, caches_data, registry_data);
  // The production native SceneTree/Viewport owners are constructed, never
  // placed inside the Tree or granted Ready for this selected constructor body.
  FieldGlobalRegistry registry;
  PodunkNativeRoot native;
  MANUAL_REQUIRE(native.initialize(native_data, registry_data, registry,
                                   nullptr, nullptr, error));
  FieldGlobalRegistryHost registry_host;
  registry_host.construct = [&](auto id, const auto &actual, auto &out,
                                auto &e) {
    return native.construct(id, actual, out, e);
  };
  MANUAL_REQUIRE(
      registry.initialize(registry_data, std::move(registry_host), error));
  FieldObjectId reserved = 0;
  {
    PodunkGlobalDataHost host;
    MANUAL_REQUIRE(registry.allocate_object(reserved, error));
    MANUAL_REQUIRE(!registry.object_exists(reserved));
    MANUAL_REQUIRE(
        host.construct_members(global_data, spec, reserved, registry, error));
    uint64_t flag_signals = 0;
    std::vector<std::string> warnings;
    // Concrete sinks observe actual emitted events; no listener or native Ready
    // implementation is inferred from merely having a callback.
    auto emit = [&](std::string &e) {
      ++flag_signals;
      e.clear();
      return true;
    };
    auto warning = [&](const std::string &text, std::string &e) {
      warnings.push_back(text);
      std::cerr << text << '\n';
      e.clear();
      return bool(std::cerr);
    };
    MANUAL_REQUIRE(host.construct_cache_prefix(
        flags_data, caches_data, item_data, emit, warning, error));
    MANUAL_REQUIRE(!host.cache_prefix_complete());
    MANUAL_REQUIRE(!host.items_cache().directory_admitted());
    const auto baseline = registry.object_count();
    MANUAL_REQUIRE(
        host.begin_cache_directory_prefix(directory_data, files_data, error));
    auto cursor = host.cache_source_cursor();
    MANUAL_REQUIRE(cursor.directory &&
                   registry.object_exists(cursor.directory));
    auto reference = registry.native_reference(cursor.directory);
    MANUAL_REQUIRE(reference &&
                   std::string_view(reference->native_class()) == "Directory");
    reference.reset();
    // Drive every real source statement, including Directory queues, File and
    // SmartFileReader lifetimes, through the already integrated production
    // Host.
    while (!host.cache_prefix_complete()) {
      if (!host.step_cache_directory_prefix(error)) {
        std::cerr << "Source cache constructor cursor failed: " << error
                  << '\n';
        return 1;
      }
    }
    MANUAL_REQUIRE(host.yaml_caches().init_caches_complete());
    MANUAL_REQUIRE(host.items_cache().directory_admitted());
    MANUAL_REQUIRE(registry.object_count() == baseline);
    MANUAL_REQUIRE(flag_signals == 0 && warnings.empty());
    MANUAL_REQUIRE(!native.viewport().inside && !native.viewport().ready);
    MANUAL_REQUIRE(!registry.object_exists(reserved));
    // All getter names/roles/content keys come from the checked binary
    // resource. Insertion lists are actual source Directory order, not
    // descriptor sorting.
    for (const auto &getter : caches_data.getters()) {
      std::vector<std::string> args;
      const auto &order = host.yaml_caches().insertion_order(getter.role);
      if (getter.action != 3) {
        MANUAL_REQUIRE(!order.empty());
        args.push_back(order.front());
      }
      std::shared_ptr<GlobalYamlValue> result;
      MANUAL_REQUIRE(
          host.call_cache_getter(getter.method, args, result, error));
      MANUAL_REQUIRE(result);
      if (getter.action == 3) {
        MANUAL_REQUIRE(result->kind == 5 &&
                       result->array.size() == order.size());
        for (size_t i = 0; i < order.size(); ++i)
          MANUAL_REQUIRE(result->array[i] && result->array[i]->kind == 4 &&
                         result->array[i]->string == order[i]);
      } else if (getter.action == 2)
        MANUAL_REQUIRE(result->kind == 1 && result->boolean);
      else {
        MANUAL_REQUIRE(result->kind ==
                       caches_data.policies()[getter.role].root_kind);
        if (!getter.mutation.empty()) {
          auto id = result->get(getter.mutation);
          MANUAL_REQUIRE(id && id->kind == 4 && id->string == args.front());
        }
        std::shared_ptr<GlobalYamlValue> same;
        MANUAL_REQUIRE(
            host.call_cache_getter(getter.method, args, same, error) &&
            same == result);
      }
    }
    MANUAL_REQUIRE(warnings.empty());
    MANUAL_REQUIRE(!host.step_cache_directory_prefix(error));
    // Retire this selected reserved constructor slot only after its actual
    // owner has been destroyed. No unimplemented external autoload was
    // published.
    std::cout << caches_data.records().size()
              << " source YAMLs; six actual Directory cache constructor loops "
                 "and getters completed. No whole global Ready invoked.\n";
  }
  MANUAL_REQUIRE(registry.retire_object(reserved, error));
  return 0;
}
