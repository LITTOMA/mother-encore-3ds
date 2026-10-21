#include "encore/field_steps.hpp"
#include "encore/field_scene.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Catacombs steps %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(value>>(i*8));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
void pose(const FieldStepShadowState&s,uint32_t mode,uint32_t frame,int behind,int playing){CHECK(s.animation()==mode&&s.frame()==frame&&s.behind()==bool(behind)&&s.playing()==bool(playing));}
void callback(FieldRunLoop&loop,FieldStepShadowState&player,FieldStepShadowState&party,uint32_t area,bool entered,uint32_t body){
 CHECK(body<3);const auto kind=body==1?FieldStepBody::Player:body==0?FieldStepBody::Party:FieldStepBody::Other;
 CHECK(apply_step_callback(loop,body==1?&player:body==0?&party:nullptr,area,entered,kind,error));
}
}
int main(int argc,char**argv){
 CHECK(argc==2);const std::string root=argv[1];std::vector<uint8_t>bytes;CHECK(encore::read_file((root+"/catacombs.encsteps").c_str(),bytes,1048576,error));FieldStepsData data;CHECK(data.load(bytes.data(),bytes.size(),error));const auto v=data.view();CHECK(v.capability()==2&&v.scene_id()==4&&v.area_count()==3&&v.sound_count()==8&&v.part_count()==23&&v.shadow_count()==3);
 CHECK(v.text(v.sound(v.policy().default_sound).name)=="wood"&&v.text(v.policy().slot)=="run");CHECK(v.text(v.area(0).enter_shadow)=="ripple"&&v.text(v.area(0).exit_shadow)=="shadow");CHECK(v.area_id(0)&&v.part_count(0)==21&&v.part_count(1)==1&&v.part_count(2)==1);
 CHECK(v.text(v.shadow(0).name)=="shadow"&&v.shadow(0).frames==1&&v.shadow(0).fps==5&&!v.shadow(0).front&&v.shadow(0).loop);CHECK(v.text(v.shadow(1).name)=="ripple"&&v.shadow(1).frames==3&&v.shadow(1).fps==5&&v.shadow(1).front&&v.shadow(1).loop);CHECK(v.text(v.shadow(2).name)=="magic"&&v.shadow(2).frames==3&&v.shadow(2).fps==8&&!v.shadow(2).front&&v.shadow(2).loop);
 CHECK(v.area_id(99)==0&&v.part_count(99)==0&&v.point(99999).x==0&&v.shadow(999).frames==0);CHECK(!FieldStepsView{}.capability()&&!FieldStepsView{}.part_count()&&!FieldStepsView{}.shadow_count());
 const auto policy=get(bytes,84),sounds=get(bytes,100),areas=get(bytes,116),parts=get(bytes,132),points=get(bytes,148),modes=get(bytes,164);
 auto reject=[&](std::vector<uint8_t>bad){sign(bad);CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.view().bytes_identity()==v.bytes_identity()&&data.view().part_count()==23);};
 for(auto mutation:std::vector<std::pair<size_t,uint32_t>>{{8,2},{12,0},{20,0},{20,1},{20,3},{24,0},{28,1},{32,0},{52,4},{52,8},{56,1},{60,1},{64,2},{68,0},{72,0},{76,2},{124,64},{128,6},{140,8},{156,16},{172,16},{policy,8},{policy+4,UINT32_MAX},{policy+12,UINT32_MAX},{policy+20,2},{sounds,UINT32_MAX},{sounds+8,UINT32_MAX},{sounds+48,0},{sounds+52,0},{areas+8,8},{areas+12,8},{areas+16,0x7fc00000},{areas+24,0},{areas+32,UINT32_MAX},{areas+40,1},{areas+48,0},{areas+52,0},{areas+56,2},{areas+60,0},{areas+60,UINT32_MAX},{areas+64,1},{areas+68,0},{areas+68,65},{areas+72+60,get(bytes,areas+60)},{parts,1},{parts+4,1},{parts+8,2},{parts+8,65},{parts+12,1},{points,0x7fc00000},{points+4,0x7f800000},{modes,UINT32_MAX},{modes+8,0},{modes+8,0x7fc00000},{modes+12,0},{modes+12,65},{modes+16,2},{modes+20,2},{modes+24,1},{modes+28,1}}){auto bad=bytes;put(bad,mutation.first,mutation.second);reject(std::move(bad));}
 {auto bad=bytes;put(bad,policy+4,get(bytes,sounds));put(bad,policy+8,get(bytes,sounds+4));reject(std::move(bad));}
 {auto bad=bytes;put(bad,areas+32,get(bytes,sounds));put(bad,areas+36,get(bytes,sounds+4));reject(std::move(bad));}
 {auto bad=bytes;for(unsigned i=0;i<8;++i)bad[points+8+i]=bad[points+i];reject(std::move(bad));}
 {auto bad=bytes;for(unsigned i=0;i<32;++i)bad[sounds+16+i]=0;reject(std::move(bad));}
 {auto bad=bytes;bad.back()^=1;CHECK(!data.load(bad.data(),bad.size(),error));}
 for(size_t n=0;n<bytes.size();++n){CHECK(!data.load(bytes.data(),n,error));CHECK(data.view().bytes_identity()==v.bytes_identity());}
 CHECK(!data.load(nullptr,bytes.size(),error));CHECK(!data.load(bytes.data(),1048577,error));
 FieldScene scene;AudioBank bank;CHECK(!scene.bind_steps(v,bank,"wood",error));CHECK(error=="Polygon stepping requires a matched contact and Shadow consumer");CHECK(!v.matches(bank,error));
 FieldRunLoop loop;FieldStepShadowState player,party;CHECK(loop.initialize(v,v.text(v.sound(v.policy().default_sound).name),error));CHECK(player.initialize(v,error)&&party.initialize(v,error));CHECK(!player.initialize(v,error));
 std::ifstream call_file(root+"/calls.tsv");CHECK(call_file.good());std::string line;unsigned calls=0;
 while(std::getline(call_file,line)){
  std::istringstream row(line);unsigned op,a,b,c,d,wanted,registered,playing,running,count,event,sound,pm,pf,pb,pp,om,of,ob,oplay;CHECK(bool(row>>op>>a>>b>>c>>d>>wanted>>registered>>playing>>running>>count>>event>>sound>>pm>>pf>>pb>>pp>>om>>of>>ob>>oplay));std::string trailing;CHECK(!(row>>trailing));loop.clear_events();
  if(op==0)CHECK(loop.set_running(a,b,c,d,error));else if(op==1)callback(loop,player,party,a,b,c);else if(op==2)CHECK(loop.set_area_enabled(a,b,error));else if(op==3||op==4)CHECK(loop.stop_playback(error));else if(op==5){auto&s=a==1?player:party;CHECK(s.set_animation(v.text(v.shadow(b).name),error));CHECK(s.set_frame(c,error));}else CHECK(false);
  CHECK(loop.wanted_sound()==wanted&&loop.registered_sound()==registered&&loop.playing()==bool(playing)&&loop.running()==bool(running));CHECK(loop.events().size()==count);if(count)CHECK(uint32_t(loop.events()[0].kind)==event&&loop.events()[0].sound==sound);pose(player,pm,pf,pb,pp);pose(party,om,of,ob,oplay);++calls;
 }
 CHECK(calls==60);const auto old_wanted=loop.wanted_sound(),old_animation=player.animation(),old_frame=player.frame();const auto old_events=loop.events().size();
 CHECK(!apply_step_callback(loop,&player,99,true,FieldStepBody::Player,error));CHECK(!apply_step_callback(loop,&player,0,true,FieldStepBody(0),error));CHECK(!apply_step_callback(loop,nullptr,0,true,FieldStepBody::Player,error));CHECK(!apply_step_callback(loop,&player,0,true,FieldStepBody::Other,error));
 FieldStepsData second;CHECK(second.load(bytes.data(),bytes.size(),error));FieldStepShadowState unrelated;CHECK(unrelated.initialize(second.view(),error));CHECK(!apply_step_callback(loop,&unrelated,0,true,FieldStepBody::Player,error));CHECK(!player.set_animation("unknown_animation",error));CHECK(!player.set_frame(999,error));CHECK(player.animation()==old_animation&&player.frame()==old_frame&&loop.wanted_sound()==old_wanted&&loop.events().size()==old_events);
 FieldStepShadowState empty;CHECK(!empty.set_animation("shadow",error));CHECK(!empty.set_frame(0,error));CHECK(!empty.initialize({},error));
 std::ifstream body_file(root+"/body.tsv");CHECK(body_file.good());Vec2 local;unsigned body_count=0;CHECK(bool(body_file>>local.x>>local.y>>body_count)&&body_count==8);std::vector<Vec2>body(body_count);for(auto&p:body)CHECK(bool(body_file>>p.x>>p.y));
 std::ifstream contact_file(root+"/contacts.tsv");CHECK(contact_file.good());unsigned states=0,previous=UINT32_MAX,cases=0;
 while(std::getline(contact_file,line)){
  std::istringstream row(line);unsigned ci,phase,enabled,wanted,pm,pb,pp,om,ob,oplay;CHECK(bool(row>>ci>>phase>>enabled>>wanted>>pm>>pb>>pp>>om>>ob>>oplay));CHECK(phase<4&&phase==states%4&&ci==states/4);
  if(ci!=previous){previous=ci;++cases;loop={};player={};party={};CHECK(loop.initialize(v,v.text(v.sound(v.policy().default_sound).name),error));CHECK(player.initialize(v,error)&&party.initialize(v,error));for(uint32_t i=0;i<v.area_count();++i)CHECK(loop.set_area_enabled(i,enabled,error));}
  if(phase==3)for(uint32_t i=0;i<v.area_count();++i)CHECK(loop.set_area_enabled(i,false,error));
  struct Body{Vec2 position;uint32_t layer,mask;};std::array<Body,3>bodies{};for(auto&b:bodies)CHECK(bool(row>>b.position.x>>b.position.y>>b.layer>>b.mask));
  for(uint32_t i=0;i<v.area_count();++i)for(size_t j=0;j<bodies.size();++j){unsigned expected;CHECK(bool(row>>expected)&&expected<=1);std::vector<Vec2>world;for(const auto&p:body)world.push_back({p.x+local.x+bodies[j].position.x,p.y+local.y+bodies[j].position.y});bool actual=false;CHECK(step_polygon_overlap(v,i,world.data(),world.size(),actual,error));const auto area=v.area(i);const bool filter=(area.mask&bodies[j].layer)||(area.layer&bodies[j].mask);CHECK((actual&&filter)==bool(expected));}
  unsigned count;CHECK(bool(row>>count)&&count<=v.area_count());for(unsigned i=0;i<count;++i){unsigned area,entered,which;CHECK(bool(row>>area>>entered>>which));callback(loop,player,party,area,entered,which);}std::string trailing;CHECK(!(row>>trailing));
  CHECK(loop.wanted_sound()==wanted&&!loop.playing()&&loop.events().empty());CHECK(player.animation()==pm&&player.behind()==bool(pb)&&player.playing()==bool(pp));CHECK(party.animation()==om&&party.behind()==bool(ob)&&party.playing()==bool(oplay));++states;
 }
 CHECK(cases==41&&states==164);
 bool unchanged=true;CHECK(!step_polygon_overlap({},0,body.data(),body.size(),unchanged,error)&&unchanged);CHECK(!step_polygon_overlap(v,99,body.data(),body.size(),unchanged,error)&&unchanged);CHECK(!step_polygon_overlap(v,0,nullptr,body.size(),unchanged,error)&&unchanged);CHECK(!step_polygon_overlap(v,0,body.data(),2,unchanged,error)&&unchanged);CHECK(!step_polygon_overlap(v,0,body.data(),65,unchanged,error)&&unchanged);auto bad_body=body;bad_body[1]=bad_body[0];CHECK(!step_polygon_overlap(v,0,bad_body.data(),bad_body.size(),unchanged,error)&&unchanged);bad_body=body;bad_body[0].x=std::numeric_limits<float>::quiet_NaN();CHECK(!step_polygon_overlap(v,0,bad_body.data(),bad_body.size(),unchanged,error)&&unchanged);
 std::printf("Catacombs stepping: %u checks;60 original callback/run/Shadow and164 stable native contact states;strict polygon/owner/version negatives;actual legacy scene rejects extended metadata;no elapsed Shadow clock,renderer,NDSP or complete scene pass\n",checks);
}
