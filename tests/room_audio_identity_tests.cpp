#include "room_audio_identity.hpp"
#include <cstdlib>
#include <iostream>
using namespace encore;
static void check(bool value,const char*text){if(!value){std::cerr<<text<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==2,"Provide a checked AudioBank path");upstream::AudioBank bank;std::string error;check(bank.load_file(argv[1],error),error.c_str());check(bank.count()>1,"Need two distinct source assets");
 const auto first=bank.asset(0),other=bank.asset(1);upstream::RoomResource resource;resource.kind=2;resource.stable_id=first.stable_id;resource.sha256=first.source_sha256;upstream::AudioAsset out;
 for(uint32_t cap:{4u,5u,6u,7u,8u,9u})check(ctr::resolve_audio_source_identity(cap,resource,first.source_path,bank,out,error)&&out.stable_id==first.stable_id,"Legacy exact identity remains accepted");
 for(uint32_t cap:{0u,10u,UINT32_MAX})check(!ctr::resolve_audio_source_identity(cap,resource,first.source_path,bank,out,error),"Unknown capability cannot acquire an audio identity");
 uint32_t unused=UINT32_MAX;while(bank.find(unused,out))--unused;resource.stable_id=unused;
 check(ctr::resolve_audio_source_identity(9,resource,first.source_path,bank,out,error)&&out.stable_id==first.stable_id,"Capability9 pack-local resource resolves unique source");
 const auto retained=out.stable_id;for(uint32_t cap:{4u,5u,6u,7u,8u})check(!ctr::resolve_audio_source_identity(cap,resource,first.source_path,bank,out,error)&&out.stable_id==retained,"Legacy cannot silently gain source remapping");
 resource.stable_id=other.stable_id;check(!ctr::resolve_audio_source_identity(9,resource,first.source_path,bank,out,error)&&out.stable_id==retained,"Occupied ID cannot impersonate another source");
 resource.stable_id=unused;resource.sha256[0]^=1;check(!ctr::resolve_audio_source_identity(9,resource,first.source_path,bank,out,error)&&out.stable_id==retained,"Correct path with incorrect fingerprint rejected");
 resource.sha256=first.source_sha256;check(!ctr::resolve_audio_source_identity(9,resource,"res://unreviewed.wav",bank,out,error),"Unmapped source rejected");resource.kind=1;check(!ctr::resolve_audio_source_identity(9,resource,first.source_path,bank,out,error),"Texture cannot masquerade as source audio");
 upstream::RoomView invalid;check(!ctr::resolve_room_audio_asset(invalid,bank,0,out,error)&&out.stable_id==retained,"Invalid Room view is atomic");
}
