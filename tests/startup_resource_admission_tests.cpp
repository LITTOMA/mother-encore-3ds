// Manual resource-binding regression; normal development never executes it.
// argv[1] is a real generated RomFS root, not a gameplay fixture.
#include "encore/resource_catalog.hpp"
#include "encore/startup_resource_admission.hpp"
#include <cstdlib>
#include <iostream>

using namespace encore::upstream;
namespace {
void check(bool value, const std::string &message) {
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}
} // namespace

int main(int argc, char **argv) {
  check(argc == 2, "Actual generated RomFS root required");
  const std::string root = std::string(argv[1]) + "/";
  std::string error;
  ResourceCatalog catalog;
  check(catalog.load_file((root + "data/native.encresources").c_str(), error),
        error);
  const auto path = [&](ResourceRole role) {
    check(!catalog.path(role).empty(), "Required checked role missing");
    return root + catalog.path(role);
  };
  StartupSettingsData settings;
  NativeSessionData session;
  HouseButtonPromptData prompts;
  ItemUseData use;
  HouseData house;
  PhoneData phone;
  HouseInspectionData inspections;
  ItemData items;
  StorageData storage;
  ItemDetailsData details;
  check(settings.load_file(path(ResourceRole::Settings).c_str(), error), error);
  check(session.load_file(path(ResourceRole::Session).c_str(), error), error);
  check(prompts.load_file(path(ResourceRole::Prompts).c_str(), error), error);
  check(use.load_file(path(ResourceRole::ItemUse).c_str(), error), error);
  check(house.load_file(path(ResourceRole::House).c_str(), error), error);
  check(phone.load_file(path(ResourceRole::Phone).c_str(), error), error);
  check(inspections.load_file(path(ResourceRole::HouseInspections).c_str(), error),
        error);
  check(items.load_file(path(ResourceRole::Items).c_str(), error), error);
  check(storage.load_file(path(ResourceRole::Storage).c_str(), error), error);
  check(details.load_file(path(ResourceRole::ItemDetails).c_str(), error), error);
  const StartupResourceBindings baseline{
      settings, session, prompts, use, house.view(), phone.view(),
      inspections.view(), items.view(), storage.view(), details.view()};
  error = "stale diagnostic";
  ResourceRole diagnostic = ResourceRole::Room;
  check(admit_startup_resource_bindings(baseline, error, &diagnostic) &&
            error.empty() && diagnostic == ResourceRole::Room,
        "Real source startup bindings rejected: " + error);
  const auto speeds = settings.speeds;
  const auto flavors = settings.flavors;
  const auto prompt_modes = settings.prompts;
  const auto default_character_count = session.defaults().characters.size();
  const auto prompt_target_count = prompts.targets.size();
  const auto rejected = [&](const StartupResourceBindings &candidate,
                            ResourceRole expected) {
    error.clear();
    ResourceRole rejected_role = ResourceRole::Room;
    check(!admit_startup_resource_bindings(candidate, error, &rejected_role) &&
              !error.empty() && rejected_role == expected,
          "Unbound/mismatched startup metadata accepted");
    check(settings.speeds == speeds && settings.flavors == flavors &&
              settings.prompts == prompt_modes &&
              session.defaults().characters.size() == default_character_count &&
              prompts.targets.size() == prompt_target_count &&
              storage.view().same_content(baseline.storage) &&
              details.view().same_content(baseline.legacy_item_details),
          "Rejected candidate changed admitted source owners");
    check(admit_startup_resource_bindings(baseline, error),
          "Rejected candidate invalidated prior binding: " + error);
  };
  {
    auto candidate = baseline;
    candidate.legacy_item_details = ItemDetailsView{};
    rejected(candidate, ResourceRole::ItemDetails);
  }
  {
    auto candidate = baseline;
    candidate.items = ItemView{};
    rejected(candidate, ResourceRole::ItemDetails);
  }
  {
    auto candidate = baseline;
    candidate.house = HouseView{};
    rejected(candidate, ResourceRole::Prompts);
  }
  {
    auto candidate = baseline;
    candidate.phone = PhoneView{};
    rejected(candidate, ResourceRole::Prompts);
  }
  {
    auto candidate = baseline;
    candidate.inspections = HouseInspectionView{};
    rejected(candidate, ResourceRole::HouseInspections);
  }
  {
    auto candidate = baseline;
    candidate.storage = StorageView{};
    rejected(candidate, ResourceRole::Storage);
  }
  const auto with_settings = [&](const StartupSettingsData &candidate) {
    return StartupResourceBindings{
        candidate, session, prompts, use, house.view(), phone.view(),
        inspections.view(), items.view(), storage.view(), details.view()};
  };
  StartupSettingsData absent_settings;
  rejected(with_settings(absent_settings), ResourceRole::Settings);
  check(!settings.speeds.empty() && !settings.flavors.empty() &&
            !settings.prompts.empty() && !prompts.targets.empty(),
        "Real startup source choices/targets missing");
  {
    auto candidate = settings;
    candidate.speeds.front() += 1;
    rejected(with_settings(candidate), ResourceRole::Settings);
  }
  {
    auto candidate = settings;
    candidate.flavors.front() += "unknown-manual-choice";
    rejected(with_settings(candidate), ResourceRole::Settings);
  }
  {
    auto candidate = settings;
    candidate.prompts.front() += "unknown-manual-choice";
    rejected(with_settings(candidate), ResourceRole::Settings);
  }
  NativeSessionData absent_session;
  rejected({settings, absent_session, prompts, use, house.view(), phone.view(),
            inspections.view(), items.view(), storage.view(), details.view()},
           ResourceRole::Session);
  ItemUseData absent_use;
  rejected({settings, session, prompts, absent_use, house.view(), phone.view(),
            inspections.view(), items.view(), storage.view(), details.view()},
           ResourceRole::ItemUse);
  HouseButtonPromptData absent_prompts;
  rejected({settings, session, absent_prompts, use, house.view(), phone.view(),
            inspections.view(), items.view(), storage.view(), details.view()},
           ResourceRole::Prompts);
  {
    auto candidate = prompts;
    candidate.targets.front().center.x += 1;
    rejected({settings, session, candidate, use, house.view(), phone.view(),
              inspections.view(), items.view(), storage.view(), details.view()},
             ResourceRole::Prompts);
  }
  check(admit_startup_resource_bindings(baseline, error) && error.empty(),
        "Final real source startup binding rejected: " + error);
  return 0;
}
