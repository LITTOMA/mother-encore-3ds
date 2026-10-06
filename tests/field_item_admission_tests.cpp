// Manual consumer regression; normal builds do not execute this file.
#include "encore/field_item_admission.hpp"
#include <cstdlib>
#include <iostream>
using namespace encore::upstream;
static void check(bool value, const std::string &message) {
  if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
int main(int argc, char **argv) {
  check(argc == 5, "original definitions, House Items, equipment and full details required");
  std::string error;
  FieldItemDefinitions definitions, missing;
  ItemData items;
  FieldEquipmentData equipment;
  ItemDetailsData details;
  check(definitions.load_file(argv[1], error), error);
  check(items.load_file(argv[2], error), error);
  check(equipment.load_file(argv[3], error), error);
  check(details.load_file(argv[4], error), error);
  check(admit_house_item_definitions(definitions, items.view(), equipment.view(), error), error);
  check(!admit_house_item_definitions(missing, items.view(), equipment.view(), error), "missing definitions accepted");
  check(!admit_house_item_definitions(definitions, ItemView{}, equipment.view(), error), "missing House Items accepted");
  check(!admit_house_item_definitions(definitions, items.view(), FieldEquipmentView{}, error), "missing equipment accepted");
  check(admit_house_item_definitions(definitions, items.view(), equipment.view(), error), error);
  HouseItemDetailsBindings bridge;
  check(bridge.bind(definitions, items.view(), equipment.view(), details.view(), error), error);
  check(bridge.matches(details.view()), "bridge borrowed another details owner");
  check(!bridge.matches(ItemDetailsView{}), "missing details owner matched");
  for (uint32_t i = 0; i < items.view().count(ItemSection::Definitions); ++i) {
    uint32_t original = 0;
    check(bridge.source_definition(i, original, error), error);
    const auto *definition = definitions.legacy(1, items.view().definition(i).id);
    check(definition && original == definition->id,
          "House index did not resolve original stable identity");
  }
  uint32_t unchanged = 123;
  check(!bridge.source_definition(items.view().count(ItemSection::Definitions), unchanged, error) && unchanged == 123,
        "invalid House index changed output identity");
  check(!bridge.bind(definitions, items.view(), equipment.view(), ItemDetailsView{}, error) && bridge.matches(details.view()),
        "failed full details binding discarded prior mapping");
  check(!bridge.bind(missing, items.view(), equipment.view(), details.view(), error) && bridge.matches(details.view()),
        "failed source binding discarded prior mapping");
}
