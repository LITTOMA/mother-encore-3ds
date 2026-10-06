#pragma once
#include "encore/resource_catalog.hpp"
namespace encore::upstream {
struct CatalogResourceAdmissionReport {
  size_t bindings = 0, singleton_formats = 0, encounter_battles = 0,
         encounter_rounds = 0, room_effects = 0;
};
// Actual catalog records -> their existing typed CPU loaders and common
// startup binding helper. All file identities are verified before decoding.
// No source constructor/Ready, gameplay, random, NDSP or GPU is executed.
// Restore borrows the real decoded Room/House owners while it is admitted.
// Report is published only after every resource and binding succeeds.
bool admit_catalog_resource_formats(
    const ResourceCatalog &, const char *root, std::string &error,
    CatalogResourceAdmissionReport *report = nullptr);
} // namespace encore::upstream
