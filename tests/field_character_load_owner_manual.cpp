#include "encore/field_character_load.hpp"
#include <cassert>
using namespace encore::upstream;
// Deliberately no stand-in objects/fixtures or automatic execution. A caller
// with real constructed owners can execute this before the accepted LOAD
// cursor.
void field_character_load_owner_manual(
    const FieldCharacterLoadData &data, const FieldGlobalDataData &owner_data,
    const FieldGlobalDataRuntime &owner, FieldGlobalRegistry &registry,
    GlobalYamlCachesRuntime &caches, GlobalItemCache &items,
    SourceRandom &random, std::vector<uint32_t> &ledger,
    LoadRngClockProvider clock, const FieldCharacterLoadHost &host) {
  auto before = ledger;
  auto missing = host;
  missing.publish = {};
  FieldCharacterLoadRuntime runtime;
  std::string error;
  assert(!runtime.initialize(data, owner_data, owner, registry, caches, items,
                             random, ledger, clock, missing, error));
  assert(ledger == before && !runtime.characters_complete());
  missing = host;
  missing.new_enemy_skill = {};
  assert(!runtime.initialize(data, owner_data, owner, registry, caches, items,
                             random, ledger, clock, missing, error));
  assert(ledger == before);
  assert(!runtime.load_cold_default(error));
}
