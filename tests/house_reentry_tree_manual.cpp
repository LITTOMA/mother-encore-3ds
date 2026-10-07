#include "encore/field_node_tree.hpp"
#include "encore/house_reentry.hpp"
#include "manual_require.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
using namespace encore::upstream;
namespace {
void put(std::vector<uint8_t> &bytes, size_t at, uint32_t value) {
  MANUAL_REQUIRE(at + 4 <= bytes.size());
  for (unsigned i = 0; i < 4; ++i) bytes[at + i] = uint8_t(value >> (8 * i));
}
uint32_t word(const std::vector<uint8_t> &bytes, size_t at) {
  MANUAL_REQUIRE(at + 4 <= bytes.size());
  return uint32_t(bytes[at]) | uint32_t(bytes[at+1]) << 8 |
         uint32_t(bytes[at+2]) << 16 | uint32_t(bytes[at+3]) << 24;
}
void seal(std::vector<uint8_t> &bytes) {
  uint32_t crc = ~0u;
  for (size_t i = 128; i < bytes.size(); ++i) {
    crc ^= bytes[i];
    for (unsigned j = 0; j < 8; ++j)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
  }
  put(bytes, 16, uint32_t(bytes.size())); put(bytes, 20, ~crc);
}
size_t tile_offset(const std::vector<uint8_t> &bytes, const HouseReentryTileMap &tile) {
  std::vector<uint8_t> prefix(24);
  put(prefix, 0, tile.id); put(prefix, 4, tile.layer); put(prefix, 8, tile.mask);
  put(prefix, 12, tile.cell_count); put(prefix, 16, 0);
  put(prefix, 20, uint32_t(tile.node.size()));
  prefix.insert(prefix.end(), tile.node.begin(), tile.node.end());
  const auto found = std::search(bytes.begin(), bytes.end(), prefix.begin(), prefix.end());
  MANUAL_REQUIRE(found != bytes.end());
  MANUAL_REQUIRE(std::search(found + 1, bytes.end(), prefix.begin(), prefix.end()) == bytes.end());
  return size_t(found - bytes.begin());
}
bool hex(const std::string &s, uint8_t *out, size_t n) {
  if (s.size() != 2 * n) return false;
  for (size_t i = 0; i < n; ++i) {
    unsigned value = 0;
    for (unsigned j = 0; j < 2; ++j) {
      const char c = s[2 * i + j];
      const unsigned digit = c >= '0' && c <= '9' ? unsigned(c - '0') :
                             c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10) : 99;
      if (digit > 15) return false;
      value = value * 16 + digit;
    }
    out[i] = uint8_t(value);
  }
  return true;
}
// A manual fixture ObjectDB checks allocation boundaries. These callbacks do
// not implement source gameplay or attest native House Ready.
struct Factory {
  FieldObjectId next = 100;
  std::set<FieldObjectId> objects;
  std::map<FieldObjectId, FieldNodeDescriptor> descriptors;
  unsigned allocations = 0, constructors = 0, bindings = 0, dispatches = 0;
  unsigned fail_constructor = 0;
  FieldNodeTreeHost host() {
    FieldNodeTreeHost h;
    h.object_domain = 99;
    h.allocate_object = [&](FieldObjectId &out, std::string &) {
      out = ++next; ++allocations; objects.insert(out); return true;
    };
    h.allocate_fast_name = [](uint64_t &out, std::string &) { out = 1; return true; };
    h.native_allocated = [&](FieldObjectId id, const FieldNodeDescriptor &d,
                             const FieldIdentity &, std::string &) {
      return descriptors.emplace(id, d).second;
    };
    h.construct_source = [&](FieldObjectId, const FieldNodeDescriptor &,
                             const FieldIdentity &, std::string &e) {
      ++constructors;
      if (constructors == fail_constructor) { e = "fixture constructor rejected"; return false; }
      return true;
    };
    h.bind = [&](FieldObjectId, const FieldNodeDescriptor &, FieldNodeBinding &,
                 std::string &) { ++bindings; return false; };
    h.dispatch = [&](FieldObjectId, const FieldNodeBinding &, FieldTreePhase,
                     std::string &) { ++dispatches; return false; };
    h.deferred = [](const FieldDeferredMessage &, std::string &) { return false; };
    h.object_exists = [&](FieldObjectId id) { return objects.count(id) != 0; };
    h.input_registration = [](FieldObjectId, uint32_t, bool, std::string &) { return false; };
    h.external_pause_process = [](FieldObjectId) { return true; };
    h.release = [](FieldObjectId, const FieldNodeBinding &, std::string &) { return false; };
    return h;
  }
};
}
int main(int argc, char **argv) {
  if (argc != 8) {
    std::cerr << "return-pack door-pack Room-pack House-pack door-scene-id pin door-source-sha\n";
    return 2;
  }
  FieldIdentity identity;
  identity.scene_id = uint32_t(std::strtoul(argv[5], nullptr, 10));
  MANUAL_REQUIRE(hex(argv[6], identity.upstream_commit.data(), 20));
  MANUAL_REQUIRE(hex(argv[7], identity.source_sha256.data(), 32));
  std::string error;
  RoomData room; HouseData house; FieldDoorData doors; HouseReentryData data;
  MANUAL_REQUIRE(room.load_file(argv[3], error));
  MANUAL_REQUIRE(house.load_file(argv[4], error));
  MANUAL_REQUIRE(doors.load_file(argv[2], identity, error));
  MANUAL_REQUIRE(data.load_file(argv[1], doors, room.view(), house.view(), error));
  MANUAL_REQUIRE(data.tilemaps().size() == 3);
  std::ifstream input(argv[1], std::ios::binary);
  const std::vector<uint8_t> original{std::istreambuf_iterator<char>(input), {}};
  MANUAL_REQUIRE(original.size() >= 128);
  const auto first = tile_offset(original, data.tilemaps()[0]);
  const auto second = tile_offset(original, data.tilemaps()[1]);
  auto reject_resource = [&](std::vector<uint8_t> bytes) {
    seal(bytes);
    const auto old_ir = data.ir_sha256();
    MANUAL_REQUIRE(!data.load(bytes.data(), bytes.size(), doors, room.view(), house.view(), error));
    MANUAL_REQUIRE(data.valid() && data.ir_sha256() == old_ir && data.tilemaps().size() == 3);
  };
  auto bad = original; put(bad, 8, 1); reject_resource(bad);
  bad = original; put(bad, first + 16, 1); reject_resource(bad); // newly colliding tile
  bad = original; put(bad, first + 12, 0); reject_resource(bad); // incomplete placed cells
  bad = original; put(bad, second, data.tilemaps()[0].id); reject_resource(bad);
  size_t cursor = first + 20;
  for (unsigned i = 0; i < 3; ++i) cursor += 4 + word(original, cursor);
  bad = original; bad[cursor + 32] ^= 1; reject_resource(bad); // TileSet owner fingerprint
  cursor += 64;
  MANUAL_REQUIRE(word(original, cursor) != 0);
  bad = original; put(bad, cursor + 8, 2); reject_resource(bad); // unknown present tag
  bad = original; put(bad, cursor + 12, 1); reject_resource(bad); // unsupported native shape
  bad = original; bad.pop_back(); reject_resource(bad);

  Factory good;
  FieldNodeTreeRuntime tree;
  MANUAL_REQUIRE(tree.initialize_house_continuation(data, good.host(), error));
  MANUAL_REQUIRE(good.allocations == 2 && good.constructors == 2);
  MANUAL_REQUIRE(good.bindings == 0 && good.dispatches == 0);
  MANUAL_REQUIRE(tree.object_count() == 2 && !tree.lifecycle_pending());
  const auto root = tree.source_object(data.native_nodes()[0].id);
  const auto container = tree.source_object(data.native_nodes()[1].id);
  MANUAL_REQUIRE(root && container && root != container && tree.root() == root);
  const auto *r = tree.state(root), *c = tree.state(container);
  MANUAL_REQUIRE(r && c && !r->inside && !c->inside);
  MANUAL_REQUIRE(r->ready_first && c->ready_first && !r->ready_notified && !c->ready_notified);
  MANUAL_REQUIRE(!r->bound && !c->bound && !r->parent && c->parent == root);
  MANUAL_REQUIRE(c->owner == root && c->canvas_parent == root);
  MANUAL_REQUIRE(tree.descriptor(root)->script_sha == data.native_nodes()[0].script_sha);
  MANUAL_REQUIRE(tree.descriptor(container)->script.empty());
  FieldIdentity actual;
  MANUAL_REQUIRE(tree.object_identity(container, actual));
  MANUAL_REQUIRE(actual.scene_id == data.identity().scene_id &&
                 actual.upstream_commit == data.identity().upstream_commit &&
                 actual.source_sha256 == data.identity().source_sha256);
  const unsigned before = good.allocations;
  MANUAL_REQUIRE(!tree.initialize_house_continuation(data, good.host(), error));
  MANUAL_REQUIRE(good.allocations == before && tree.object_count() == 2);

  HouseReentryData invalid;
  Factory negative;
  FieldNodeTreeRuntime empty;
  MANUAL_REQUIRE(!empty.initialize_house_continuation(invalid, negative.host(), error));
  MANUAL_REQUIRE(negative.allocations == 0 && empty.object_count() == 0);
  auto missing = negative.host(); missing.construct_source = {};
  MANUAL_REQUIRE(!empty.initialize_house_continuation(data, missing, error));
  missing = negative.host(); missing.native_allocated = {};
  MANUAL_REQUIRE(!empty.initialize_house_continuation(data, missing, error));
  missing = negative.host(); missing.object_domain = 0;
  MANUAL_REQUIRE(!empty.initialize_house_continuation(data, missing, error));
  missing = negative.host();
  missing.enqueue_global = [](FieldDeferredMessage, std::string &) { return false; };
  MANUAL_REQUIRE(!empty.initialize_house_continuation(data, missing, error));
  MANUAL_REQUIRE(negative.allocations == 0 && empty.object_count() == 0);

  Factory failure; failure.fail_constructor = 2;
  FieldNodeTreeRuntime rejected;
  MANUAL_REQUIRE(!rejected.initialize_house_continuation(data, failure.host(), error));
  MANUAL_REQUIRE(!rejected.root() && failure.bindings == 0 && failure.dispatches == 0);
  const unsigned failed_allocations = failure.allocations;
  MANUAL_REQUIRE(!rejected.initialize_house_continuation(data, failure.host(), error));
  MANUAL_REQUIRE(failure.allocations == failed_allocations);
  std::cout << "Bounded House allocation checks completed; no source lifecycle executed\n";
}
