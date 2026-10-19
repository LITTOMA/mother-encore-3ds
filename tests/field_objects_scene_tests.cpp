#include "encore/field_scene.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Object scene %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t x){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(x>>(8*i));}
void fix(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
struct Host:MickHost {
 FieldScene&scene;std::set<std::string>items;
 Host(FieldScene&s,MickView v):scene(s){items.insert(std::string(v.string(v.actor().item)));for(uint32_t i=0;i<v.count(MickSection::Commands);++i){const auto c=v.command(i);if(c.opcode==uint32_t(MickOpcode::GrantKeyItem))items.insert(std::string(v.string(c.a)));}}
 bool validate_flag(std::string_view name,std::string&e)override{const auto r=scene.world.content();for(uint32_t i=0;i<r.flag_count();++i)if(r.string(r.flag(i).name_string)==name){e.clear();return true;}e="Unknown fixture flag";return false;}
 bool validate_item(std::string_view name,std::string&e)override{if(items.count(std::string(name))){e.clear();return true;}e="Unknown fixture item";return false;}
 bool flag(std::string_view name,bool&out,std::string&e)override{if(!validate_flag(name,e))return false;out=scene.world.story_flag(name);return true;}
 bool set_flag(std::string_view,bool,std::string&)override{return false;}
 bool grant_key_item(std::string_view,uint32_t,std::string&)override{return false;}
 bool remove_key_item(std::string_view,std::string&)override{return false;}
 bool show_text(std::string_view,std::string_view,std::string&)override{return false;}
 bool play_sound(std::string_view,std::string&)override{return false;}
};
}
int main(int argc,char**argv){
 CHECK(argc==5);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,1024*1024,error));FieldObjectsData legacy,drawing;CHECK(legacy.load(bytes.data(),bytes.size(),error)&&legacy.capabilities()==2&&!legacy.drawing_bound());
 // Synthetic bytes exercise the receipt mechanism, not a texture conversion
 // or frontend. The actual source art will be checked by its asset producer.
 const std::vector<uint8_t>payload{4,8,15,16,23,42};put(bytes,20,3);put(bytes,56,uint32_t(payload.size()));put(bytes,60,encore::crc32(payload.data(),payload.size()));fix(bytes);
 CHECK(drawing.load(bytes.data(),bytes.size(),error)&&drawing.drawing_bound()&&drawing.rendering_bound());CHECK(drawing.texture_matches(payload.data(),payload.size(),error));CHECK(!drawing.texture_matches(nullptr,payload.size(),error)&&!drawing.texture_matches(payload.data(),payload.size()-1,error));auto corrupt=payload;corrupt.back()^=1;CHECK(!drawing.texture_matches(corrupt.data(),corrupt.size(),error));CHECK(!legacy.texture_matches(payload.data(),payload.size(),error));
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{20,0},{20,1},{20,2},{20,4},{56,0},{56,1024*1024+1}}){auto b=bytes;put(b,edit.first,edit.second);fix(b);CHECK(!drawing.load(b.data(),b.size(),error)&&drawing.drawing_bound());}
 FieldMapData map;RoomData room;WorldLinksData links;MickData mick;CHECK(map.load_file(argv[2],error)&&room.load_file(argv[3],error)&&mick.load_file(argv[4],error)&&links.load_file("romfs/world/world.enclinks",error));
 const auto r=room.view();uint32_t programme=kRoomNoIndex;for(uint32_t i=0;i<r.program_count();++i)if(r.string(r.program(i).source_path_string)=="fixture/ordered-source-object-deletion")programme=i;CHECK(programme!=kRoomNoIndex);
 const auto&d=drawing.objects()[0];
 auto prepare=[&](FieldScene&s,const std::vector<bool>&flags){return s.prepare(r,map.view(),links,flags,{1432,4719},{0,-1},{400,240},error);};
 FieldScene unbound;CHECK(prepare(unbound,std::vector<bool>(r.flag_count(),false)));CHECK(!unbound.bind_objects(drawing,error)&&!unbound.objects().prepared());CHECK(!unbound.finish_transition()&&unbound.world.house_paused());
 FieldScene scene;CHECK(prepare(scene,std::vector<bool>(r.flag_count(),false)));Host host(scene,mick.view());CHECK(scene.bind_mick(mick.view(),host,error));CHECK(!scene.bind_objects(legacy,error)&&!scene.objects().prepared());
 {auto b=bytes;put(b,word(bytes,84)+8,word(bytes,word(bytes,84)+8)^1);fix(b);FieldObjectsData wrong;CHECK(wrong.load(b.data(),b.size(),error)&&!scene.bind_objects(wrong,error)&&!scene.objects().prepared());}
 CHECK(scene.bind_objects(drawing,error));CHECK(!scene.bind_objects(drawing,error)&&!scene.bind_mick(mick.view(),host,error));CHECK(scene.objects().state(1)->alive&&!scene.objects().state(1)->visible&&!scene.item_active(d.draw));CHECK(scene.finish_transition());
 CHECK(scene.idle_frame(1./60,false)&&scene.item_active(d.draw)&&scene.objects().state(1)->frame==6);
 auto notices=scene.nearby_notices(d.position,1,128);CHECK(std::any_of(notices.begin(),notices.end(),[&](auto n){return n.index==d.notice;}));
 CHECK(scene.world.begin_house_program(programme)&&scene.world.story_flag("got_shovel"));CHECK(scene.refresh_conditions(error)&&scene.item_active(d.draw)&&scene.objects().state(1)->alive&&!scene.objects().state(1)->queued);
 CHECK(scene.world.idle_frame(1./60)&&scene.objects().state(1)->alive&&scene.objects().state(1)->queued);CHECK(scene.idle_frame(1./60,false)&&!scene.item_active(d.draw)&&!scene.objects().state(1)->alive);
 notices=scene.nearby_notices(d.position,1,128);CHECK(std::none_of(notices.begin(),notices.end(),[&](auto n){return n.index==d.notice;}));CHECK(scene.refresh_conditions(error)&&!scene.item_active(d.draw));
 std::vector<bool>flags;for(uint32_t i=0;i<r.flag_count();++i)flags.push_back(scene.world.story_flag_at(i));FieldScene again;CHECK(prepare(again,flags));Host again_host(again,mick.view());CHECK(again.bind_mick(mick.view(),again_host,error)&&again.bind_objects(drawing,error));CHECK(!again.objects().state(1)->alive&&!again.item_active(d.draw)&&again.finish_transition());CHECK(again.idle_frame(.1,false)&&!again.objects().state(1)->alive);
 const auto revision=again.world.story_revision();CHECK(!again.world.begin_house_program(programme)&&again.world.healthy()&&again.world.story_revision()==revision);
 CHECK(!again.prepare(r,map.view(),links,flags,{1432,4719},{0,-1},{400,240},error)&&!again.objects().state(1)->alive);
 std::printf("Field object scene owner: %u checks; bound ready state, first-idle show, live flag survives until deferred deletion, notice/draw removal and suppressed reentry; synthetic texture receipt, no GPU/gameplay claim\n",checks);
}
