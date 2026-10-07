#include "encore/catalog_resource_admission.hpp"
#include "encore/audio_data.hpp"
#include "encore/basement_actor_assets.hpp"
#include "encore/basement_progression.hpp"
#include "encore/battle_data.hpp"
#include "encore/battle_round_data.hpp"
#include "encore/blackbars.hpp"
#include "encore/continue_menu_data.hpp"
#include "encore/dialogue_choices_data.hpp"
#include "encore/drawer_program.hpp"
#include "encore/field_goods.hpp"
#include "encore/field_scene_destination.hpp"
#include "encore/house_return_sources.hpp"
#include "encore/field_item_admission.hpp"
#include "encore/field_programme.hpp"
#include "encore/field_psi.hpp"
#include "encore/house_button_prompts.hpp"
#include "encore/house_inspection_data.hpp"
#include "encore/introduction.hpp"
#include "encore/item_use.hpp"
#include "encore/loading_indicator_data.hpp"
#include "encore/localization.hpp"
#include "encore/music_regions.hpp"
#include "encore/native_input.hpp"
#include "encore/native_session.hpp"
#include "encore/new_game_setup.hpp"
#include "encore/phone_data.hpp"
#include "encore/present_sparkles.hpp"
#include "encore/restore_data.hpp"
#include "encore/room_music_admission.hpp"
#include "encore/save_menu_data.hpp"
#include "encore/session_migration.hpp"
#include "encore/source_font.hpp"
#include "encore/startup_resource_admission.hpp"
#include "encore/title_locale_data.hpp"
#include "encore/world_effect_data.hpp"
#include <memory>
#include <cstdio>
#include <set>
namespace encore::upstream {
namespace {
bool rejected(std::string &error, const std::string &path,
              const std::string &reason) {
  error = path + ": " + (reason.empty() ? "typed resource rejected" : reason);
  return false;
}
template <class T> bool decode(const std::string &path, std::string &error) {
  T owner;
  return owner.load_file(path.c_str(), error);
}
// These owners alone must survive the traversal, because their immutable
// views participate in actual later cross-resource binding. Large encounter
// packs and unrelated metadata are loaded one at a time and then released.
struct Owners {
  RoomData room;
  HouseData house;
  ItemData items;
  PhoneData phone;
  NativeSessionData session;
  StartupSettingsData settings;
  HouseButtonPromptData prompts;
  HouseInspectionData inspections;
  StorageData storage;
  ItemDetailsData legacy_details, field_details;
  ItemUseData item_use;
  FieldEquipmentData equipment;
  FieldItemDefinitions definitions;
  FieldInventoryData inventory;
  FieldGoodsData goods;
  BasementProgressionData basement;
  MusicRegionData music;
  AudioBank audio;
  BasementActorData basement_actors;
  PresentSparklesData sparkles;
};
} // namespace
bool admit_catalog_resource_formats(const ResourceCatalog &catalog,
                                    const char *root, std::string &error,
                                    CatalogResourceAdmissionReport *report) {
  if (!root || !*root || !catalog.valid()) {
    error = "Catalog format admission requires a valid catalog and actual root";
    return false;
  }
  if (!catalog.verify_files(root, error))
    return false;
  std::string prefix = root;
  if (prefix.back() != '/' && prefix.back() != '\\')
    prefix += '/';
  auto owners = std::make_unique<Owners>();
  CatalogResourceAdmissionReport result;
  std::set<ResourceRole> singleton_roles;
  bool restore = false;
  bool scene_door=false,scene_bundle=false;
  for (const auto &binding : catalog.binding_records()) {
    const std::string absolute = prefix + binding.path;
    std::string detail;
    bool accepted = false;
    switch (binding.role) {
    case ResourceRole::Room:
      accepted = owners->room.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::Blackbars:
      accepted = decode<Blackbars>(absolute, detail);
      break;
    case ResourceRole::Battle:
      accepted = decode<BattleData>(absolute, detail);
      break;
    case ResourceRole::Round:
      accepted = decode<BattleRoundData>(absolute, detail);
      break;
    case ResourceRole::House:
      accepted = owners->house.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::Items:
      accepted = owners->items.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::Audio:
      accepted = owners->audio.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::Phone:
      accepted = owners->phone.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::Choices:
      accepted = decode<DialogueChoicesData>(absolute, detail);
      break;
    case ResourceRole::SaveMenu:
      accepted = decode<SaveMenuData>(absolute, detail);
      break;
    case ResourceRole::Session:
      accepted = owners->session.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::Settings:
      accepted = owners->settings.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::Prompts:
      accepted = owners->prompts.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::Continue:
      accepted = decode<ContinueMenuData>(absolute, detail);
      break;
    case ResourceRole::Restore:
      // Deferred, not accepted: the real loader requires BOTH original views.
      if (restore)
        return rejected(error, binding.path, "Duplicate restore owner");
      restore = true;
      continue;
    case ResourceRole::SessionMigration:
      accepted = decode<SessionMigrationData>(absolute, detail);
      break;
    case ResourceRole::NewGame:
      accepted = decode<NewGameSetupData>(absolute, detail);
      break;
    case ResourceRole::Localization:
      accepted = decode<LocaleCatalog>(absolute, detail);
      break;
    case ResourceRole::TitleLocale:
      accepted = decode<TitleLocaleData>(absolute, detail);
      break;
    case ResourceRole::SourceFonts: {
      encore::SourceFontCatalog fonts;
      accepted = fonts.load(absolute.c_str(), detail);
      break;
    }
    case ResourceRole::Input:
      accepted = decode<NativeInputData>(absolute, detail);
      break;
    case ResourceRole::LoadingIndicator:
      accepted = decode<LoadingIndicatorData>(absolute, detail);
      break;
    case ResourceRole::EncounterBattle:
      accepted = decode<BattleData>(absolute, detail);
      break;
    case ResourceRole::EncounterRound:
      accepted = decode<BattleRoundData>(absolute, detail);
      break;
    case ResourceRole::Introduction:
      accepted = decode<IntroductionData>(absolute, detail);
      break;
    case ResourceRole::HouseInspections:
      accepted = owners->inspections.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::DrawerProgram:
      accepted = decode<DrawerProgramData>(absolute, detail);
      break;
    case ResourceRole::Storage:
      accepted = owners->storage.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::ItemDetails:
      accepted = owners->legacy_details.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::FieldEquipment:
      accepted = owners->equipment.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::ItemUse:
      accepted = owners->item_use.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::BasementProgression:
      accepted = owners->basement.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::BasementActors:
      accepted = owners->basement_actors.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::MusicRegions:
      accepted = owners->music.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::PresentSparkles:
      accepted = owners->sparkles.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::FieldPsi:
      accepted = decode<FieldPsiData>(absolute, detail);
      break;
    case ResourceRole::FieldProgrammes:
      accepted = decode<FieldProgrammeData>(absolute, detail);
      break;
    case ResourceRole::FieldInventory:
      accepted = owners->inventory.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::FieldItemDefinitions:
      accepted = owners->definitions.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::FieldItemDetails:
      accepted = owners->field_details.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::FieldGoods:
      accepted = owners->goods.load_file(absolute.c_str(), detail);
      break;
    case ResourceRole::HouseSceneDoor:
      if(scene_door)return rejected(error,binding.path,"Duplicate House Door owner");
      scene_door=true;continue;
    case ResourceRole::FieldSceneBundle:
      if(scene_bundle)return rejected(error,binding.path,"Duplicate destination owner");
      scene_bundle=true;continue;
    default:
      return rejected(error, binding.path, "Unknown catalog resource role");
    }
    if (!accepted)
      return rejected(error, binding.path, detail);
    ++result.bindings;
    if (binding.role == ResourceRole::EncounterBattle)
      ++result.encounter_battles;
    else if (binding.role == ResourceRole::EncounterRound)
      ++result.encounter_rounds;
    else {
      if (!singleton_roles.insert(binding.role).second)
        return rejected(error, binding.path, "Duplicate singleton owner");
      ++result.singleton_formats;
    }
  }
  // No catalog order assumption: Room and House views remain backed by their
  // actual owners until Restore's cross-packet SHA/rules checks finish.
  if (!restore)
    return rejected(error, catalog.path(ResourceRole::Restore),
                    "Required Restore resource absent");
  RestoreData restore_owner;
  std::string detail;
  const auto &restore_path = catalog.path(ResourceRole::Restore);
  if (!restore_owner.load_file((prefix + restore_path).c_str(),
                               owners->room.view(), owners->house.view(),
                               detail))
    return rejected(error, restore_path, detail);
  ++result.bindings;
  ++result.singleton_formats;
  if(!scene_door||!scene_bundle)return rejected(error,"destination","Required source Door/bundle absent");
  FieldSceneDestinationData destination;
  if(!destination.load(catalog,root,owners->room.view(),detail)||
     !destination.bundle().verify_files(prefix,detail))
    return rejected(error,catalog.path(ResourceRole::FieldSceneBundle),detail);
  // Load the same immutable House destination as the actual exit owner.
  // Digests alone cannot prove the NPC/geometry/Sprite source attachments.
  // Offline production admission allocates no Nodes and executes no gameplay.
  const auto *door_entry = destination.bundle().entry(PodunkPackRole::Door);
  std::vector<uint8_t> house_door_bytes;
  FieldDoorData house_doors;
  HouseReturnSources house_destination;
  if (!door_entry ||
      !destination.bundle().read(PodunkPackRole::Door, prefix, house_door_bytes, detail) ||
      !house_doors.load(house_door_bytes.data(), house_door_bytes.size(),
                        door_entry->identity, detail) ||
      !house_destination.load(destination.bundle(), prefix, house_doors,
                              owners->room.view(), owners->house.view(), detail))
    return rejected(error,catalog.path(ResourceRole::FieldSceneBundle),detail);
  result.bindings+=2;result.singleton_formats+=2;
  ResourceRole failed = ResourceRole::Room;
  const StartupResourceBindings bindings{owners->settings,
                                         owners->session,
                                         owners->prompts,
                                         owners->item_use,
                                         owners->house.view(),
                                         owners->phone.view(),
                                         owners->inspections.view(),
                                         owners->items.view(),
                                         owners->storage.view(),
                                         owners->legacy_details.view()};
  if (!admit_startup_resource_bindings(bindings, detail, &failed))
    return rejected(error, catalog.path(failed), detail);
  if (!owners->inventory.bind_definitions(owners->definitions, detail))
    return rejected(error, catalog.path(ResourceRole::FieldInventory), detail);
  if (!owners->goods.bind_inventory(owners->inventory, detail))
    return rejected(error, catalog.path(ResourceRole::FieldGoods), detail);
  HouseItemDetailsBindings field_details;
  if (!field_details.bind(owners->definitions, owners->items.view(),
                          owners->equipment.view(),
                          owners->field_details.view(), detail))
    return rejected(error, catalog.path(ResourceRole::FieldItemDetails),
                    detail);
  std::vector<AudioAsset> region_assets;
  if (!owners->music.select_assets(owners->audio, owners->audio.master_db(),
                                   MusicRegionController::maximum_voices,
                                   region_assets, detail))
    return rejected(error, catalog.path(ResourceRole::MusicRegions), detail);
  if (!admit_house_music_bindings(owners->room.view(), restore_owner,
                                  owners->basement, owners->music, detail))
    return rejected(error, catalog.path(ResourceRole::Room), detail);
  // Collect the exact texture paths that the House renderers will open,
  // independently of the staging producers' output lists. This is offline
  // resource admission only; do not add this traversal to game startup.
  std::set<std::string> house_textures;
  const auto house = owners->house.view();
  for (uint32_t i = 0; i < house.count(HouseSection::Resources); ++i)
    house_textures.emplace(house.string(house.resource(i).path));
  for (uint32_t i = 0; i < owners->room.view().resource_count(); ++i) {
    const auto resource = owners->room.view().resource(i);
    if (resource.kind == uint16_t(RoomResourceKind::Texture))
      house_textures.emplace(owners->room.view().string(resource.path_string));
  }
  for (const auto &resource : owners->basement_actors.resources()) {
    house_textures.insert(resource.path);
    if (!resource.primary_path.empty()) house_textures.insert(resource.primary_path);
  }
  house_textures.insert(owners->sparkles.texture_path());
  for (const auto &texture : house_textures) {
    FILE *file = std::fopen((prefix + texture).c_str(), "rb");
    if (!file) return rejected(error, texture, "Referenced House texture absent from actual RomFS root");
    const bool readable = std::fgetc(file) != EOF && !std::ferror(file);
    std::fclose(file);
    if (!readable) return rejected(error, texture, "Referenced House texture empty or unreadable");
  }
  // This real checked child binary is owned by Room, not a catalog role.
  // Loading its format does not start the effect animation or allocate GPU.
  auto room = owners->room.view();
  for (uint32_t i = 0; i < room.resource_count(); ++i) {
    const auto resource = room.resource(i);
    if (resource.kind != uint16_t(RoomResourceKind::CheckedWorldEffectPack))
      continue;
    const std::string path(room.string(resource.path_string));
    if (!decode<WorldEffectData>(prefix + path, detail))
      return rejected(error, path, detail);
    ++result.room_effects;
  }
  if (result.bindings != catalog.binding_records().size()) {
    error = "Catalog contains a resource that did not undergo typed format "
            "admission";
    return false;
  }
  if (report)
    *report = result;
  error.clear();
  return true;
}
} // namespace encore::upstream
