#include "encore/restore_data.hpp"
#include "encore/content.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"RestoreData check failed at %d: %s; %s\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
static uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
static void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
static void fix(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,encore::crc32(b.data()+24,b.size()-24));}
static void skip_text(const std::vector<uint8_t>&b,size_t&at){at+=4+get(b,at);}
int main(int argc,char**argv){
 std::string root=argc>1?argv[1]:"romfs/data",error;RoomData room;HouseData house;RestoreData data;std::vector<uint8_t>bytes;
 CHECK(room.load_file((root+"/opening.encroom").c_str(),error));CHECK(house.load_file((root+"/opening.enchouse").c_str(),error));CHECK(encore::read_file((root+"/opening.encrestore").c_str(),bytes,1024*1024,error));
 auto load=[&](const std::vector<uint8_t>&b){return data.load(b.data(),b.size(),room.view(),house.view(),error);};CHECK(load(bytes));CHECK(data.valid());CHECK(data.matches(room.view(),house.view()));CHECK(!data.matches(RoomView{},house.view()));CHECK(!RestoreData{}.matches(room.view(),house.view()));CHECK(data.npc_event_positions().size()==2);CHECK(data.music_areas().size()==4);CHECK(data.inventory_load_order().size()==10);
 const auto&positions=data.npc_event_positions();CHECK(positions[0].condition.flag_name=="minnie_leave"&&positions[1].condition.flag_name=="doll_defeated");CHECK(positions[0].position.x==32&&positions[0].position.y==368);CHECK(positions[1].position.x==464&&positions[1].position.y==80);
 CHECK(data.music_areas()[0].room_resource_index==33&&data.music_areas()[0].supported);CHECK(data.music_areas()[0].extents.x==356&&data.music_areas()[0].extents.y==456);CHECK(data.music_areas()[0].fadein_seconds==1&&data.music_areas()[0].fadeout_seconds==1.5);CHECK(!data.music_areas()[2].supported&&data.music_areas()[2].room_resource_index==kRoomNoIndex);CHECK(data.music_areas()[3].conditions[0].flag_name=="doll_melody"&&!data.music_areas()[3].conditions[0].expected_value);
 const auto&order=data.inventory_load_order();CHECK(order[4].character_id=="lloyd"&&order[4].projected_items.size()==2);CHECK(order[4].projected_items[0].item_id=="KickMeNote"&&order[4].projected_items[1].item_id=="GlassesCelluloid");uint32_t item_count=0;for(const auto&v:order)item_count+=uint32_t(v.projected_items.size());CHECK(item_count==4);CHECK(!order[7].rebuilds_inventory&&!order[8].rebuilds_inventory&&!order[9].rebuilds_inventory);
 // Truncation and bit corruption must never replace a valid loaded owner.
 for(size_t size=0;size<bytes.size();++size)CHECK(!data.load(bytes.data(),size,room.view(),house.view(),error));
 for(size_t i=0;i<bytes.size();++i){auto bad=bytes;bad[i]^=1;CHECK(!load(bad));}
 CHECK(data.valid()&&data.npc_event_positions().size()==2&&data.inventory_load_order()[4].projected_items.size()==2);
 std::vector<std::pair<size_t,uint32_t>>bad_fields{{8,0},{20,2},{24,0},{28,0},{60,0},{64,0},{96,999}};
 size_t at=100;skip_text(bytes,at);const auto npc_count=get(bytes,at);bad_fields.push_back({at,257});at+=4;
 for(uint32_t i=0;i<npc_count;++i){const size_t npc=at;bad_fields.push_back({npc,999});bad_fields.push_back({npc+4,999});bad_fields.push_back({npc+8,999});bad_fields.push_back({npc+12,999});bad_fields.push_back({npc+16,999});at+=20;skip_text(bytes,at);bad_fields.push_back({at,999});bad_fields.push_back({at+4,999});bad_fields.push_back({at+8,2});at+=12;skip_text(bytes,at);bad_fields.push_back({at,0x7fc00000});at+=8;}
 const auto areas=get(bytes,at);bad_fields.push_back({at,0});at+=4;
 for(uint32_t i=0;i<areas;++i){const bool supported=get(bytes,at+12)!=0;bad_fields.push_back({at,0});bad_fields.push_back({at+4,999});bad_fields.push_back({at+8,999});bad_fields.push_back({at+12,2});at+=16;skip_text(bytes,at);skip_text(bytes,at);if(supported)bad_fields.push_back({at,0});at+=32;bad_fields.push_back({at,0x7f800000});bad_fields.push_back({at+8,0});at+=16;bad_fields.push_back({at+4,0x7ff00000});bad_fields.push_back({at+12,0xbff00000});at+=24;const auto conditions=get(bytes,at);bad_fields.push_back({at,0});at+=4;for(uint32_t j=0;j<conditions;++j){bad_fields.push_back({at,999});bad_fields.push_back({at+4,999});bad_fields.push_back({at+8,2});at+=12;skip_text(bytes,at);}}
 bad_fields.push_back({at,2});at+=4;const auto inventories=get(bytes,at);bad_fields.push_back({at,2});at+=4;
 for(uint32_t i=0;i<inventories;++i){bad_fields.push_back({at,999});bad_fields.push_back({at+4,999});bad_fields.push_back({at+8,2});at+=12;skip_text(bytes,at);const auto items=get(bytes,at);bad_fields.push_back({at,257});at+=4;for(uint32_t j=0;j<items;++j){skip_text(bytes,at);bad_fields.push_back({at,2});bad_fields.push_back({at+4,0});at+=8;}}
 CHECK(at==bytes.size());for(const auto&entry:bad_fields){auto bad=bytes;put(bad,entry.first,entry.second);fix(bad);CHECK(!load(bad));}
 auto trailing=bytes;trailing.push_back(0);fix(trailing);CHECK(!load(trailing));CHECK(!data.load(bytes.data(),bytes.size(),RoomView{},house.view(),error));CHECK(!data.load(bytes.data(),bytes.size(),room.view(),HouseView{},error));CHECK(load(bytes));
 std::vector<uint8_t>changed(room.view().bytes(),room.view().bytes()+room.view().byte_size());const auto actor=room.view().section_offset(RoomSection::ActorInstance)+40+16;put(changed,actor,0x43f88000);put(changed,52,0);put(changed,52,encore::crc32(changed.data(),changed.size()));RoomData other;CHECK(other.load(changed.data(),changed.size(),error));CHECK(!data.matches(other.view(),house.view()));CHECK(!data.load(bytes.data(),bytes.size(),other.view(),house.view(),error));CHECK(data.matches(room.view(),house.view()));
 std::printf("RestoreData: %u checks; source event/music/all-inventory metadata; strict cross-pack fingerprints and negative parser paths\n",checks);
}
