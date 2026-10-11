#pragma once
#include "encore/battle_round.hpp"
#include "encore/house_data.hpp"
#include "encore/items_menu.hpp"
#include "encore/room_data.hpp"
#include "encore/session_save.hpp"
#include "encore/storage_menu.hpp"
#include <array>

namespace encore::upstream {
// A bounded save bridge, not a scene-restoration engine. All identities, initial
// values, supported level rows and stat/skill rules come from ENCNSESS data.
struct NativeSessionLevel {
    uint32_t level=0, minimum_exp=0, next_exp=0;
    std::array<int32_t,7> stats{}; // RoundStat order, including equipped boosts.
    std::vector<std::string> skills;
};
struct NativeSessionAcquisition {
    std::string item_id,flag_id;
    uint32_t doses=0,max_count=0;
};
struct NativeSessionStoragePolicy {
    std::string item_id;uint32_t doses=0,total_count=0;bool required=false;
    std::array<int32_t,7> boosts{};
};
// Schema 6: key items granted by a source flag. The item is held exactly while
// flag_id is true and consumed_flag_id (if any) is false, after the defaults.
struct NativeSessionKeyAcquisition {
    std::string item_id,flag_id,consumed_flag_id;
    uint32_t doses=0;
};
class NativeSessionData {
public:
    bool load(const uint8_t*,size_t,std::string&);
    bool load_file(const char*,std::string&);
    bool valid()const{return valid_;}
    const SessionSnapshot& defaults()const{return defaults_;}
    // Complete source naming order for a detached New Game candidate. Inactive
    // records are data only, never active party members. Template item UIDs are
    // reviewed placeholders; New Game stages source UIDs on copied RNG state
    // and commits those IDs only with accepted naming.
    // Legacy ENCNSESS v1 resources expose only their unchanged singleton here.
    const std::vector<SessionCharacter>& startup_characters()const{return startup_characters_;}
    const SessionSaveCompatibility& compatibility()const{return compatibility_;}
    const std::vector<double>& text_speeds()const{return text_speeds_;}
    const std::vector<std::string>& menu_flavors()const{return menu_flavors_;}
    const std::vector<std::string>& button_prompts()const{return button_prompts_;}
    bool supports_settings(const SessionSettings&)const;
    const std::string& leader_id()const{return defaults_.party.at(0);}
    const std::string& saved_flag_id()const{return saved_flag_;}
    const std::string& earned_cash_flag_id()const{return earned_cash_flag_;}
    const std::vector<NativeSessionLevel>& levels()const{return levels_;}
    const std::vector<std::string>& mutable_flags()const{return mutable_flags_;}
    const std::vector<uint32_t>& camera_area_ids()const{return camera_area_ids_;}
    const std::vector<NativeSessionAcquisition>& acquisitions()const{return acquisitions_;}
    uint32_t storage_capacity()const{return storage_capacity_;}
    const std::vector<NativeSessionStoragePolicy>& storage_policies()const{return storage_policies_;}
    const std::vector<NativeSessionKeyAcquisition>& key_acquisitions()const{return key_acquisitions_;}
private:
    std::vector<double> text_speeds_;
    std::vector<std::string> menu_flavors_,button_prompts_;
    bool valid_=false;
    SessionSaveCompatibility compatibility_;
    SessionSnapshot defaults_;
    std::vector<SessionCharacter> startup_characters_;
    std::vector<NativeSessionLevel> levels_;
    std::string saved_flag_,earned_cash_flag_;
    std::vector<std::string>mutable_flags_;
    std::vector<uint32_t>camera_area_ids_;
    std::vector<NativeSessionAcquisition>acquisitions_;
    uint32_t storage_capacity_=0;std::vector<NativeSessionStoragePolicy>storage_policies_;
    std::vector<NativeSessionKeyAcquisition>key_acquisitions_;
};
// Expected key inventory identities for the flags in s (defaults, then held
// schema-6 acquisitions in policy order). UIDs are not part of identity.
bool native_session_expected_key_items(const NativeSessionData&,const SessionSnapshot&,std::vector<SessionItem>&,std::string&);

struct NativeSnapshotInput {
    // Initialize once from data.defaults(). The caller owns and updates all
    // world/session fields, including seen/encountered registries and playtime.
    // No NPC transforms, suspended dialogue/menu or RNG state belongs here.
    SessionSnapshot state;
    const BattleSessionStats* stats=nullptr;
    const InventoryState* inventory=nullptr;
    const StorageState* storage=nullptr;
};
// Validates a decoded candidate's complete supported content scope. This does
// not apply it to a world. Settings/modifiers/items beyond this slice fail closed.
bool validate_native_session_snapshot(const NativeSessionData&,RoomView,HouseView,
    RoundView,ItemView,const SessionSnapshot&,std::string&);
// Copies the explicit input; overlays live combat stats/currency/inventory;
// validates before committing output. The live saved flag is never changed.
bool build_native_session_snapshot(const NativeSessionData&,RoomView,HouseView,
    RoundView,ItemView,const NativeSnapshotInput&,SessionSnapshot&,std::string&);
// Resource rows retain the historical initial equipment baseline. This checked
// calculation subtracts that baseline and adds only current admitted equipment.
bool native_session_derived_stats(const NativeSessionData&,ItemView,
    const SessionCharacter&,std::array<int32_t,7>&,std::string&);
}
