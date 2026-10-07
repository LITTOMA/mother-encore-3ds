// Source negative cases authored for actual integration fixtures; NOT RUN.
#include "../platform/ctr/house_return_camera_control_effects.hpp"
#include <cassert>
#include "encore/crc32.hpp"
namespace encore::ctr::manual {
void house_native_audio_parser(const upstream::FieldNodeTreeData&tree,const std::vector<uint8_t>&pack){
 upstream::FieldSceneAudioData data;std::string error;
 assert(data.load(pack.data(),pack.size(),tree,error));const auto prior=data.ir_sha();
 auto corrupt=pack;corrupt[40]^=1;
 assert(!data.load(corrupt.data(),corrupt.size(),tree,error));assert(data.ir_sha()==prior);
 for(auto at:{8u,24u,28u}){corrupt=pack;corrupt[at]=0xff;assert(!data.load(corrupt.data(),corrupt.size(),tree,error));assert(data.ir_sha()==prior);}
 corrupt=pack;corrupt.back()^=1;assert(!data.load(corrupt.data(),corrupt.size(),tree,error));assert(data.valid()&&data.ir_sha()==prior);
}
void house_room_shaker_effect_scope(HouseReturnCameraControlEffects&actual,
 upstream::FieldObjectId control,upstream::FieldObjectId audio,
 upstream::FieldObjectId other,const HouseReturnCameraControlData&source){
 std::string error;std::array<uint8_t,32>hash{};
 assert(source.source_hash(source.sound(),hash));
 // These calls are outside actual source Ready/vibrate callback depth.
 assert(!actual.load_sound(control,audio,source.sound(),hash,error));
 assert(!actual.play(control,audio,error));
 bool over=false;assert(!actual.source_game_over(other,over,error));
 assert(!actual.source_joy(other,source.joy_device(),source.joy_weak(),source.joy_strong(),source.length(),error));
}
void house_room_shaker_exact_owner(upstream::OpeningWorld&actual_world,
 HouseReturnCameraControlEffects&bound,upstream::OpeningRoomShakerOwner&foreign){
 std::string error;assert(actual_world.room_shaker_owner()==&bound);
 assert(!actual_world.bind_room_shaker_owner(bound,error));
 assert(!actual_world.unbind_room_shaker_owner(foreign,error));
 assert(actual_world.room_shaker_owner()==&bound);
}
// Remaining real fixtures (not executed): ENCSAUD1 House29 complete coverage;
// audio Resource bank/source SHA corruption; UI constructor/getter SHA mismatch;
// Input engine pin mismatch; old audio voices retained -> retirement rejects;
// Timer/wait live -> unbind rejects; genuine source Delete -> unbind then release;
// same actual SFX playback callback survives prior native audio owner retirement;
// disabled rumble leaves request unchanged; enabled records f32 + monotonic ticks,
// with physical_force_feedback_available()==false (no hardware assertion).
}
