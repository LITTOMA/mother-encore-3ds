#pragma once
#include "encore/field_item_definitions.hpp"
#include "encore/field_equipment_data.hpp"
#include "encore/item_details.hpp"

namespace encore::upstream {
// Admit the existing House adapter against the same original definitions used
// by the full source inventory. This does not convert owners, UIDs or saves.
bool admit_house_item_definitions(const FieldItemDefinitions &, ItemView,
                                 FieldEquipmentView, std::string &);
// Map the existing House definition indices to the original stable identities.
// Inventory UIDs and saved identities are never translated or reconstructed.
class HouseItemDetailsBindings {
public:
  bool bind(const FieldItemDefinitions &, ItemView, FieldEquipmentView,
            ItemDetailsView, std::string &);
  bool matches(ItemDetailsView details) const {
    return !definitions_.empty() && details_.same_content(details);
  }
  bool source_definition(uint32_t house_index, uint32_t &source_id,
                         std::string &) const;
private:
  ItemDetailsView details_;
  std::vector<uint32_t> definitions_;
};
}
