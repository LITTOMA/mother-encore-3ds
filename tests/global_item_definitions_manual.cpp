#include "encore/field_global_data.hpp"
#include "encore/global_item_cache.hpp"
#include "manual_require.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
using namespace encore::upstream;
// Manual-only format cases. Not registered with automatic CI and not executed.
void global_item_definitions_manual(const char *global_pack,
                                    const char *scoped_pack) {
  std::string e;
  FieldItemDefinitions all, scoped;
  MANUAL_REQUIRE(all.load_file(global_pack, e) &&
                 all.global_constructor_scope());
  MANUAL_REQUIRE(scoped.load_file(scoped_pack, e) &&
                 !scoped.global_constructor_scope());
  GlobalItemCache bad;
  MANUAL_REQUIRE(!bad.initialize(scoped, 1, e));
  GlobalItemCache cache;
  MANUAL_REQUIRE(cache.initialize(all, 1, e));
  auto order = all.definitions();
  std::reverse(order.begin(), order.end());
  for (const auto &r : order) {
    std::array<uint8_t, 32> h{};
    MANUAL_REQUIRE(all.source_hash(r.source, h));
    auto wrong = h;
    wrong[0] ^= 1;
    MANUAL_REQUIRE(!cache.insert_loaded_yaml(r.source, wrong, e));
    MANUAL_REQUIRE(cache.insert_loaded_yaml(r.source, h, e));
    MANUAL_REQUIRE(!cache.insert_loaded_yaml(r.source, h, e));
  }
  MANUAL_REQUIRE(cache.definitions_loaded() &&
                 cache.insertion_order().front() == order.front().id);
  MANUAL_REQUIRE(!cache.source_id_assigned(order.front().id));
  MANUAL_REQUIRE(cache.get_item_data(order.front().id));
  MANUAL_REQUIRE(cache.source_id_assigned(order.front().id));
  MANUAL_REQUIRE(cache.source_id(order.front().id) &&
                 *cache.source_id(order.front().id) == order.front().item_name);
  std::ifstream file(global_pack, std::ios::binary);
  std::vector<uint8_t> raw((std::istreambuf_iterator<char>(file)), {});
  for (auto offset : {0u, 8u, 20u, 24u}) {
    auto b = raw;
    b[offset] ^= 0x20;
    uint32_t crc = ~0u;
    for (size_t n = 0; n < b.size(); ++n) {
      crc ^= n >= 16 && n < 20 ? 0 : b[n];
      for (int k = 0; k < 8; ++k)
        crc = (crc >> 1) ^ (0xedb88320u & uint32_t(-int(crc & 1)));
    }
    crc = ~crc;
    for (unsigned n = 0; n < 4; ++n)
      b[16 + n] = uint8_t(crc >> (n * 8));
    FieldItemDefinitions unknown;
    MANUAL_REQUIRE(!unknown.load(b.data(), b.size(), e));
  }
  FieldGlobalDataRuntime unconstructed;
  SourceRandom random(1);
  std::vector<uint32_t> ledger;
  MANUAL_REQUIRE(!unconstructed.construct_god_storage(cache, "en", random,
                                                      ledger, {}, {}, e));
  MANUAL_REQUIRE(ledger.empty());
}
int main(int argc, char **argv) {
  std::string error;
  if (argc == 3 && std::string(argv[1]) == "--expect-reject") {
    std::ifstream input(argv[2], std::ios::binary);
    if (!input) {
      std::cerr << argv[2] << ": unreadable fixture\n";
      return 2;
    }
    FieldItemDefinitions candidate;
    if (candidate.load_file(argv[2], error)) {
      std::cerr << argv[2] << ": unexpectedly accepted invalid resource\n";
      return 1;
    }
    std::cout << argv[2] << ": rejected: " << error << '\n';
    return 0;
  }
  if (argc != 3) {
    std::cerr
        << "Usage: global_item_definitions_manual GLOBAL_PACK SCOPED_PACK\n"
           "       global_item_definitions_manual --expect-reject PACK\n";
    return 2;
  }
  FieldItemDefinitions all, scoped;
  if (!all.load_file(argv[1], error) || !scoped.load_file(argv[2], error)) {
    std::cerr << error << '\n';
    return 1;
  }
  global_item_definitions_manual(argv[1], argv[2]);
  std::cout
      << "Global/scoped definitions and source cache manual cases completed\n";
  return 0;
}
