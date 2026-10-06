#include "encore/field_dialogue_audio.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace encore::upstream;
// Manual-only format/identity and cursor admission. Never auto-registered.
int main(int argc, char **argv) {
  assert(argc == 4);
  std::string error;
  FieldIdentity identity{};
  FILE *f = std::fopen(argv[1], "rb");
  assert(f);
  std::vector<uint8_t> b(128);
  assert(std::fread(b.data(), 1, b.size(), f) == b.size());
  std::fclose(f);
  identity.scene_id = uint32_t(b[36]) | uint32_t(b[37]) << 8 |
                      uint32_t(b[38]) << 16 | uint32_t(b[39]) << 24;
  std::copy_n(b.begin() + 40, 20, identity.upstream_commit.begin());
  std::copy_n(b.begin() + 60, 32, identity.source_sha256.begin());
  FieldNodeRecipeData recipe;
  assert(recipe.load_file(argv[1], identity, error));
  FieldDialogueAudioData data;
  assert(data.load_file(argv[2], identity, error));
  assert(data.nodes().size() == 3 && !data.scene_admitted() &&
         data.recipe_sha() == recipe.ir_sha256());
  AudioBank bank;
  assert(bank.load_file(argv[3], error));
  assert(data.verify_bank(bank, error));
  f = std::fopen(argv[2], "rb");
  assert(f);
  assert(!std::fseek(f, 0, SEEK_END));
  auto count = std::ftell(f);
  assert(count > 128);
  std::rewind(f);
  b.resize(size_t(count));
  assert(std::fread(b.data(), 1, b.size(), f) == b.size());
  std::fclose(f);
  for (size_t at : {size_t(8), size_t(24), size_t(28), size_t(40), size_t(60),
                    b.size() - 1}) {
    auto changed = b;
    changed[at] ^= 1;
    FieldDialogueAudioData rejected;
    assert(!rejected.load(changed.data(), changed.size(), identity, error) &&
           !rejected.valid());
  }
  auto changed = b;
  changed.insert(changed.end(), 1);
  FieldDialogueAudioData rejected;
  assert(!rejected.load(changed.data(), changed.size(), identity, error));
  assert(!rejected.load(b.data(), 127, identity, error));
  auto wrong = identity;
  wrong.scene_id ^= 1;
  assert(!rejected.load(b.data(), b.size(), wrong, error));
  AudioBank absent;
  assert(!data.verify_bank(absent, error));
  FieldDialogueAudioRuntime runtime;
  FieldNodeTreeRuntime tree;
  SourceRandom random(1);
  assert(!runtime.initialize(data, recipe, tree, random, {}, error));
  assert(!runtime.phrase_sound(1, "unknown-original-voice", error));
  AudioFrameCursor cursor;
  assert(!cursor.seek(0));
  assert(cursor.reset(100, 20, true));
  assert(cursor.seek(75));
  assert(!cursor.seek(101) && cursor.position() == 75);
  uint32_t first = 0;
  assert(cursor.take(100, first) == 25 && first == 75);
  assert(cursor.take(100, first) == 80 && first == 20);
  assert(cursor.seek(100));
  assert(cursor.take(1, first) == 1 && first == 20);
  AudioPcmStream stream;
  assert(!stream.seek(0));
  return 0;
}
