// Manual only, using real generated House data; no Ready or animation clock.
#include "encore/field_sprite_bridge.hpp"
#include "encore/field_npc.hpp"
#include "encore/crc32.hpp"
#include "manual_require.hpp"
#include <algorithm>

using namespace encore::upstream;
namespace {
void sprite_word(std::vector<uint8_t> &b, size_t at, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) b.at(at + i) = uint8_t(value >> (8 * i));
}
void sprite_crc(std::vector<uint8_t> &b) {
  sprite_word(b, 16, 0); sprite_word(b, 16, encore::crc32(b.data(), b.size()));
}
}

void house_return_sprite_resource_manual(const std::vector<uint8_t> &bytes,
                                         const FieldNpcData &npcs) {
  std::string error;
  FieldSpriteData sprites;
  MANUAL_REQUIRE(sprites.load(bytes.data(), bytes.size(), error));
  MANUAL_REQUIRE(sprites.records().size() == 16 && npcs.npcs().size() == 6);
  MANUAL_REQUIRE(field_sprite_npc_binding(sprites, npcs, error));
  const auto *kept = sprites.record(sprites.records().front().id);
  auto rejected = [&](std::vector<uint8_t> bad) {
    FieldSpriteData empty;
    MANUAL_REQUIRE(!empty.load(bad.data(), bad.size(), error) && !empty.valid());
    MANUAL_REQUIRE(!sprites.load(bad.data(), bad.size(), error));
    MANUAL_REQUIRE(sprites.valid() && sprites.record(kept->id) == kept);
  };
  for (size_t at : {size_t(8), size_t(20), size_t(24), size_t(28),
                    size_t(56), size_t(60)}) {
    auto bad = bytes; sprite_word(bad, at, UINT32_MAX); sprite_crc(bad); rejected(bad);
  }
  for (size_t end : {size_t(63), bytes.size() - 1}) {
    auto bad = bytes; bad.resize(end); rejected(bad);
  }
  // Both header changes remain parseable; the same House NPC borrow rejects.
  for (size_t at : {size_t(32), size_t(52)}) {
    auto bad = bytes; bad.at(at) ^= 1; sprite_crc(bad);
    FieldSpriteData foreign;
    MANUAL_REQUIRE(foreign.load(bad.data(), bad.size(), error));
    MANUAL_REQUIRE(!field_sprite_npc_binding(foreign, npcs, error));
  }
  const auto &first = sprites.records().front();
  MANUAL_REQUIRE(first.kind == FieldSpriteKind::Character);
  std::vector<uint8_t> prefix(16);
  sprite_word(prefix, 0, first.id); sprite_word(prefix, 4, first.parent_id);
  sprite_word(prefix, 8, first.ready_ordinal); sprite_word(prefix, 12, uint32_t(first.kind));
  const auto at = std::search(bytes.begin() + 64, bytes.end(), prefix.begin(), prefix.end());
  MANUAL_REQUIRE(at != bytes.end());
  const size_t record = size_t(at - bytes.begin());
  MANUAL_REQUIRE(std::search(at + 16, bytes.end(), prefix.begin(), prefix.end()) == bytes.end());
  for (size_t offset : {size_t(8), size_t(16), size_t(48), size_t(52)}) {
    auto bad = bytes; sprite_word(bad, record + offset, UINT32_MAX);
    sprite_crc(bad); rejected(bad);
  }
  // A structurally valid wrong parent cannot bind a substitute NPC.
  auto wrong = bytes; sprite_word(wrong, record + 4, first.parent_id ^ 1u); sprite_crc(wrong);
  FieldSpriteData foreign;
  MANUAL_REQUIRE(foreign.load(wrong.data(), wrong.size(), error));
  MANUAL_REQUIRE(!field_sprite_npc_binding(foreign, npcs, error));
}
