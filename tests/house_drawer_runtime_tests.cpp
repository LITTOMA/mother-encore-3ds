#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include "encore/content.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace encore::upstream;
namespace {
void check(bool ok,const char*why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);}
void put(std::vector<uint8_t>&b,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(n>>(8*i));}
uint32_t crc(const std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t i=0;i<b.size();++i){c^=i>=16&&i<20?0:b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
struct Effects:DrawerHost {
 bool space=true,reject=false;unsigned grants=0,sounds=0;
 bool validate_text(uint32_t,std::string&)override{return true;}
 bool validate_flag(std::string_view,std::string&)override{return true;}
 bool validate_item(DrawerItemTemplate,std::string_view,std::string&e)override{if(reject){e="missing grant definition";return false;}return true;}
 bool validate_sound(std::string_view,std::string&)override{return true;}
 bool show_text(uint32_t,std::string&)override{return false;}
 bool flag(std::string_view,bool&,std::string&)override{return false;}
 bool inventory_space()const override{return space;}
 bool grant_item(DrawerItemTemplate,std::string_view,std::string&)override{++grants;return true;}
 bool play_sound(std::string_view,std::string&)override{++sounds;return true;}
 bool set_flag(std::string_view,bool,std::string&)override{return false;}
};
}
int main(int argc,char**argv){
 check(argc==6,"House, Room, font, inspections and Drawer binary paths required");std::string error;HouseData house;RoomData room;BattleData font;HouseInspectionData inspectors;DrawerProgramData drawer;
 check(house.load_file(argv[1],error),error.c_str());check(room.load_file(argv[2],error),error.c_str());check(font.load_file(argv[3],error),error.c_str());check(inspectors.load_file(argv[4],error),error.c_str());check(drawer.load_file(argv[5],error),error.c_str());
 OpeningWorld world;HousePresentation presentation;HouseRuntime runtime;SourceRandom random(47);Effects effects;
 check(world.initialize(room.view()),world.error());world.attach_random(random);check(presentation.begin(house.view(),font.view(),random),presentation.error());check(runtime.initialize(house.view(),world,presentation),runtime.error());
 check(!runtime.bind_inspections(inspectors.view()),"text-only binding cannot flatten programme");effects.reject=true;check(!runtime.bind_drawer(drawer.view(),effects),"unbound item rejected before side effects");check(!effects.grants&&!effects.sounds&&!world.house_paused(),"failed binding preserves idle scene");effects.reject=false;check(runtime.bind_drawer(drawer.view(),effects),runtime.error());
 const auto iv=inspectors.view();const auto dv=drawer.view();uint32_t target=UINT32_MAX;std::string flag;
 for(uint32_t i=0;i<dv.count(DrawerSection::Commands);++i)if(dv.command(i).opcode==uint32_t(DrawerOpcode::SetFlag))flag=std::string(dv.string(dv.command(i).a));check(!flag.empty(),"source programme has acquisition flag");
 for(uint32_t i=0;i<iv.count(HouseInspectionSection::Objects);++i){const auto o=iv.object(i);if(iv.string(o.source_path)==dv.string(dv.binding().inspection_source))target=i;for(uint32_t j=0;j<o.override_count;++j)check(world.set_story_flag(iv.string(iv.override_dialogue(o.first_override+j).flag),false,false),"clear source conditions");}
 check(target!=UINT32_MAX,"programme targets actual source inspector");std::vector<uint8_t>bytes;check(encore::read_file(argv[4],bytes,1024*1024,error),error.c_str());const auto ob=get(bytes,84);auto bits=[](float v){uint32_t n;std::memcpy(&n,&v,4);return n;};const auto player=world.player();const auto ray=house.view().interaction();
 for(uint32_t i=0;i<iv.count(HouseInspectionSection::Objects);++i){const auto at=ob+size_t(i)*76;for(unsigned field:{44u,48u,52u,56u})put(bytes,at+field,bits(9000));}
 for(unsigned field:{44u,52u})put(bytes,ob+size_t(target)*76+field,bits(player.position.x+ray.ray_origin.x));for(unsigned field:{48u,56u})put(bytes,ob+size_t(target)*76+field,bits(player.position.y+ray.ray_origin.y));put(bytes,16,crc(bytes));HouseInspectionData isolated;check(isolated.load(bytes.data(),bytes.size(),error),error.c_str());check(runtime.bind_inspections(isolated.view()),runtime.error());check(runtime.inspection_interaction_supported(target),"programme inspector supported");
 auto interact=[&](){check(runtime.idle_frame(double(float(1./60)),true,false),runtime.error());check(runtime.phase()==HousePhase::InspectionProgram&&world.house_paused(),"Accept starts original programme");};
 auto finish=[&](){for(unsigned frame=0;frame<10000&&runtime.phase()==HousePhase::InspectionProgram;++frame){check(presentation.idle_frame(double(float(1./60))),presentation.error());check(runtime.idle_frame(double(float(1./60)),true,false),runtime.error());}check(runtime.phase()==HousePhase::Idle&&!world.house_paused(),"programme returns control");};
 interact();check(!effects.grants&&!effects.sounds&&!world.story_flag(flag),"first phrase waits before grant");finish();check(effects.grants==1&&effects.sounds==1&&world.story_flag(flag),"actual House consumer grants and sets source flag");check(runtime.seen_dialogue_keys().empty(),"inspection programme does not write NPC seen history");
 interact();finish();check(effects.grants==1&&effects.sounds==1,"repeat interaction does not duplicate acquisition");
 check(world.set_story_flag(flag,false,false),"clear source flag for full-inventory branch");effects.space=false;interact();finish();check(effects.grants==1&&effects.sounds==1&&!world.story_flag(flag),"full branch preserves item/sound/flag state");
 std::cout<<"Actual House Drawer binding, interaction, acknowledgement, repeat and full-inventory manual cases\n";
}
