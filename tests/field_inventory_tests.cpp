// Manual only: compile this source now; do not execute without explicit
// request.
#include "encore/field_inventory.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
using namespace encore::upstream;
static std::vector<uint8_t> read(const char *p) {
  std::ifstream f(p, std::ios::binary);
  assert(f);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
static uint32_t crc(const std::vector<uint8_t> &b) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < b.size(); ++i) {
    c ^= i >= 16 && i < 20 ? 0 : b[i];
    for (unsigned k = 0; k < 8; ++k)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
static void patch(std::vector<uint8_t> &b, size_t at, uint32_t v) {
  for (unsigned k = 0; k < 4; ++k)
    b[at + k] = uint8_t(v >> (k * 8));
}
int main(int argc, char **argv) {
  assert(argc == 3);
  std::string e;
  auto raw = read(argv[1]);
  FieldInventoryData data;
  FieldItemDefinitions definitions;
  assert(data.load(raw.data(), raw.size(), e) &&
         definitions.load_file(argv[2], e) &&
         data.bind_definitions(definitions, e));
  for (size_t n = 0; n < raw.size(); ++n) {
    FieldInventoryData bad;
    assert(!bad.load(raw.data(), n, e));
  }
  for (auto offset : {8u, 20u, 24u, 28u}) {
    auto b = raw;
    patch(b, offset, 99);
    patch(b, 16, crc(b));
    FieldInventoryData bad;
    assert(!bad.load(b.data(), b.size(), e));
  }
  FieldInventoryState initial;
  initial.level = data.initial_level();
  initial.items.party_order = {data.role(0)->id};
  for (const auto &o : data.owners())
    initial.items.inventories.push_back({o.id, o.role, {}});
  FieldInventoryRuntime runtime;
  assert(runtime.initialize(data, definitions, initial, e));
  SourceRandom rng(7);
  std::vector<uint32_t> ledger;
  uint64_t calls = 0;
  auto clock = [&](LoadRngClockSample &s, std::string &) {
    s = {1700000000, ++calls};
    return true;
  };
  assert(runtime.source_initial(rng, ledger, clock, e));
  assert(calls == data.initial().size());
  auto s = runtime.state();
  auto *normal = &*std::find_if(
      s.items.inventories.begin(), s.items.inventories.end(),
      [&](const auto &i) { return i.owner == data.role(0)->id; });
  const auto *hp = definitions.definition("CapsuleHP");
  assert(hp);
  normal->items.push_back({hp->id, 0, hp->doses, false});
  assert(runtime.initialize(data, definitions, s, e));
  FieldInventoryResult result;
  int64_t before = runtime.state().hp;
  assert(runtime.consume(0, 0, data.role(0)->id, result, e));
  assert(result.removed && runtime.state().hp == before + hp->boost[0] &&
         runtime.state().permanent[0] == hp->boost[0]);
  auto duplicate = s;
  duplicate.items.inventories.front().items.push_back(normal->items.back());
  assert(!runtime.validate(duplicate, e));
  auto malformed = s;
  for (auto &i : malformed.items.inventories)
    for (auto &v : i.items)
      v.doses = 0;
  assert(!runtime.validate(malformed, e));
  malformed = s;
  malformed.statuses.push_back({"not-an-upstream-status", 0});
  assert(!runtime.validate(malformed, e));
  malformed = s;
  malformed.items.party_order.push_back(123);
  assert(!runtime.validate(malformed, e));
  std::vector<uint8_t> saved;
  assert(runtime.encode_save(saved, e));
  FieldInventoryState decoded;
  assert(runtime.decode_save(saved.data(), saved.size(), decoded, e));
  for (size_t n = 0; n < saved.size(); ++n)
    assert(!runtime.decode_save(saved.data(), n, decoded, e));
  for (auto offset : {8u, 20u, 24u, 28u, 64u}) {
    auto bad = saved;
    patch(bad, offset, 99);
    patch(bad, 16, crc(bad));
    assert(!runtime.decode_save(bad.data(), bad.size(), decoded, e));
  }
  auto old = rng.state();
  auto oldcalls = calls;
  assert(runtime.restore(saved.data(), saved.size(), rng, ledger, clock, e));
  assert(calls > oldcalls && rng.state() != old);
  // The callback adapter is the owning consumer, not a mutation approval stub.
  auto host = runtime.definitions_host();
  FieldItemSnapshot b;
  assert(host.read(b, e));
  auto forged = b;
  forged.inventories.front().items.clear();
  FieldItemResult noop;
  assert(!host.commit(b, forged, noop, e));
  return 0;
}
