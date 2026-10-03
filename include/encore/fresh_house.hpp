#pragma once
#include "encore/world.hpp"
#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include "encore/phone_runtime.hpp"
#include "encore/session_restore.hpp"
#include "encore/restore_data.hpp"
#include <memory>
namespace encore::upstream {
// Pointer-bearing runtime members stay at fixed addresses. Commit by swapping
// unique owners, never by moving their members or the nonmovable motion solver.
struct FreshHouseState {
 FreshHouseState()=default;
 // Resource preparation can retain the exact player/camera owner. This binding
 // remains read-only until a future checked cross-scene commit is implemented;
 // initialize/initialize_restored cannot reset the shared player.
 FreshHouseState(RetainPersistentPlayer tag,const OpeningWorld& active):world(tag,active){}
 FreshHouseState(const FreshHouseState&)=delete;
 FreshHouseState& operator=(const FreshHouseState&)=delete;
 FreshHouseState(FreshHouseState&&)=delete;
 FreshHouseState& operator=(FreshHouseState&&)=delete;
 OpeningWorld world;
 HouseRuntime house;
 HousePresentation presentation;
 PhoneRuntime phone;
 bool finish_scene_ready();
 bool scene_ready_pending()const{return ready_pending_;}
private:
 bool ready_pending_=false;
 bool music_pending_=false;
 uint32_t music_resource_=kRoomNoIndex;
 double music_gain_=0,music_fade_=0;
 friend bool prepare_fresh_house(const PreparedSessionRestore&,const RestoreData&,RoomView,HouseView,BattleView,PhoneView,SourceRandom&,Vec2,std::unique_ptr<FreshHouseState>&,std::string&);
};
// Validation and construction do not consume RNG. Existing output remains
// intact on failure. Slot/codec inspection must not call finish_scene_ready.
bool prepare_fresh_house(const PreparedSessionRestore&,const RestoreData&,
 RoomView,HouseView,BattleView font,PhoneView,SourceRandom&,Vec2 viewport,
 std::unique_ptr<FreshHouseState>&output,std::string&error);
}
