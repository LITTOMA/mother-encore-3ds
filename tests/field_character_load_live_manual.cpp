#include "encore/field_character_load.hpp"
#include <cassert>
#include <limits>
using namespace encore::upstream;

// Manual only: the caller supplies the genuinely constructed globalData
// singleton and source resource. No replacement objects or source Ready.
void field_character_load_live_manual(const FieldCharacterLoadData &data,
                                      FieldGlobalDataRuntime &core) {
  std::string error;
  if (!core.character_load_bound_to(data))
    assert(core.initialize_character_load(data, error));
  assert(!data.rows().empty());
  const auto declaration = data.rows().front().id;
  FieldCharacterLoadState before, after;
  assert(core.read_character_load(declaration, before, error));
  auto bad = before;
  bad.write = FieldCharacterLoadWrite::Name;
  bad.object = std::numeric_limits<FieldObjectId>::max();
  assert(!core.publish_character_load(bad, error));
  bad = before;
  bad.write = FieldCharacterLoadWrite::None;
  assert(!core.publish_character_load(bad, error));
  bad = before;
  bad.write = FieldCharacterLoadWrite::Stat;
  bad.stat_index = bad.stats.size();
  assert(!core.publish_character_load(bad, error));
  bad.stat_index = 0;
  bad.stats.front() = -1;
  assert(!core.publish_character_load(bad, error));
  assert(core.read_character_load(declaration, after, error));
  assert(before.object == after.object && before.name == after.name &&
         before.stats == after.stats &&
         before.inventory.object == after.inventory.object);
  assert(!core.load_complete());
}

// Call after actual eight-character LOAD. These getters inspect the same
// source bodies; they do not rerun LOAD, setters, signals or randomness.
void field_character_load_live_publication_manual(
    const FieldCharacterLoadData &data, const FieldGlobalDataRuntime &core) {
  std::string error;
  for (const auto &row : data.rows()) {
    FieldCharacterLoadState live;
    assert(core.read_character_load(row.id, live, error));
    assert(live.name == row.display_name);
    if (row.role == 0) {
      FieldGlobalDataMemberState backing, getter, skills;
      assert(core.read_constructed_member(
          live.object, data.source_bindings().inventory, backing, error));
      assert(core.read_constructed_member(
          live.object, data.source_bindings().inventory_getter, getter, error));
      assert(backing.references == getter.references &&
             !backing.references.empty());
      assert(backing.references.front().second == live.inventory.object);
      assert(core.read_constructed_member(
          live.object, data.source_bindings().learned_skills, skills, error));
      assert(skills.value && skills.value->array.size() == live.skills.size());
    }
  }
  assert(!core.load_complete());
}
