#pragma once
#include "encore/movement.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
constexpr uint32_t phone_no_index=UINT32_MAX;
enum class PhoneSection:uint16_t {Strings=1,Resources,Objects,Clips,FrameKeys,SoundKeys,FlagRefs,Dispatch};
enum class PhoneClipRole:uint32_t {Idle=1,Ring=2};
// PlayerTurnX/Y, CenteredSprite, PositionalAudio, FreePhone. Payphones are not admitted.
enum class PhonePolicy:uint32_t {PlayerTurnX=1,PlayerTurnY=2,Centered=4,Positional=8,Free=16};
struct PhoneResource {uint32_t id=0,path=0,kind=0,width=0,height=0,columns=0,rows=0;uint8_t sha256[32]{};};
struct PhoneObject {
 uint32_t id=0,source_path=0,resource=phone_no_index,default_program=0,first_dispatch=0,dispatch_count=0;
 uint32_t idle_clip=phone_no_index,ring_clip=phone_no_index,ring_sound=0,hangup_sound=0,audio_bus=0,phone_location=0;
 uint32_t policy=0,collision_layer=0,collision_mask=0,interaction_layer=0,interaction_mask=0,initial_frame=0;
 Vec2 position{},sprite_center{},interact_center{},interact_extents{},interact_source_offset{},interact_source_extents{},interact_source_scale{},collider_center{},collider_extents{},audio_center{};
};
struct PhoneClip {uint32_t id=0,role=0,first_frame=0,frame_count=0,first_sound=0,sound_count=0,loop=0,frame_track=0,sound_track=phone_no_index;double length=0;};
struct PhoneFrameKey {double time=0;uint32_t frame=0;};
struct PhoneSoundKey {double time=0;uint32_t resource=0;};
struct PhoneFlagRef {uint32_t id=0,identity=0;};
struct PhoneDispatch {uint32_t flag_ref=phone_no_index,program=0;};
class PhoneView {
public:
 // View lifetime is owned by PhoneData; keep that object alive and do not load
 // another pack while a runtime or renderer still uses its current view.
 bool valid()const{return bytes_!=nullptr;} explicit operator bool()const{return valid();}
 uint32_t count(PhoneSection)const;std::string_view string(uint32_t)const;
 PhoneResource resource(uint32_t)const;PhoneObject object(uint32_t)const;PhoneClip clip(uint32_t)const;
 PhoneFrameKey frame_key(uint32_t)const;PhoneSoundKey sound_key(uint32_t)const;
 PhoneFlagRef flag_ref(uint32_t)const;PhoneDispatch dispatch(uint32_t)const;
private:
 friend class PhoneData;const uint8_t*bytes_=nullptr;size_t size_=0;
 const uint8_t*record(PhoneSection,uint32_t)const;
};
class PhoneData {
public:
 PhoneData()=default;PhoneData(const PhoneData&)=delete;PhoneData&operator=(const PhoneData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 PhoneView view()const{PhoneView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:std::vector<uint8_t>bytes_;
};
}
