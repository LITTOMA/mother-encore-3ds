#pragma once
#include "encore/session_restore.hpp"
#include <memory>

namespace encore::upstream {
// Frozen source-derived validation resources, not a saved player profile.
// Only the reviewed rules6 -> rules7 identity transition is supported. The
// save schema and every snapshot field remain unchanged; no missing defaults,
// progress flags, positions, UIDs, currency or random state are synthesized.
class SessionMigrationData {
public:
 bool load(const uint8_t*,size_t,std::string&);
 bool load_file(const char*,std::string&);
 bool valid()const{return valid_&&house_&&round_&&items_;}
 const SessionSaveCompatibility& legacy_compatibility()const{return legacy_.compatibility();}
 const SessionSaveCompatibility& target_compatibility()const{return target_;}
 bool validate_legacy(const SessionSnapshot&,std::string&)const;
private:
 bool valid_=false;
 SessionSaveCompatibility target_;
 NativeSessionData legacy_;
 RoomData room_;
 std::unique_ptr<HouseData> house_;
 std::unique_ptr<BattleRoundData> round_;
 std::unique_ptr<ItemData> items_;
};

// Strict current decode first, otherwise exact declared legacy decode + full
// legacy domain validation, then current domain/font preparation. Output and
// optional migration indicator are committed only on success. All input bytes
// remain unchanged and prepared views borrow only CURRENT resource owners.
bool prepare_compatible_session_restore(const uint8_t*,size_t,
 const SessionMigrationData&,const NativeSessionData&,RoomView,HouseView,
 RoundView,ItemView,BattleView,PreparedSessionRestore&,std::string&,
 bool* migrated=nullptr);

// Read-only entry for slot scans/LOAD; never writes, repairs, substitutes a
// backup, or consumes random state. The legacy bundle is needed only when the
// exact current decoder rejects the file. FileOps injection is read-only.
bool read_compatible_session_restore(const char* save_path,const char* migration_path,
 const NativeSessionData&,RoomView,HouseView,RoundView,ItemView,BattleView,
 SessionSnapshot&,PreparedSessionRestore&,std::string&,
 SessionSaveFileOps* operations=nullptr,bool* migrated=nullptr);
}
