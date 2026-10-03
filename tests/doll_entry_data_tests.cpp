#include "encore/battle_data.hpp"
#include "encore/content.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool condition,const char* why){++checks;if(!condition){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);}
void put(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned n=0;n<4;++n)b[p+n]=uint8_t(v>>(n*8));}
void crc(std::vector<uint8_t>&b){put(b,16,0);uint32_t c=~0u;for(auto v:b){c^=v;for(unsigned k=0;k<8;++k)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}put(b,16,~c);}
}
int main(int argc,char**argv){
 check(argc==2,"provide real Doll entry pack");std::vector<uint8_t>bytes;std::string error;check(encore::read_file(argv[1],bytes,1024*1024,error),error.c_str());BattleData owner;check(owner.load(bytes.data(),bytes.size(),error),error.c_str());auto v=owner.view();
 check(get(bytes,8)==2,"extended schema selected");check(v.metadata().room_battle_id==2&&v.metadata().enemy_instance==2&&v.metadata().player_instance==0,"new source encounter binding");check(v.metadata().encounter_audio==1002&&v.string(v.metadata().enemy_name)=="doll","source boss audio and roster");check(!(v.metadata().flags&1)&&v.count(BattleSection::Menus)==3,"source queued battle forbids Run");
 check(v.participant(1).hp==38&&v.participant(1).maxhp==38&&v.participant(1).level==2&&v.participant(1).offense==4&&v.participant(1).defense==9&&v.participant(1).xp==8&&v.participant(1).cash==10,"source Doll statistics only; no action implementation");
 check(v.count(BattleSection::Backgrounds)==2,"two source Baby layers");
 for(unsigned i=0;i<2;++i){auto b=v.background(i);check(b.width==176&&b.height==172&&b.flags==15,"original tile divisor and explicit palette policy");check(b.palette_frames==0&&b.palette_fixed_row==0,"source omitted0 divisor preserved; reviewed row0 adapter");check(v.resource(b.palette_resource).kind==2&&v.resource(b.palette_resource).width==4&&v.resource(b.palette_resource).height==4,"source palette independent checked indexed resource");check(b.ping_pong_speed.x==0&&b.ping_pong_speed.y>0&&b.compression_amplitude.y>0&&b.move.x>0,"active source distortion fields retained");}
 bool world_doll=false;for(uint32_t i=0;i<v.count(BattleSection::Resources);++i)world_doll|=v.string(v.resource(i).path)=="house-preview/doll.t3x";check(world_doll,"Doll world sprite reused by resource reference");
 for(size_t n=0;n<bytes.size();++n)check(!owner.load(bytes.data(),n,error),"all truncations rejected");
 const size_t bg=get(bytes,64+9*16+4);auto reject=[&](size_t p,uint32_t x,const char*why){auto bad=bytes;put(bad,p,x);crc(bad);check(!owner.load(bad.data(),bad.size(),error),why);check(owner.view().metadata().room_battle_id==2,"bad pack preserves owner");};
 reject(8,3,"unknown version rejected");reject(64+9*16,10u|(112u<<16),"wrong extended stride rejected");reject(bg+4,31,"unknown shader flag rejected");reject(bg+60,0x7fc00000,"nonfinite scroll rejected");reject(bg+96,0x7f800000,"nonfinite compression rejected");reject(bg+100,0xffffffffu,"missing palette rejected");reject(bg+104,0x7fc00000,"nonfinite palette speed rejected");reject(bg+108,4,"fixed row cannot replace defined shader cycle");reject(bg+112,4,"fixed palette row outside source rejected");reject(bg+4,7,"undefined zero divisor without explicit policy rejected");reject(bg+4,11,"fixed row without palette shifting rejected");
 std::printf("Doll BattleData: %u checks; source roster, complete Baby fields, reviewed fixed-row policy, all truncations and CRC-correct corruption\n",checks);
}
