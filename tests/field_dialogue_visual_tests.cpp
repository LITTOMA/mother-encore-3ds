#include "encore/field_dialogue_visual.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace encore::upstream;
// Manual-only source admission cases. Not registered or executed by default.
int main(int argc, char **argv) {
  assert(argc == 3);
  FieldIdentity identity{};
  FieldNodeRecipeData recipe;
  // Caller supplies an already checked identity: derive it from a separately
  // checked recipe header only for these manual negative-format source cases.
  FILE *f = std::fopen(argv[1], "rb");
  assert(f);
  std::vector<uint8_t> b(128);
  assert(std::fread(b.data(), 1, b.size(), f) == b.size());
  std::fclose(f);
  identity.scene_id = uint32_t(b[36]) | uint32_t(b[37]) << 8 |
                      uint32_t(b[38]) << 16 | uint32_t(b[39]) << 24;
  std::copy_n(b.begin() + 40, 20, identity.upstream_commit.begin());
  std::copy_n(b.begin() + 60, 32, identity.source_sha256.begin());
  std::string error;
  assert(recipe.load_file(argv[1], identity, error));
  FieldDialogueVisualData data;
  assert(data.load_file(argv[2], identity, error));
  assert(data.nodes().size() == 20 && data.cursors().size() == 2 &&
         !data.scene_admitted());
  assert(data.recipe_sha() == recipe.ir_sha256());
  f = std::fopen(argv[2], "rb");
  assert(f);
  std::fseek(f, 0, SEEK_END);
  auto count = std::ftell(f);
  assert(count > 128);
  std::rewind(f);
  b.resize(size_t(count));
  assert(std::fread(b.data(), 1, b.size(), f) == b.size());
  std::fclose(f);
  for (size_t position : {size_t(8), size_t(24), size_t(28), size_t(40),
                          size_t(60), b.size() - 1}) {
    auto changed = b;
    changed[position] ^= 1;
    FieldDialogueVisualData rejected;
    assert(!rejected.load(changed.data(), changed.size(), identity, error));
    assert(!rejected.valid());
  }
  assert(!data.load(b.data(), 127, identity, error));
  auto wrong = identity;
  wrong.scene_id ^= 1;
  assert(!data.load(b.data(), b.size(), wrong, error));
  FieldDialogueVisualRuntime runtime;
  FieldNodeTreeRuntime tree;
  SourceRandom random(1);
  assert(!runtime.initialize(data, recipe, tree, {}, random, {}, {}, error));
  return 0;
}
