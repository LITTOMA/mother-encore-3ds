#pragma once
#include "encore/audio_data.hpp"
#include "encore/room_data.hpp"
namespace encore::ctr {
// Room capability9 separates pack-local resource identity from saved Bank IDs.
// Legacy scopes require the original exact ID. An occupied ID cannot be
// substituted with an unrelated source, even in the new scope.
inline bool resolve_audio_source_identity(uint32_t capability,const upstream::RoomResource&resource,std::string_view path,const upstream::AudioBank&bank,upstream::AudioAsset&output,std::string&error){
 auto fail=[&](){error="Room/audio bank source identity mismatch";return false;};
 if(capability<1||capability>9||resource.kind!=2||!resource.stable_id||path.empty())return fail();upstream::AudioAsset candidate;
 if(bank.find(resource.stable_id,candidate)){
  if(candidate.source_path!=path||candidate.source_sha256!=resource.sha256)return fail();
 }else{
  if(capability!=9)return fail();uint32_t matches=0;
  for(uint32_t i=0;i<bank.count();++i){const auto asset=bank.asset(i);if(asset.source_path==path){if(asset.source_sha256!=resource.sha256)return fail();candidate=asset;++matches;}}
  if(matches!=1)return fail();
 }
 output=candidate;error.clear();return true;
}
inline bool resolve_room_audio_asset(const upstream::RoomView&room,const upstream::AudioBank&bank,uint32_t index,upstream::AudioAsset&output,std::string&error){
 if(!room.valid()||room.byte_size()<40||index>=room.resource_count()){error="Invalid room audio identity request";return false;}
 const auto*p=room.bytes()+36;const auto capability=uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
 const auto resource=room.resource(index);return resolve_audio_source_identity(capability,resource,room.string(resource.path_string),bank,output,error);
}
}
