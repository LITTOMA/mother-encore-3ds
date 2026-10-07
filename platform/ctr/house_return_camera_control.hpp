#pragma once
#include "encore/house_return_sources.hpp"
#include "encore/house_return_camera_control.hpp"
#include "encore/field_scene_tree_timer.hpp"
#include "house_ui_continuation.hpp"
#include "podunk_player_host.hpp"
#include "podunk_scene_timers.hpp"
#include "house_return_controls.hpp"
#include "podunk_scene_audio.hpp"

namespace encore::ctr {
using upstream::HouseReturnCameraControlData;
// Receipt from the actual native Control / UI / source Audio / Input owners.
// There is no default method, inferred Ready or optional successful fallback.
struct HouseRoomShakerEffectsState {
  const upstream::FieldNodeTreeRuntime*tree=nullptr;
  const upstream::FieldGlobalRegistry*registry=nullptr;
  const HouseUiContinuation*ui=nullptr;
  const upstream::FieldGlobalConstructorRuntime*global=nullptr;
  const upstream::SourceRandom*random=nullptr;
  const PodunkSceneAudio*audio_owner=nullptr;
  upstream::FieldObjectId control=0,audio=0,stream=0;
  std::string source_sound,bus;
  std::array<uint8_t,32>sound_sha{};
  bool control_constructed=false,audio_constructed=false,audio_ready=false;
};
class HouseRoomShakerEffectsOwner {
public:
  virtual ~HouseRoomShakerEffectsOwner()=default;
  virtual bool observe(upstream::FieldObjectId control,upstream::FieldObjectId audio,
      HouseRoomShakerEffectsState&,std::string&)const=0;
  // Real ResourceLoader assignment to this same AudioStreamPlayer. Must return
  // its actual stream Resource receipt through observe, never a bool-only load.
  virtual bool load_sound(upstream::FieldObjectId control,upstream::FieldObjectId audio,
      std::string_view,const std::array<uint8_t,32>&,std::string&)=0;
  virtual bool play(upstream::FieldObjectId control,upstream::FieldObjectId audio,
      std::string&)=0;
  // These methods execute the named real source receivers. game_over reads the
  // continued UiManager._game_over; joy reads same globaldata.rumble then Input.
  // Disabled rumble is the source no-op; unsupported enabled Input must reject.
  virtual bool source_game_over(upstream::FieldObjectId actual_ui,bool&,std::string&)const=0;
  virtual bool source_joy(upstream::FieldObjectId actual_global,uint32_t,double,double,
      double,std::string&)=0;
};
struct HouseReturnCameraControlInput {
  const upstream::HouseReturnSources*sources=nullptr;
  const HouseReturnCameraControlData*data=nullptr;
  upstream::FieldNodeTreeRuntime*tree=nullptr;
  upstream::FieldGlobalRegistry*registry=nullptr;
  upstream::FieldObjectSignals*signals=nullptr;
  upstream::FieldGlobalConstructorRuntime*global=nullptr;
  upstream::SourceRandom*random=nullptr;
  HouseUiContinuation*ui=nullptr;
  PodunkPlayerHost*player=nullptr;
  PodunkSceneTimers*node_timers=nullptr;
  upstream::FieldSceneTreeTimers*scene_timers=nullptr;
  HouseReturnControlsNative*controls=nullptr;
  HouseRoomShakerEffectsOwner*effects=nullptr;
};
// Actual single roomshaker.gd body. Neither Camera/shaker physics nor native
// Timer/SceneTreeTimer traversal is driven here. Borrowers stay fixed until
// all source waiters and source callbacks have completed/deleted their owner.
class HouseReturnCameraControl final {
public:
  bool prepare(HouseReturnCameraControlInput,std::string&);
  bool owns(const upstream::FieldNodeDescriptor&)const;
  bool owns(upstream::FieldObjectId)const;
  bool construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,
                 const upstream::FieldIdentity&,std::string&);
  bool bind(upstream::FieldObjectId,const upstream::FieldNodeBinding&,std::string&);
  bool finish_factory(std::string&);
  bool phase(upstream::FieldObjectId,upstream::FieldTreePhase,std::string&);
  bool deferred(const upstream::FieldDeferredMessage&,std::string&);
  bool start_shake(upstream::FieldObjectId,std::string&);
  bool stop_shake(upstream::FieldObjectId,std::string&);
  bool delayed_start(upstream::FieldObjectId,double,std::string&);
  bool vibrate(upstream::FieldObjectId,std::string&);
  bool collect_expired(std::string&);
  bool release_deleted(upstream::FieldObjectId,std::string&);
  size_t pending_waiters()const{return waiting_.size();}
  size_t callback_depth()const{return depth_;}
  bool source_call_live(upstream::FieldObjectId,std::string&)const;
  bool source_closed(std::string&)const;
private:
  struct Wait {std::weak_ptr<upstream::FieldSceneTreeTimer>timer;bool returned=false;};
  bool borrows(std::string&)const;
  bool node(uint32_t,upstream::FieldObjectId&,bool,std::string&)const;
  bool live(upstream::FieldObjectId,bool,std::string&)const;
  bool effects(bool,std::string&)const;
  bool timeout(upstream::FieldObjectId,std::string&);
  HouseReturnCameraControlInput in_{};
  upstream::FieldObjectId object_=0,timer_=0,audio_=0,deleted_object_=0,deleted_timer_=0,deleted_audio_=0;
  bool prepared_=false,bound_=false,factory_=false,entered_=false,ready_=false,failed_=false;
  size_t depth_=0;std::map<upstream::FieldObjectId,Wait>waiting_;
};
}
