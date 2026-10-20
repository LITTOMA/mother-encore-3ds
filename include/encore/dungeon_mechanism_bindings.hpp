#pragma once
#include "encore/dungeon_switches.hpp"
#include "encore/dungeon_wall_timeline.hpp"
#include <memory>

namespace encore::upstream {
enum class DungeonMechanismFamily:uint32_t {Lever=1,Plate=2,Gate=3,Wall=4};
enum class DungeonMechanismOwner:uint32_t {Switch=1,Controlled=2};
struct DungeonMechanismBinding {
 uint32_t owner=0;DungeonMechanismOwner kind=DungeonMechanismOwner::Switch;
 uint32_t profile=0;std::array<uint32_t,7>roles{};
};
struct DungeonMechanismProfile {
 uint32_t id=0;DungeonMechanismFamily family=DungeonMechanismFamily::Lever;
 PropClipsData properties;DungeonWallTimelineData wall;
};
// ENCDMA01 format1/capability1. Immutable shared ownership keeps the graph view
// alive across copies. Loading is fresh-owner-only; no live scene/media/save.
class DungeonMechanismBindingsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}
 DungeonSwitchesView graph()const;
 const std::vector<DungeonMechanismBinding>&bindings()const;
 const DungeonMechanismProfile*profile(uint32_t)const;
private:
 struct State;std::shared_ptr<const State>state_;
};
}
