// Explicit manual cases only. CI and development do not execute this file.
#include "../tests/manual_require.hpp"
#include "encore/crc32.hpp"
#include "encore/global_packed_directory.hpp"
namespace encore::upstream {
namespace {
void set_word(std::vector<uint8_t> &b, size_t i, uint32_t x) {
  for (unsigned k = 0; k < 4; ++k)
    b[i + k] = uint8_t(x >> (k * 8));
}
void refresh(std::vector<uint8_t> &b) {
  set_word(b, 20, crc32(b.data() + 128, b.size() - 128));
}
class ClaimedReference final : public FieldGlobalNativeReference {
public:
  FieldGlobalExternalBinding b;
  const FieldGlobalRegistry *domain = nullptr;
  FieldGlobalExternalBinding binding() const override { return b; }
  const char *native_class() const override { return "Directory"; }
  const FieldGlobalRegistry *registry() const override { return domain; }
  bool checked_source_hash(std::string_view p,
                           std::array<uint8_t, 32> &out) const override {
    if (p != b.source.source)
      return false;
    out = b.source.source_sha;
    return true;
  }
};
} // namespace
void manual_packed_directory_parser(const std::vector<uint8_t> &bytes,
                                    const GlobalYamlCachesData &c,
                                    const FieldGlobalRegistryData &native) {
  if (bytes.size() < 256) {
    MANUAL_REQUIRE(false);
    return;
  }
  std::string e;
  GlobalPackedDirectoryData d;
  MANUAL_REQUIRE(d.load(bytes.data(), bytes.size(), c, native, e));
  const auto original = d.ir_sha256();
  for (size_t offset :
       {size_t(8), size_t(24), size_t(28), size_t(32), size_t(124)}) {
    auto bad = bytes;
    set_word(bad, offset, 99);
    MANUAL_REQUIRE(!d.load(bad.data(), bad.size(), c, native, e));
    MANUAL_REQUIRE(d.valid() && d.ir_sha256() == original);
  }
  auto bad = bytes;
  bad.at(128) ^= 1;
  MANUAL_REQUIRE(!d.load(bad.data(), bad.size(), c, native, e));
  MANUAL_REQUIRE(!d.load(bytes.data(), bytes.size() - 1, c, native, e));
  // Rules cannot be relaxed by recomputing CRC: locate native rule payload.
  bad = bytes;
  size_t at = 128;
  auto read = [&](size_t p) {
    return uint32_t(bad[p]) | uint32_t(bad[p + 1]) << 8 |
           uint32_t(bad[p + 2]) << 16 | uint32_t(bad[p + 3]) << 24;
  };
  at += 4 + read(at);
  at += 4 + read(at);
  at += 4 + read(at);
  set_word(bad, at + 4, 2);
  refresh(bad);
  MANUAL_REQUIRE(!d.load(bad.data(), bad.size(), c, native, e));
}
void manual_packed_directory_references(GlobalPackedDirectoryHost &host,
                                        FieldGlobalRegistry &registry,
                                        FieldGlobalRegistry &foreign) {
  std::string e;
  std::shared_ptr<GlobalPackedDirectoryReference> ref;
  auto before = registry.object_count();
  MANUAL_REQUIRE(host.make_reference(ref, e));
  const auto id = ref->binding().object;
  MANUAL_REQUIRE(registry.object_count() == before + 1 &&
                 registry.object_exists(id));
  MANUAL_REQUIRE(registry.native_reference(id).get() == ref.get());
  MANUAL_REQUIRE(!registry.retire_object(id, e));
  auto spec = ref->binding().source;
  MANUAL_REQUIRE(!registry.publish_native_reference(spec, id, ref, e));
  MANUAL_REQUIRE(!ref->invoke_node_method("get_node", e));
  MANUAL_REQUIRE(!ref->open("user://Data/Items/", e));
  std::string next;
  bool isdir = false;
  MANUAL_REQUIRE(!ref->get_next(next, e));
  MANUAL_REQUIRE(!ref->current_is_dir(isdir, e));
  auto claim = std::make_shared<ClaimedReference>();
  claim->b = ref->binding();
  claim->domain = &registry;
  FieldObjectId slot = 0;
  MANUAL_REQUIRE(registry.allocate_object(slot, e));
  claim->b.object = slot;
  claim->domain = &foreign;
  MANUAL_REQUIRE(!registry.publish_native_reference(spec, slot, claim, e));
  claim->domain = &registry;
  auto wrong = spec;
  wrong.identity.upstream_commit[0] ^= 1;
  MANUAL_REQUIRE(!registry.publish_native_reference(wrong, slot, claim, e));
  wrong = spec;
  wrong.source_sha[0] ^= 1;
  MANUAL_REQUIRE(!registry.publish_native_reference(wrong, slot, claim, e));
  wrong = spec;
  wrong.native_class = "Node";
  MANUAL_REQUIRE(!registry.publish_native_reference(wrong, slot, claim, e));
  auto family = claim->b.family;
  claim->b.family = 0x454effff;
  MANUAL_REQUIRE(!registry.publish_native_reference(spec, slot, claim, e));
  claim->b.family = family;
  claim->b.capability = 99;
  MANUAL_REQUIRE(!registry.publish_native_reference(spec, slot, claim, e));
  claim->b.capability = 1;
  MANUAL_REQUIRE(!registry.publish_native_reference(spec, id, claim, e));
  MANUAL_REQUIRE(registry.retire_object(slot, e));
  auto retained = ref;
  ref.reset();
  MANUAL_REQUIRE(registry.object_exists(id));
  retained.reset();
  MANUAL_REQUIRE(!registry.object_exists(id));
  MANUAL_REQUIRE(registry.object_count() == before);
}
void manual_packed_directory_loop_guard(GlobalPackedDirectoryHost &host) {
  std::string e;
  MANUAL_REQUIRE(!host.step(e));
  MANUAL_REQUIRE(!host.complete());
}
} // namespace encore::upstream
