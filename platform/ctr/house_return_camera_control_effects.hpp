#pragma once
#include "house_return_camera_control.hpp"
#include "house_return_dialogue.hpp"
#include "encore/field_global_data.hpp"
#include "encore/native_input.hpp"
namespace encore::ctr {
// Concrete InputDefault vibration request state on the existing HID input owner.
// No device poll/timer is introduced. CTR has no joypad force-feedback driver;
// source request/getters are mapped, physical vibration is not provided.
struct HouseJoyVibrationRequest {uint32_t device=0;float weak=0,strong=0,duration=0;uint64_t timestamp_usec=0;};
class HouseReturnJoyInput final {
public:
 HouseReturnJoyInput()=default;
 HouseReturnJoyInput(const HouseReturnJoyInput&)=delete;
 HouseReturnJoyInput&operator=(const HouseReturnJoyInput&)=delete;
 HouseReturnJoyInput(HouseReturnJoyInput&&)=delete;
 HouseReturnJoyInput&operator=(HouseReturnJoyInput&&)=delete;
 bool physical_force_feedback_available()const{return false;}
 bool prepare(const HouseReturnCameraControlData&,upstream::NativeInputAdapter&,
              upstream::FieldGlobalRegistry&,std::string&);
 bool start(uint32_t,float,float,float,std::string&);
 bool observe(uint32_t,HouseJoyVibrationRequest&,std::string&)const;
 bool shutdown(std::string&);
 const upstream::FieldGlobalRegistry*registry()const{return registry_;}
 const upstream::NativeInputAdapter*input_owner()const{return input_;}
 ~HouseReturnJoyInput();
private:
 const HouseReturnCameraControlData*data_=nullptr;
 upstream::NativeInputAdapter*input_=nullptr;upstream::FieldGlobalRegistry*registry_=nullptr;
 std::array<uint8_t,32>ir_{};std::map<uint32_t,HouseJoyVibrationRequest>requests_;
};
struct HouseReturnCameraEffectsInput {
 const upstream::HouseReturnSources*sources=nullptr;
 const HouseReturnCameraControlData*data=nullptr;
 const upstream::FieldSceneAudioData*audio_data=nullptr;
 upstream::FieldNodeTreeRuntime*tree=nullptr;upstream::FieldGlobalRegistry*registry=nullptr;
 HouseReturnControlsNative*controls=nullptr;HouseReturnCameraControl*source=nullptr;
 PodunkSceneAudio*audio=nullptr;AudioPlayer*audio_player=nullptr;
 HouseUiContinuation*ui=nullptr;upstream::FieldGlobalConstructorRuntime*global=nullptr;
 upstream::FieldGlobalDataRuntime*globaldata=nullptr;upstream::SourceRandom*random=nullptr;
 HouseReturnJoyInput*input=nullptr;HouseReturnDialogue*dialogue=nullptr;
 upstream::OpeningWorld*world=nullptr;
};
// Fixed address; all borrowed owners/buffers outlive exact checked unbind and
// release_stream_after_delete. The Registry retains the actual non-Node Sample.
class HouseReturnCameraControlEffects final : public HouseRoomShakerEffectsOwner,
                                            public upstream::OpeningRoomShakerOwner {
public:
 HouseReturnCameraControlEffects()=default;
 HouseReturnCameraControlEffects(const HouseReturnCameraControlEffects&)=delete;
 HouseReturnCameraControlEffects&operator=(const HouseReturnCameraControlEffects&)=delete;
 HouseReturnCameraControlEffects(HouseReturnCameraControlEffects&&)=delete;
 HouseReturnCameraControlEffects&operator=(HouseReturnCameraControlEffects&&)=delete;
 struct Sample;
 bool prepare(HouseReturnCameraEffectsInput,std::string&);
 bool bind_world(std::string&);
 bool unbind_world(std::string&);
 bool release_stream_after_delete(std::string&);
 const upstream::OpeningWorld*world()const override{return in_.world;}
 bool source_frame_closed(const upstream::OpeningWorld&,std::string&)const override;
 bool invoke_room_binding(upstream::OpeningWorld&,uint32_t,std::string&)override;
 bool observe(upstream::FieldObjectId,upstream::FieldObjectId,HouseRoomShakerEffectsState&,std::string&)const override;
 bool load_sound(upstream::FieldObjectId,upstream::FieldObjectId,std::string_view,const std::array<uint8_t,32>&,std::string&)override;
 bool play(upstream::FieldObjectId,upstream::FieldObjectId,std::string&)override;
 bool source_game_over(upstream::FieldObjectId,bool&,std::string&)const override;
 bool source_joy(upstream::FieldObjectId,uint32_t,double,double,double,std::string&)override;
private:
 HouseReturnCameraEffectsInput in_{};std::array<uint8_t,32>ir_{},audio_ir_{};
 std::shared_ptr<Sample>sample_;bool prepared_=false,bound_=false;size_t depth_=0;
 bool live(std::string&)const;
 bool receivers(upstream::FieldObjectId,upstream::FieldObjectId,bool,std::string&)const;
 bool periodic(uint32_t,upstream::RoomBinding&,std::string&)const;
};
}
