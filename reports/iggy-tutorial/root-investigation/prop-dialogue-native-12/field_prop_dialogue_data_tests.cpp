#include "encore/field_story.hpp"
#include "encore/field_props.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace{
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Prop dialogue data %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(value>>(i*8));}
void checksum(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==3);const std::string fixture=argv[1],root=argv[2];std::vector<uint8_t>bytes;CHECK(encore::read_file((fixture+"/story.encstory").c_str(),bytes,128*1024,error));FieldStoryData story;CHECK(story.load(bytes.data(),bytes.size(),error));const auto owned=story.view().text(story.view().prop_dialogue().flag);CHECK(owned=="bat"&&story.view().prop_dialogue().stop_running==1);FieldPropsData props;CHECK(props.load_file((root+"/world/podunk/podunk.encprops").c_str(),error)&&story.view().bind_props(props,error));
 const auto policy=u32(bytes.data()+64+7*16+4);const auto programmes=u32(bytes.data()+64+16+4);CHECK(u32(bytes.data()+20)==5&&u32(bytes.data()+52)==8);
 auto reject=[&](std::vector<uint8_t>bad){checksum(bad);CHECK(!story.load(bad.data(),bad.size(),error));CHECK(story.view().text(story.view().prop_dialogue().flag).data()==owned.data()&&story.view().text(story.view().prop_dialogue().flag)=="bat");};
 for(auto pair:{std::pair<size_t,uint32_t>{20,6},{20,4},{52,7},{64+7*16+12,48},{policy,UINT32_MAX},{policy+4,0},{policy+8,UINT32_MAX},{policy+8,9},{policy+16,0x7fc00000},{policy+20,0x7f800000},{policy+24,0},{policy+24,0xbf800000},{policy+28,0},{policy+28,1u<<20},{policy+32,0},{policy+32,2},{policy+36,0},{policy+36,3},{policy+40,0},{policy+40,128*1024+1},{policy+48,2},{programmes+8*24+20,1},{programmes+8*24+20,3}}){auto bad=bytes;put(bad,pair.first,pair.second);reject(std::move(bad));}
 for(const auto at:{policy+40,policy+44}){auto bad=bytes;put(bad,at,u32(bad.data()+at)+1);checksum(bad);FieldStoryData other;CHECK(other.load(bad.data(),bad.size(),error)&&!other.view().bind_props(props,error));}
 RoomData room;FieldMapData map;ItemData items;CHECK(map.load_file((root+"/world/podunk/podunk.encmap").c_str(),error)&&items.load_file((root+"/ui/opening.encitems").c_str(),error));CHECK(room.load_file((fixture+"/podunk.encroom").c_str(),error)&&story.view().bind(room.view(),map.view(),items.view(),error));
 for(const auto name:{"actor.encroom","side-effect.encroom"}){RoomData bad;CHECK(bad.load_file((fixture+"/"+name).c_str(),error));CHECK(!story.view().bind(bad.view(),map.view(),items.view(),error));}
 std::printf("Prop dialogue data: %u checks; cap5/legacy boundary, policy domains, source/CRC joins, actor and side-effect rejection, failed loads preserve previous owner.\n",checks);
}
