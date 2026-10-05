#pragma once
#include "encore/native_session.hpp"
#include <set>

namespace encore::upstream {
// A checked, detached candidate for constructing a fresh scene. The complete
// snapshot remains owned here; derived fields contain only resolved bindings.
// ItemView borrows its original checked ItemData owner, as in InventoryState.
// This does not apply scene state or consume/reseed the shared random stream.
struct PreparedSessionRestore {
    SessionSnapshot state;
    BattleSessionStats stats;
    InventoryState inventory;
    StorageState storage;
    std::vector<bool> reviewed_flag_mutations; // External NativeSession mutation scope.
    std::vector<bool> story_flags; // Room flag index order, including false values.
    std::set<uint32_t> seen_dialogue_keys; // House string offsets, true values only.
    Vec2 position{}, direction{};
    bool valid()const{return valid_;}
private:
    bool valid_=false;
    friend bool prepare_session_restore(const NativeSessionData&,RoomView,
        HouseView,RoundView,ItemView,BattleView,const SessionSnapshot&,
        PreparedSessionRestore&,std::string&);
};

// Validates structural/domain scope, derived stats, identity bindings and every
// dynamic display name against the checked font. Current byte-oriented text
// renderers support printable ASCII only; other codepoints fail explicitly.
// Failure preserves output. Success retains all saved fields exactly, including
// UID/equipment/doses, nickname/settings/playtime, false seen flags and direction.
bool prepare_session_restore(const NativeSessionData&,RoomView,HouseView,
    RoundView,ItemView,BattleView font,const SessionSnapshot&,
    PreparedSessionRestore& output,std::string& error);
}
