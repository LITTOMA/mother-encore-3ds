#include "encore/field_character_load.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace encore::upstream;
// Manual only. Receives the actual resource path and upstream identity from
// the caller. This source is not registered in automatic CI or test targets.
void field_character_load_format_manual(const char *path,
                                        const FieldIdentity &identity) {
  FieldCharacterLoadData data;
  std::string error;
  assert(data.load_file(path, identity, error));
  const auto admitted = data.ir_sha256();
  auto *f = std::fopen(path, "rb");
  assert(f);
  std::fseek(f, 0, SEEK_END);
  auto length = std::ftell(f);
  std::rewind(f);
  std::vector<uint8_t> bytes(static_cast<size_t>(length));
  assert(std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size());
  std::fclose(f);
  for (size_t offset :
       {size_t(0), size_t(8), size_t(12), size_t(20), size_t(24), size_t(28),
        size_t(32), size_t(36), size_t(40), size_t(60), size_t(124),
        bytes.size() - 1}) {
    auto broken = bytes;
    broken[offset] ^= 1;
    assert(!data.load(broken.data(), broken.size(), identity, error));
    assert(data.valid() && data.ir_sha256() == admitted);
  }
  assert(!data.load(bytes.data(), bytes.size() - 1, identity, error));
  assert(!data.load(nullptr, 0, identity, error));
  assert(data.valid() && data.ir_sha256() == admitted);
}
