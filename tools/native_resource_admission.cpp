// Resource publication uses the actual shared typed loaders and bindings.
// This does not enter scenes, execute gameplay, render or advance RNG.
#include "encore/catalog_resource_admission.hpp"
#include <iostream>

int main(int argc, char **argv) {
  using namespace encore::upstream;
  if (argc != 2) return 2;
  const std::string root = std::string(argv[1]) + "/";
  std::string error;
  ResourceCatalog catalog;
  CatalogResourceAdmissionReport report;
  if (!catalog.load_file((root + "data/native.encresources").c_str(), error) ||
      !admit_catalog_resource_formats(catalog, root.c_str(), error, &report)) {
    std::cerr << error << '\n';
    return 1;
  }
  std::cout << "Runtime binary admission: " << report.bindings << " bindings, "
            << report.singleton_formats << " startup singletons, "
            << report.encounter_battles + report.encounter_rounds
            << " encounter resources, " << report.room_effects
            << " Room effect resources; shared startup and music bindings\n";
}
