#include "encore/crc32.hpp"
#include "encore/field_global_data.hpp"
#include "encore/global_yaml_caches.hpp"
#include "manual_require.hpp"
#include <cerrno>
#include <fstream>
using namespace encore::upstream;
namespace {
template <size_t N>
bool hex(const std::string &text, std::array<uint8_t, N> &out) {
  if (text.size() != N * 2)
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
    const auto a = digit(text[n * 2]), b = digit(text[n * 2 + 1]);
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
void put(std::vector<uint8_t> &b, size_t at, uint32_t value) {
  MANUAL_REQUIRE(at <= b.size() && b.size() - at >= 4);
  for (unsigned n = 0; n < 4; ++n)
    b[at + n] = uint8_t(value >> (n * 8));
}
void refresh(std::vector<uint8_t> &b) {
  MANUAL_REQUIRE(b.size() >= 128);
  put(b, 20, encore::crc32(b.data() + 128, b.size() - 128));
}
// Locate reviewed schema fields solely for malformed fixtures. All fixtures
// subsequently enter the production GlobalYamlCachesData loader.
struct Fields {
  size_t policy_role = 0, getter_action = 0, value_kind = 0;
};
Fields fields(const std::vector<uint8_t> &b) {
  size_t at = 128;
  auto skip = [&](size_t n) {
    MANUAL_REQUIRE(at <= b.size() && n <= b.size() - at);
    at += n;
  };
  auto u = [&]() {
    const auto n = word(b, at);
    at += 4;
    return n;
  };
  auto text = [&]() {
    const auto n = u();
    skip(n);
  };
  text();
  const auto policies = u();
  MANUAL_REQUIRE(policies);
  Fields out;
  out.policy_role = at;
  for (uint32_t n = 0; n < policies; ++n) {
    u();
    u();
    text();
    text();
    text();
    skip(32);
  }
  const auto getters = u();
  MANUAL_REQUIRE(getters);
  for (uint32_t n = 0; n < getters; ++n) {
    text();
    u();
    if (!n)
      out.getter_action = at;
    u();
    u();
    text();
    text();
  }
  const auto sources = u();
  for (uint32_t n = 0; n < sources; ++n) {
    text();
    skip(32);
  }
  MANUAL_REQUIRE(u());
  u();
  text();
  text();
  skip(32);
  MANUAL_REQUIRE(u() == 1);
  out.value_kind = at;
  return out;
}
bool source_spec(const FieldGlobalRegistryData &registry,
                 const std::string &script, FieldGlobalExternalSpec &out,
                 std::string &e) {
  for (const auto &row : registry.autoloads())
    if (row.script == script) {
      out = {registry.identity(), row.id,   3,          row.name,
             row.native_class,    row.path, row.script, row.source_sha,
             row.script_sha};
      return true;
    }
  e = "Requested owner script is absent from actual Registry descriptors";
  return false;
}
bool read_file(const char *path, std::vector<uint8_t> &out, std::string &e) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    e = std::string("Unreadable explicit resource: ") + path;
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(file), {});
  if (file.bad()) {
    e = "Actual resource read failed";
    return false;
  }
  return true;
}
void parser_cases(const std::vector<uint8_t> &original,
                  const FieldGlobalExternalSpec &spec) {
  std::string e;
  GlobalYamlCachesData data;
  MANUAL_REQUIRE(data.load(original.data(), original.size(), spec, e));
  const auto identity = data.ir_sha256();
  const auto locations = fields(original);
  // Header-only mutations preserve the body CRC, isolating version/schema
  // rejection. CRC itself is independently damaged below.
  for (const auto offset :
       {size_t(8), size_t(24), size_t(28), size_t(32), size_t(124)}) {
    auto bad = original;
    put(bad, offset, word(bad, offset) ^ 0x40);
    MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), spec, e));
    MANUAL_REQUIRE(data.valid() && data.ir_sha256() == identity);
  }
  for (const auto offset :
       {locations.value_kind, locations.policy_role, locations.getter_action}) {
    auto bad = original;
    put(bad, offset, 99);
    refresh(bad);
    MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), spec, e));
    MANUAL_REQUIRE(data.valid() && data.ir_sha256() == identity);
  }
  auto bad = original;
  bad.at(128) ^= 1;
  MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), spec, e));
  bad = original;
  put(bad, 20, word(bad, 20) ^ 1);
  MANUAL_REQUIRE(!data.load(bad.data(), bad.size(), spec, e));
  MANUAL_REQUIRE(!data.load(original.data(), original.size() - 1, spec, e));
  auto wrong = spec;
  wrong.script_sha[0] ^= 1;
  MANUAL_REQUIRE(!data.load(original.data(), original.size(), wrong, e));
  wrong = spec;
  wrong.role = 0;
  MANUAL_REQUIRE(!data.load(original.data(), original.size(), wrong, e));
  wrong = spec;
  wrong.script += ".unknown";
  MANUAL_REQUIRE(!data.load(original.data(), original.size(), wrong, e));
  MANUAL_REQUIRE(data.valid() && data.ir_sha256() == identity);
}
void host_rejections(const GlobalYamlCachesData &data,
                     const FieldGlobalExternalSpec &spec) {
  // No real Directory lifecycle is synthesized. Its positive execution awaits
  // the independent native PCK Directory owner (family 0051).
  GlobalYamlCachesRuntime absent;
  std::string e;
  MANUAL_REQUIRE(!absent.begin_directory(0, e));
  MANUAL_REQUIRE(!absent.begin_directory(1, e));
  MANUAL_REQUIRE(!absent.finish_directory(0, e));
  MANUAL_REQUIRE(!absent.insert_loaded_yaml(
      data.records().front().source, data.records().front().source_sha, e));
  auto unchanged = std::make_shared<GlobalYamlValue>();
  unchanged->kind = 4;
  unchanged->string = "unchanged";
  auto out = unchanged;
  MANUAL_REQUIRE(!absent.call("unknown_getter", {}, out, e) &&
                 out == unchanged);
  for (const auto &getter : data.getters()) {
    MANUAL_REQUIRE(!absent.call(getter.method,
                                getter.action == 3
                                    ? std::vector<std::string>{}
                                    : std::vector<std::string>{"missing"},
                                out, e));
    MANUAL_REQUIRE(out == unchanged);
  }
  FieldGlobalRegistry uninitialized_registry;
  FieldGlobalDataRuntime unconstructed_core;
  GlobalItemCache uninitialized_items;
  unsigned callbacks = 0;
  GlobalYamlItemsPort port;
  port.actual_cache = &uninitialized_items;
  port.insert = [&](const auto &, const auto &, std::string &) {
    ++callbacks;
    return true;
  };
  port.finish = [&](const auto &, const auto &, std::string &) {
    ++callbacks;
    return true;
  };
  auto flags = [&](GlobalYamlFlagsReceipt &, std::string &) {
    ++callbacks;
    return true;
  };
  auto warning = [&](const std::string &, std::string &) {
    ++callbacks;
    return true;
  };
  // Negative-only identity: no allocated/live source owner exists. The
  // callbacks cannot manufacture proof of that absent owner/constructor.
  MANUAL_REQUIRE(!absent.initialize(data, 0, spec, uninitialized_registry,
                                    unconstructed_core, port, flags, warning,
                                    e));
  auto wrong = spec;
  wrong.script_sha[0] ^= 1;
  MANUAL_REQUIRE(!absent.initialize(data, 1, wrong, uninitialized_registry,
                                    unconstructed_core, port, flags, warning,
                                    e));
  MANUAL_REQUIRE(callbacks == 0 && !absent.init_caches_complete() &&
                 !absent.directory_open() && !absent.poisoned());
}
} // namespace
int main(int argc, char **argv) {
  if (argc != 8 || (std::string(argv[1]) != "--reader" &&
                    std::string(argv[1]) != "--host-rejections" &&
                    std::string(argv[1]) != "--expect-reject")) {
    std::cerr << "Usage: global_yaml_caches_manual MODE REGISTRY CACHE "
                 "SCENE_ID PIN_HEX PROJECT_SHA_HEX OWNER_SCRIPT\nModes: "
                 "--reader, --host-rejections, --expect-reject\nHost "
                 "lifecycle/getter positives require actual native PCK "
                 "Directory owner inputs; this CLI does not initialize them.\n";
    return 2;
  }
  FieldIdentity identity;
  char *end = nullptr;
  errno = 0;
  const auto id = std::strtoull(argv[4], &end, 10);
  if (errno || !end || *end || !id || id > UINT32_MAX ||
      !hex(argv[5], identity.upstream_commit) ||
      !hex(argv[6], identity.source_sha256)) {
    std::cerr << "Invalid independently supplied source identity\n";
    return 2;
  }
  identity.scene_id = uint32_t(id);
  std::string e;
  FieldGlobalRegistryData registry;
  FieldGlobalExternalSpec spec;
  if (!registry.load_file(argv[2], identity, e) ||
      !source_spec(registry, argv[7], spec, e)) {
    std::cerr << argv[2] << ": " << e << '\n';
    return 2;
  }
  std::vector<uint8_t> raw;
  if (!read_file(argv[3], raw, e)) {
    std::cerr << e << '\n';
    return 2;
  }
  GlobalYamlCachesData data;
  if (std::string(argv[1]) == "--expect-reject") {
    if (data.load(raw.data(), raw.size(), spec, e)) {
      std::cerr << argv[3] << ": unexpected acceptance\n";
      return 1;
    }
    std::cout << argv[3] << ": actual loader rejected: " << e << '\n';
    return 0;
  }
  if (!data.load_file(argv[3], spec, e)) {
    std::cerr << argv[3] << ": " << e << '\n';
    return 1;
  }
  if (std::string(argv[1]) == "--reader")
    parser_cases(raw, spec);
  else
    host_rejections(data, spec);
  std::cout << argv[1] << ": manual cases completed. Reader records="
            << data.records().size()
            << ". No native Directory loops, getter positives, full "
               "constructor or Ready were invoked.\n";
  return 0;
}
