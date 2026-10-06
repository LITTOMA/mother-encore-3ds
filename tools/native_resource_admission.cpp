// Resource publication uses the actual shared typed loaders and bindings.
// This does not enter scenes, execute gameplay, render or advance RNG.
#include "encore/catalog_resource_admission.hpp"
#include "encore/field_item_definitions.hpp"
#include <iostream>

int main(int argc, char **argv) {
  using namespace encore::upstream;
  if (argc != 2 && (argc != 4 || std::string(argv[2]) != "--global-items")) return 2;
  const std::string root = std::string(argv[1]) + "/";
  std::string error;
  ResourceCatalog catalog;
  CatalogResourceAdmissionReport report;
  if (!catalog.load_file((root + "data/native.encresources").c_str(), error) ||
      !admit_catalog_resource_formats(catalog, root.c_str(), error, &report)) {
    std::cerr << error << '\n';
    return 1;
  }
  // The full Items constructor resource is a development dependency until
  // the complete globalData lifecycle is connected. Validate its real bytes
  // offline without adding it to the shipping catalog or startup work.
  if (argc == 4) {
    FieldItemDefinitions global_items;
    if (!global_items.load_file(argv[3], error)) {
      std::cerr << "Global Items constructor resource: " << error << '\n';
      return 1;
    }
    if (!global_items.global_constructor_scope()) {
      error = "Expected global constructor capability";
      std::cerr << "Global Items constructor resource: " << error << '\n';
      return 1;
    }
    std::cout << "Global Items constructor format admitted: "
              << global_items.definitions().size() << " definitions\n";
  }
  std::cout << "Runtime binary admission: " << report.bindings << " bindings, "
            << report.singleton_formats << " startup singletons, "
            << report.encounter_battles + report.encounter_rounds
            << " encounter resources, " << report.room_effects
            << " Room effect resources; shared startup and music bindings\n";
}
