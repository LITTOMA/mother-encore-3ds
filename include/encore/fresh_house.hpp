#pragma once
#include "encore/world.hpp"
#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include "encore/phone_runtime.hpp"
#include "encore/session_restore.hpp"
#include "encore/restore_data.hpp"
#include "encore/present_sparkles.hpp"
#include <memory>
namespace encore::upstream {
// Pointer-bearing runtime members stay at fixed addresses. Commit by swapping
// unique owners, never by moving their members or the nonmovable motion solver.
struct FreshHouseState;
struct FreshHouseAdapters {
 std::function<bool(FreshHouseState&,std::string&)>prepare_world,bind_house;
 // Admission only; managed areas must be cross-bound to independent source
 // music metadata before they suppress legacy restoration playback.
 std::function<bool(FreshHouseState&,const RestoreData&,std::string&)>prepare_music;
};
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
 PresentSparklesRuntime present_sparkles;
 bool bind_ready_adapter(std::function<bool(std::string&)> callback){if(!callback||ready_adapter_)return false;ready_adapter_=std::move(callback);ready_pending_=true;return true;}
 bool bind_music_adapter(std::function<bool(const RestoreMusicArea&)> claims,std::function<bool(std::string&)>ready){if(!claims||!ready||music_claims_||music_ready_)return false;music_claims_=std::move(claims);music_ready_=std::move(ready);return true;}
 bool finish_scene_ready();
 bool scene_ready_pending()const{return ready_pending_;}
private:
 std::function<bool(std::string&)>ready_adapter_;
 std::function<bool(const RestoreMusicArea&)>music_claims_;
 std::function<bool(std::string&)>music_ready_;
 bool ready_pending_=false;
 bool music_pending_=false;
 uint32_t music_resource_=kRoomNoIndex;
 double music_gain_=0,music_fade_=0;
 friend bool prepare_fresh_house(const PreparedSessionRestore&,const RestoreData&,RoomView,HouseView,BattleView,PhoneView,SourceRandom&,Vec2,std::unique_ptr<FreshHouseState>&,std::string&,const FreshHouseAdapters&);
};
// Validation and construction do not consume RNG. Existing output remains
// intact on failure. Slot/codec inspection must not call finish_scene_ready.
bool prepare_fresh_house(const PreparedSessionRestore&,const RestoreData&,
 RoomView,HouseView,BattleView font,PhoneView,SourceRandom&,Vec2 viewport,
 std::unique_ptr<FreshHouseState>&output,std::string&error,const FreshHouseAdapters&adapters={});
}
