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
bool equal(const GlobalYamlValue &a, const GlobalYamlValue &b) {
  if (a.kind != b.kind || a.boolean != b.boolean || a.integer != b.integer ||
      a.real != b.real || a.string != b.string ||
      a.array.size() != b.array.size() ||
      a.dictionary.size() != b.dictionary.size())
    return false;
  for (size_t i = 0; i < a.array.size(); ++i)
    if (!equal(*a.array[i], *b.array[i]))
      return false;
  for (size_t i = 0; i < a.dictionary.size(); ++i)
    if (a.dictionary[i].first != b.dictionary[i].first ||
        !equal(*a.dictionary[i].second, *b.dictionary[i].second))
      return false;
  return true;
}
} // namespace
// Caller initializes two independent actual Registries whose numeric owner
// IDs collide. A mere object_exists(owner) check must not admit the foreign
// one.
void global_load_files_registry_manual(const GlobalYamlFileData &files,
                                       const GlobalYamlCachesData &data,
                                       GlobalYamlCachesRuntime &actual,
                                       FieldGlobalRegistry &same,
                                       FieldGlobalRegistry &foreign) {
  MANUAL_REQUIRE(&same != &foreign && actual.registry() == &same);
  MANUAL_REQUIRE(actual.owner() && same.object_exists(actual.owner()) &&
                 foreign.object_exists(actual.owner()));
  const auto before = foreign.object_count();
  std::string error;
  GlobalYamlFileHost rejected;
  MANUAL_REQUIRE(!rejected.initialize(files, data, actual, foreign, error));
  MANUAL_REQUIRE(!error.empty() && !rejected.poisoned() &&
                 foreign.object_count() == before);
  GlobalYamlFileHost admitted;
  MANUAL_REQUIRE(admitted.initialize(files, data, actual, same, error));
}
// Explicit manual entry only. Caller supplies the actual same-Registry cache
// and source FileHost; no fake ObjectIDs, callbacks, Ready, or fixtures.
void global_load_files_manual(GlobalYamlFileHost &host,
                              FieldGlobalRegistry &registry,
                              const std::vector<uint8_t> &pack,
                              const GlobalDataConstructorData &constructor,
                              const FieldCharacterLoadData &characters,
                              const FieldGlobalFlagsData &flags,
                              const FieldItemDefinitions &definitions,
                              const GlobalYamlFileData &files) {
  std::string error;
  GlobalLoadData load;
  MANUAL_REQUIRE(load.load(pack.data(), pack.size(), constructor, characters,
                           flags, definitions, error));
  MANUAL_REQUIRE(files.valid() && load.file_ir_sha256() == files.ir_sha256());
  const auto before = registry.object_count();
  std::shared_ptr<GlobalYamlValue> result;
  for (uint32_t kind : {uint32_t(1), uint32_t(3), UINT32_MAX}) {
    MANUAL_REQUIRE(!host.actual_global_load(load, kind, result, error));
    MANUAL_REQUIRE(!result && !error.empty() && !host.poisoned() &&
                   registry.object_count() == before);
  }
  // Checked data can have a nonzero independent File IR proof that differs
  // from the actual File owner. Runtime must reject it before allocation.
  size_t proof = 128;
  for (size_t i = 0; i < 8; ++i)
    proof += 4 + word(pack, proof);
  auto bad = pack;
  bad.at(proof + 3 * 32) ^= 1;
  seal(bad);
  GlobalLoadData wrong_ir;
  MANUAL_REQUIRE(wrong_ir.load(bad.data(), bad.size(), constructor, characters,
                               flags, definitions, error));
  MANUAL_REQUIRE(!host.actual_global_load(wrong_ir, 0, result, error));
  MANUAL_REQUIRE(!result && registry.object_count() == before &&
                 !host.poisoned());
  // Pin and source-path corruption cannot be laundered into a valid document.
  bad = pack;
  bad.at(40) ^= 1;
  GlobalLoadData wrong_pin;
  MANUAL_REQUIRE(!wrong_pin.load(bad.data(), bad.size(), constructor,
                                 characters, flags, definitions, error));
  MANUAL_REQUIRE(!host.actual_global_load(wrong_pin, 0, result, error));
  bad = pack;
  size_t cold_path = 128 + 4 + word(pack, 128) + 4;
  bad.at(cold_path) = uint8_t('X');
  seal(bad);
  GlobalLoadData wrong_path;
  MANUAL_REQUIRE(!wrong_path.load(bad.data(), bad.size(), constructor,
                                  characters, flags, definitions, error));
  MANUAL_REQUIRE(!host.actual_global_load(wrong_path, 2, result, error));
  MANUAL_REQUIRE(!result && registry.object_count() == before &&
                 !host.poisoned());
  for (uint32_t kind : {uint32_t(0), uint32_t(2)}) {
    // These two real temporary slots only observe counter progress. No source
    // Reference is fabricated or published by the test.
    FieldObjectId first = 0, last = 0;
    MANUAL_REQUIRE(registry.allocate_object(first, error));
    MANUAL_REQUIRE(registry.retire_object(first, error));
    MANUAL_REQUIRE(host.actual_global_load(load, kind, result, error));
    MANUAL_REQUIRE(result && !host.poisoned());
    const GlobalLoadDocument *document = nullptr;
    for (const auto &d : load.documents())
      if (d.kind == kind)
        document = &d;
    MANUAL_REQUIRE(document && result.get() != document->file.parsed.get() &&
                   equal(*result, *document->file.parsed));
    MANUAL_REQUIRE(registry.allocate_object(last, error));
    MANUAL_REQUIRE(last == first + 4); // outer File, Reader, member File.
    for (FieldObjectId id = first + 1; id < last; ++id)
      MANUAL_REQUIRE(!registry.object_exists(id) &&
                     !registry.native_reference(id));
    MANUAL_REQUIRE(registry.retire_object(last, error));
    MANUAL_REQUIRE(registry.object_count() == before);
    result.reset();
  }
  // Observe actual public Reference ownership: reader owns its member File;
  // retaining the inner Reference delays only that ObjectDB retirement.
  std::shared_ptr<GlobalYamlFileReference> outer;
  std::shared_ptr<GlobalYamlSmartReader> reader;
  MANUAL_REQUIRE(host.make_file(0, outer, error));
  const auto outer_id = outer->binding().object;
  MANUAL_REQUIRE(outer->registry() == &registry);
  MANUAL_REQUIRE(host.make_reader(reader, error));
  const auto reader_id = reader->binding().object,
             inner_id = reader->member_file_object();
  MANUAL_REQUIRE(outer_id < reader_id && reader_id < inner_id);
  auto inner = registry.native_reference(inner_id);
  MANUAL_REQUIRE(inner && inner->registry() == &registry &&
                 reader->registry() == &registry);
  MANUAL_REQUIRE(!registry.retire_object(inner_id, error) &&
                 !registry.retire_object(reader_id, error));
  reader.reset();
  MANUAL_REQUIRE(!registry.object_exists(reader_id) &&
                 registry.object_exists(inner_id) &&
                 registry.object_exists(outer_id));
  inner.reset();
  MANUAL_REQUIRE(!registry.object_exists(inner_id) &&
                 registry.object_exists(outer_id));
  outer.reset();
  MANUAL_REQUIRE(!registry.object_exists(outer_id) &&
                 registry.object_count() == before);
}
} // namespace encore::upstream
