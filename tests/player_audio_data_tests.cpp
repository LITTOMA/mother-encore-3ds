#include "encore/field_swing_action.hpp"
#include "encore/audio_data.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;void check(bool ok,const char*s){++checks;if(!ok){std::fprintf(stderr,"Player audio data %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
void seal(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==5);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,32*1024,error));FieldSwingActionData data;FieldSwingData swing;CHECK(swing.load_file(argv[2],error));CHECK(data.load(bytes.data(),bytes.size(),error)&&data.bind(swing,error));CHECK(bytes.size()==456&&data.capability()==2&&data.audio().bus==1&&data.audio().pause_mode==2&&data.audio().gain_db==0&&data.audio().pitch==1);
 AudioBank bank;CHECK(bank.load_file(argv[4],error));AudioAsset asset;CHECK(bank.find_source("res://"+swing.strings()[2],data.audio().source_sha256,asset)&&!asset.loops());
 const auto digest=data.audio().source_sha256;auto unchanged=[&](){CHECK(data.valid()&&data.capability()==2&&data.audio().source_sha256==digest&&data.audio().pitch==1&&data.bind(swing,error));};
 for(size_t size=0;size<bytes.size();++size){CHECK(!data.load(bytes.data(),size,error));unchanged();}CHECK(!data.load(nullptr,bytes.size(),error));CHECK(!data.load(bytes.data(),32769,error));unchanged();
 const auto audio=get(bytes,116);auto reject=[&](std::vector<uint8_t>b){seal(b);CHECK(!data.load(b.data(),b.size(),error));unchanged();};
 for(auto pair:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,1},{20,3},{52,3},{112,0},{116,0},{116,129},{120,0},{120,2},{124,1},{audio+32,0x7fc00000},{audio+32,0x7f800000},{audio+32,0x42c80000},{audio+32,0xc3000000},{audio+36,0},{audio+36,0x7fc00000},{audio+36,0x40a00000},{audio+40,0},{audio+40,2},{audio+44,0},{audio+44,1},{audio+44,3}}){auto b=bytes;put(b,pair.first,pair.second);reject(b);}
 {auto b=bytes;std::fill(b.begin()+audio,b.begin()+audio+32,0);reject(b);}
 {auto b=bytes;b[get(bytes,100)+get(bytes,104)]^=1;reject(b);}
 {auto b=bytes;b[16]^=1;CHECK(!data.load(b.data(),b.size(),error));unchanged();}
 {auto b=bytes;b.push_back(0);put(b,12,uint32_t(b.size()));reject(b);}
 CHECK(data.load_file(argv[3],error)&&data.capability()==1&&data.bind(swing,error));CHECK(data.audio().pitch==0&&std::all_of(data.audio().source_sha256.begin(),data.audio().source_sha256.end(),[](uint8_t n){return n==0;}));CHECK(data.load(bytes.data(),bytes.size(),error));
 // A nonzero digest is a valid schema value, but cannot join a different
 // actual source asset merely because its relative file name matches.
 {auto b=bytes;b[audio]^=1;seal(b);FieldSwingActionData different;CHECK(different.load(b.data(),b.size(),error)&&different.bind(swing,error));CHECK(!bank.find_source("res://"+swing.strings()[2],different.audio().source_sha256,asset));}
 std::printf("Player audio data: %u checks; actual sound SHA join, binary SFX/gain/pitch/process policy, cap1 retained, cap2 corruption/truncation and atomic rejection; no audible output claim\n",checks);
}
