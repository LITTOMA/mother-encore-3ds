#include "encore/startup_resource_admission.hpp"

namespace encore::upstream {
bool admit_startup_resource_bindings(const StartupResourceBindings &data,
                                     std::string &error,
                                     ResourceRole *failed_role) {
  const auto failed = [&](ResourceRole role) {
    if (failed_role) *failed_role = role;
    return false;
  };
  if (!data.legacy_item_details.bind_items(data.items, error))
    return failed(ResourceRole::ItemDetails);
  if (!data.item_use.bind_items(data.items, error))
    return failed(ResourceRole::ItemUse);
  if (!data.settings.valid()) {
    error = "Startup resource settings owner incomplete";
    return failed(ResourceRole::Settings);
  }
  if (!data.session.valid()) {
    error = "Startup resource session owner incomplete";
    return failed(ResourceRole::Session);
  }
  if (data.settings.speeds != data.session.text_speeds() ||
      data.settings.flavors != data.session.menu_flavors() ||
      data.settings.prompts != data.session.button_prompts()) {
    error = "Startup UI/session setting choices disagree";
    return failed(ResourceRole::Settings);
  }
  if (!data.inspections.valid()) {
    error = "Startup resource inspection owner incomplete";
    return failed(ResourceRole::HouseInspections);
  }
  if (!data.prompts.validate_bindings(data.house, data.phone, error,
                                     data.inspections))
    return failed(ResourceRole::Prompts);
  if (!data.storage.bind_items(data.items, error))
    return failed(ResourceRole::Storage);
  error.clear();
  return true;
}
} // namespace encore::upstream
