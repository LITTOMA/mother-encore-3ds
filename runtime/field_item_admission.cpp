#include "encore/field_item_admission.hpp"
#include <set>

namespace encore::upstream {
bool admit_house_item_definitions(const FieldItemDefinitions &source,
                                 ItemView items, FieldEquipmentView equipment,
                                 std::string &error) {
  auto fail = [&](const char *message) { error = message; return false; };
  if (!source.valid() || !items.valid() || !equipment.valid() ||
      !equipment.bind_items(items, error))
    return fail("House original item resource owners incomplete");
  const auto pin = items.reviewed_commit();
  if (pin.size() != source.source_pin().size() * 2 ||
      pin != equipment.reviewed_commit())
    return fail("House original item source pin differs");
  for (size_t i = 0; i < source.source_pin().size(); ++i) {
    const char hex[] = "0123456789abcdef";
    if (pin[i * 2] != hex[source.source_pin()[i] >> 4] ||
        pin[i * 2 + 1] != hex[source.source_pin()[i] & 15])
      return fail("House and full inventory source pins differ");
  }
  if (items.metadata().capacity != source.capacity(0))
    return fail("House and original member inventory capacities differ");
  std::set<uint32_t> seen;
  for (uint32_t i = 0; i < items.count(ItemSection::Definitions); ++i) {
    const auto item = items.definition(i);
    // Domain 1 is the existing ordinary House item compatibility namespace.
    const auto *definition = source.legacy(1, item.id);
    if (!definition || !seen.insert(definition->id).second ||
        definition->keyitem() ||
        definition->item_name != items.string(item.source) ||
        definition->heal_hp != item.heal_hp ||
        definition->heal_pp != item.heal_pp ||
        definition->boost[0] != item.max_hp_boost ||
        definition->boost[1] != item.max_pp_boost ||
        bool(item.flags & uint32_t(ItemDefinitionFlag::Equipment)) !=
            !definition->slot.empty())
      return fail("House item compatibility binding differs from original definition");
    if (!definition->slot.empty()) {
      if (item.equipment_slot >= equipment.count(FieldSection::Slots) ||
          equipment.string(equipment.slot(item.equipment_slot).source) !=
              definition->slot)
        return fail("House original equipment slot differs");
      bool found = false;
      for (uint32_t j = 0; j < equipment.count(FieldSection::Equipment); ++j) {
        const auto policy = equipment.equipment(j);
        if (policy.definition != i) continue;
        if (found || policy.slot != item.equipment_slot ||
            equipment.string(policy.source) != definition->item_name ||
            policy.boosts != definition->boost)
          return fail("House equipment boosts differ from original definition");
        found = true;
      }
      if (!found) return fail("House equipment original policy missing");
    }
  }
  for (const auto &definition : source.definitions())
    if (definition.legacy_domain == 1 && !seen.count(definition.id))
      return fail("House ordinary item compatibility coverage incomplete");
  error.clear();
  return true;
}
bool HouseItemDetailsBindings::bind(const FieldItemDefinitions &source,
                                   ItemView items, FieldEquipmentView equipment,
                                   ItemDetailsView details, std::string &error) {
  if (!admit_house_item_definitions(source, items, equipment, error) ||
      !details.bind_field_items(source, error))
    return false;
  std::vector<uint32_t> candidate;
  for (uint32_t i = 0; i < items.count(ItemSection::Definitions); ++i) {
    const auto *definition = source.legacy(1, items.definition(i).id);
    // The complete resource validates all original definitions. Here only its
    // existing House compatibility namespace is exposed to legacy consumers.
    if (!definition) {
      error = "House description original identity unavailable";
      return false;
    }
    candidate.push_back(definition->id);
  }
  definitions_.swap(candidate);
  details_ = details;
  error.clear();
  return true;
}
bool HouseItemDetailsBindings::source_definition(uint32_t house_index,
                                               uint32_t &source_id,
                                               std::string &error) const {
  if (!details_.valid() || house_index >= definitions_.size()) {
    error = "House description definition index rejected";
    return false;
  }
  source_id = definitions_[house_index];
  error.clear();
  return true;
}
}
