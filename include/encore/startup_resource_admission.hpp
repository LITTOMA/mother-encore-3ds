#pragma once
#include "encore/house_button_prompts.hpp"
#include "encore/item_details.hpp"
#include "encore/item_use.hpp"
#include "encore/native_session.hpp"
#include "encore/resource_catalog.hpp"
#include "encore/startup_settings.hpp"
#include "encore/storage_data.hpp"

namespace encore::upstream {
// Borrowed checked owners; admission never changes resources, session state,
// presentation, source lifecycle, clocks or RNG. Every owner must remain alive.
struct StartupResourceBindings {
  const StartupSettingsData &settings;
  const NativeSessionData &session;
  const HouseButtonPromptData &prompts;
  const ItemUseData &item_use;
  HouseView house;
  PhoneView phone;
  HouseInspectionView inspections;
  ItemView items;
  StorageView storage;
  ItemDetailsView legacy_item_details;
};

// Shared by the console entry and offline resource publication. Checks retain
// their first-occurrence order from the actual console metadata bindings:
// descriptions, field Use, startup choices, prompts, then storage. Success
// clears error; rejection reports the first binding failure without mutation.
// Optional failed_role is diagnostic only, written on rejection and untouched
// on success. It identifies the resource whose actual binding check rejected.
bool admit_startup_resource_bindings(const StartupResourceBindings &,
                                     std::string &error,
                                     ResourceRole *failed_role = nullptr);
} // namespace encore::upstream
