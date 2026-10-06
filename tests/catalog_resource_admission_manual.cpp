// Manual admission harness: never registered in automatic development builds.
#include "encore/catalog_resource_admission.hpp"
#include <iostream>
#include <string>
using namespace encore::upstream;
int main(int argc, char **argv) {
  if (argc != 3 && argc != 4)
    return 2;
  ResourceCatalog catalog;
  std::string error;
  if (!catalog.load_file(argv[1], error)) {
    std::cerr << error << '\n';
    return 3;
  }
  CatalogResourceAdmissionReport report;
  report.bindings = 777;
  if (argc == 4) {
    if (!catalog.verify_files(argv[2], error)) {
      std::cerr << error << '\n';
      return 4;
    }
    if (admit_catalog_resource_formats(catalog, argv[2], error, &report) ||
        error.find(argv[3]) == std::string::npos || report.bindings != 777) {
      std::cerr << "Typed rejection/path/atomic report failed: " << error
                << '\n';
      return 5;
    }
    return 0;
  }
  ResourceCatalog missing;
  if (admit_catalog_resource_formats(missing, argv[2], error, &report) ||
      report.bindings != 777)
    return 6;
  if (admit_catalog_resource_formats(catalog, nullptr, error, &report) ||
      report.bindings != 777)
    return 7;
  if (admit_catalog_resource_formats(catalog, "", error, &report) ||
      report.bindings != 777)
    return 8;
  if (!admit_catalog_resource_formats(catalog, argv[2], error, &report)) {
    std::cerr << error << '\n';
    return 9;
  }
  size_t singles = 0, battles = 0, rounds = 0;
  for (const auto &binding : catalog.binding_records()) {
    if (binding.role == ResourceRole::EncounterBattle)
      ++battles;
    else if (binding.role == ResourceRole::EncounterRound)
      ++rounds;
    else
      ++singles;
  }
  if (report.bindings != catalog.binding_records().size() ||
      report.singleton_formats != singles ||
      report.encounter_battles != battles || report.encounter_rounds != rounds)
    return 10;
  return 0;
}
