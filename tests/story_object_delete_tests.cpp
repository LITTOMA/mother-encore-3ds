#include "encore/field_objects.hpp"
#include "encore/world.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Story object %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t x){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(x>>(8*i));}
void seal(std::vector<uint8_t>&b){put(b,52,0);put(b,52,encore::crc32(b.data(),b.size()));}
struct RejectWrite:StoryObjectHost {
 FieldObjects*objects=nullptr;unsigned calls=0;
 bool validate_story_object(uint32_t s,std::string_view p,std::string&e)const override{return objects&&objects->validate_story_object(s,p,e);}
 bool can_queue_free(std::string_view p,std::string&e)const override{return objects&&objects->can_queue_free(p,e);}
 bool queue_free(std::string_view,std::string&e)override{++calls;e="Injected object queue failure";return false;}
};
}
int main(int argc,char**argv){
 CHECK(argc==4);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,kMaxRoomBytes,error));RoomData room,legacy;FieldObjectsData metadata;
 CHECK(room.load(bytes.data(),bytes.size(),error)&&legacy.load_file(argv[3],error)&&metadata.load_file(argv[2],error));const auto v=room.view();CHECK(word(bytes,36)==14&&word(bytes,32)==7);
 uint32_t programme=kRoomNoIndex;for(uint32_t i=0;i<v.program_count();++i)if(v.string(v.program(i).source_path_string)=="fixture/ordered-source-object-deletion")programme=i;
 CHECK(programme!=kRoomNoIndex);const auto p=v.program(programme);const std::string path(v.string(v.command(p.first_command+3).target_index));CHECK(path=="Objects/Items/Item"&&v.command(p.first_command+3).opcode==49);
 CHECK(std::string(dialogue_action_name(DialogueActionKind::QueueFreeStoryObject))=="QueueFreeStoryObject");
 FieldObjects objects;SourceRandom random(9);OpeningWorld absent;CHECK(!absent.bind_story_objects(objects,error));CHECK(!objects.validate_story_object(v.scene().stable_id,path,error));CHECK(objects.prepare(metadata,v,error));OpeningWorld world;CHECK(world.initialize(v));const auto revision=world.story_revision();
 CHECK(!world.begin_house_program(programme)&&world.healthy()&&world.story_revision()==revision&&!world.story_flag("got_shovel"));
 CHECK(world.bind_story_objects(objects,error)&&!world.bind_story_objects(objects,error));
 CHECK(!world.begin_house_program(programme)&&world.healthy()&&world.story_revision()==revision&&!world.story_flag("got_shovel")); // ready admission before any flag write
 CHECK(objects.enter_ready(1,false,random,error)&&objects.idle_frame(error));CHECK(objects.state(1)->visible);
 CHECK(!objects.validate_story_object(v.scene().stable_id+1,path,error)&&!objects.validate_story_object(v.scene().stable_id,"Objects/Items/item",error));
 CHECK(world.validate_story_props(programme,error));CHECK(world.begin_house_program(programme));CHECK(world.story_flag("got_shovel")&&objects.state(1)->visible&&!objects.state(1)->queued);
 CHECK(world.idle_frame(1.0/60));CHECK(objects.state(1)->alive&&objects.state(1)->visible&&objects.state(1)->queued); // phrase3 queues, does not free immediately
 CHECK(world.end_scene_frame()&&objects.state(1)->alive); // deletion is committed by the scene object owner
 CHECK(objects.flush_queued(error)&&!objects.state(1)->alive&&!objects.state(1)->visible);
 for(unsigned i=0;i<90&&!world.story_completed();++i)CHECK(world.idle_frame(1.0/60));CHECK(world.story_completed()&&world.stage()==OpeningStage::Walking);
 CHECK(!world.begin_house_program(programme)&&world.healthy()&&world.story_flag("got_shovel"));
 {OpeningWorld again;CHECK(again.initialize(v)&&again.bind_story_objects(objects,error));CHECK(!again.begin_house_program(programme)&&again.healthy()&&!again.story_flag("got_shovel"));} // static metadata bind remains legal on a suppressed reentry
 {FieldObjects live;CHECK(live.prepare(metadata,v,error)&&live.enter_ready(1,false,random,error));OpeningWorld failed;CHECK(failed.initialize(v));RejectWrite host;host.objects=&live;CHECK(failed.bind_story_objects(host,error)&&failed.begin_house_program(programme));CHECK(!failed.idle_frame(1.0/60)&&!failed.healthy()&&host.calls==1&&live.state(1)->alive&&!live.state(1)->queued);}
 auto reject=[&](std::vector<uint8_t>b){seal(b);CHECK(!room.load(b.data(),b.size(),error));CHECK(room.view().program_count()==v.program_count());};
 for(uint32_t cap:{0u,7u,8u,9u,10u,11u,12u,13u,15u}){auto b=bytes;put(b,36,cap);reject(b);}
 {std::vector<uint8_t>b;CHECK(encore::read_file(argv[3],b,kMaxRoomBytes,error));put(b,36,14);reject(b);} // cap14 requires real49
 uint32_t offset=0;for(unsigned i=0;i<kRoomSectionCount;++i){const auto d=kRoomHeaderBytes+i*kRoomDirectoryEntryBytes;if(uint32_t(bytes[d])+(uint32_t(bytes[d+1])<<8)==uint32_t(RoomSection::Command))offset=word(bytes,d+4);}
 CHECK(offset>0);const auto at=offset+(p.first_command+3)*48;
 {auto b=bytes;b[at]=50;reject(b);}{auto b=bytes;b[at+2]=0;b[at+3]=0;reject(b);}
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{at+8,kRoomNoIndex},{at+8,0},{at+12,1},{at+16,0x3f800000},{at+24,1},{at+32,1},{at+40,0},{at+44,1}}){auto b=bytes;put(b,edit.first,edit.second);reject(b);}
 const auto begin=std::search(bytes.begin(),bytes.end(),path.begin(),path.end());CHECK(begin!=bytes.end());const size_t text_at=size_t(begin-bytes.begin());
 for(char bad:{'/','\\',':','\0'}){auto b=bytes;b[text_at]=uint8_t(bad);reject(b);}
 {auto b=bytes;b[text_at]='.';b[text_at+1]='.';reject(b);}
 {auto b=bytes;b[text_at+path.size()-1]='/';reject(b);}
 {auto b=bytes;b[text_at+1]='/';b[text_at+2]='/';reject(b);}
 {auto b=bytes;b[text_at]=0xc3;b[text_at+1]=0xa9;reject(b);} // valid UTF-8, unsupported object path domain
 {auto b=bytes;b[text_at+path.size()-1]='x';seal(b);RoomData other;CHECK(other.load(b.data(),b.size(),error));OpeningWorld mismatch;CHECK(mismatch.initialize(other.view())&&!mismatch.bind_story_objects(objects,error)&&mismatch.healthy()&&!mismatch.story_flag("got_shovel"));}
 std::printf("Typed story object deletion: %u checks; source flag0 stays visible until queue3, scene/path/live preflight, retained queued identity, explicit failure and version/operand negatives; component fixture only\n",checks);
}
