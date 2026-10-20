// Source KEY grant component fixture; not the full Iggy tutorial.
#include "encore/world.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool b,const char*s){++checks;if(!b){std::fprintf(stderr,"Key grant %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t a){return uint32_t(b[a])|uint32_t(b[a+1])<<8|uint32_t(b[a+2])<<16|uint32_t(b[a+3])<<24;}
void word(std::vector<uint8_t>&b,size_t a,uint32_t v){for(unsigned i=0;i<4;++i)b[a+i]=uint8_t(v>>(8*i));}
void seal(std::vector<uint8_t>&b){word(b,52,0);word(b,52,encore::crc32(b.data(),b.size()));}
struct Host:StoryKeyItemHost {
 OpeningWorld*world=nullptr;bool admitted=true,write=true;unsigned validations=0,grants=0;std::string item;uint32_t count=0;
 bool validate_key_item(std::string_view name,uint32_t doses,std::string&e)override{++validations;if(!admitted||name!="BatOld"||doses!=1){e="Missing source key policy";return false;}e.clear();return true;}
 bool grant_key_item(std::string_view name,uint32_t doses,std::string&e)override{CHECK(world&&!world->story_flag("bat"));if(!write){e="Source key write failed";return false;}++grants;item=std::string(name);count=doses;e.clear();return true;}
};
}
int main(int argc,char**argv){
 CHECK(argc==3);std::vector<uint8_t>bytes,legacy;CHECK(encore::read_file(argv[1],bytes,kMaxRoomBytes,error));CHECK(encore::read_file(argv[2],legacy,kMaxRoomBytes,error));RoomData data;CHECK(data.load(bytes.data(),bytes.size(),error));auto v=data.view();CHECK(get(bytes,36)==13&&v.program_count()==7);
 const auto programme=v.program_count()-1;const auto p=v.program(programme);CHECK(p.command_count==6);const auto grant=v.command(p.first_command+1);CHECK(grant.opcode==uint16_t(DialogueActionKind::GrantKeyItem)&&v.string(grant.target_index)=="BatOld"&&grant.value==1);
 OpeningWorld world;CHECK(world.initialize(v));CHECK(world.stage()==OpeningStage::Walking&&!world.story_flag("bat"));auto revision=world.story_revision();CHECK(!world.begin_house_program(programme));CHECK(world.healthy()&&world.stage()==OpeningStage::Walking&&revision==world.story_revision());
 Host host;host.world=&world;host.admitted=false;CHECK(!world.bind_story_key_items(host,error));CHECK(!world.begin_house_program(programme));CHECK(world.healthy()&&host.grants==0&&!world.story_flag("bat"));host.admitted=true;CHECK(world.bind_story_key_items(host,error));CHECK(!world.bind_story_key_items(host,error));
 host.admitted=false;revision=world.story_revision();CHECK(!world.begin_house_program(programme));CHECK(world.healthy()&&world.stage()==OpeningStage::Walking&&host.grants==0&&revision==world.story_revision());host.admitted=true;
 CHECK(world.begin_house_program(programme));CHECK(host.grants==1&&host.item=="BatOld"&&host.count==1&&!world.story_flag("bat"));CHECK(world.cutscene_active());CHECK(world.idle_frame(1.0/60));CHECK(world.story_flag("bat")&&host.grants==1);for(unsigned i=0;i<90&&!world.story_completed();++i)CHECK(world.idle_frame(1.0/60));CHECK(world.story_completed()&&world.stage()==OpeningStage::Walking);
 {OpeningWorld failed;CHECK(failed.initialize(v));Host h;h.world=&failed;h.write=false;CHECK(failed.bind_story_key_items(h,error));CHECK(!failed.begin_house_program(programme));CHECK(!failed.healthy()&&!failed.story_flag("bat")&&h.grants==0);}
 auto reject=[&](std::vector<uint8_t>b){seal(b);CHECK(!data.load(b.data(),b.size(),error));CHECK(data.view().program_count()==7);};
 for(auto cap:{0u,12u,14u}){auto b=bytes;word(b,36,cap);reject(b);} {auto b=legacy;word(b,36,13);reject(b);}
 uint32_t commands=0;
 for(unsigned i=0;i<kRoomSectionCount;++i){auto at=kRoomHeaderBytes+i*kRoomDirectoryEntryBytes;const auto kind=uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8;if(kind==uint32_t(RoomSection::Command))commands=get(bytes,at+4);}
 CHECK(commands>0);const auto at=commands+(p.first_command+1)*48;
 {auto b=bytes;b[at]=49;reject(b);} {auto b=bytes;b[at+2]=0;b[at+3]=0;reject(b);} // actor forbidden
 {auto b=bytes;word(b,at+8,kRoomNoIndex);reject(b);} // missing identity
 {auto b=bytes;word(b,at+12,1);reject(b);} // unknown flags
 {auto b=bytes;word(b,at+40,0);reject(b);} // unused auxiliary
 for(double value:{0.,2.,.5}){auto b=bytes;std::memcpy(b.data()+at+24,&value,8);reject(b);}
 {auto b=bytes;const float one=1;std::memcpy(b.data()+at+16,&one,4);reject(b);} // vector forbidden
 {auto b=bytes;const double one=1;std::memcpy(b.data()+at+32,&one,8);reject(b);} // duration forbidden
 std::printf("Story key grant: %u checks; source item22 precedes flag29; typed host preflight, explicit failure, old-capability/operand rejection; component fixture only\n",checks);
}
