#include "encore/field_scene.hpp"
#include "encore/dialogue_choices.hpp"
#include "encore/crc32.hpp"
#include "encore/content.hpp"
#include <cstdio>
#include <cstdlib>
#include <set>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Mayor story %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
struct Host:FieldStoryHost,MickHost {
 bool can_start(FieldStoryView,uint32_t,std::string&e)override{e.clear();return true;}
 bool validate_flag(std::string_view,std::string&)override{return false;}bool validate_item(std::string_view,std::string&)override{return false;}
 bool flag(std::string_view,bool&,std::string&)override{return false;}bool set_flag(std::string_view,bool,std::string&)override{return false;}
 bool grant_key_item(std::string_view,uint32_t,std::string&)override{return false;}bool remove_key_item(std::string_view,std::string&)override{return false;}
 bool show_text(std::string_view,std::string_view,std::string&)override{return false;}bool play_sound(std::string_view,std::string&)override{return false;}
};
}
int main(int argc,char**argv){
 CHECK(argc==3);const std::string fixtures=argv[1],romfs=argv[2];auto file=[&](const char*n){return fixtures+"/"+n;};
 RoomData room;FieldMapData map;WorldLinksData links;FieldStoryData data;ItemData items;
 CHECK(room.load_file(file("mayor-initial-story.encroom").c_str(),error));CHECK(map.load_file(file("mayor-static-actor.encmap").c_str(),error));CHECK(links.load_file(file("mayor-actor-routes.enclinks").c_str(),error));CHECK(items.load_file((romfs+"/ui/opening.encitems").c_str(),error));CHECK(data.load_file(file("mayor-initial.encstory").c_str(),error));
 {
  OpeningWorld turn;CHECK(turn.initialize(room.view()));
  for(Vec2 delta:std::vector<Vec2>{{.5f,-5},{5,-.5f},{-5,.5f},{.5f,5},{-5,-5},{5,5}}){
   const auto p=turn.player().position;CHECK(turn.face_interaction({p.x+delta.x,p.y+delta.y}));const auto d=turn.player().direction;
   CHECK((d.x==0&&std::abs(d.y)==1)||(std::abs(d.x)==1&&d.y==0));CHECK(std::abs(delta.x)>std::abs(delta.y)?d.y==0:d.x==0);CHECK(turn.advance({}));
  }
 }
 const auto story=data.view();CHECK(story.scene_id()==3&&story.programme_count()==4&&story.encounter_count()==0&&story.choices());CHECK(story.bind(room.view(),map.view(),items.view(),error));
 CHECK(story.choices()->groups().size()==1);MickData motion[5];const char*names[]={"mayor-abbott-policy.encmick","mayor-front-policy.encmick","mayor-side-policy.encmick","mayor-nodialog-policy.encmick","mayor-pippi-policy.encmick"};for(unsigned i=0;i<5;++i)CHECK(motion[i].load_file(file(names[i]).c_str(),error));Host host;
 auto flags=[&](std::initializer_list<const char*>enabled){std::vector<bool>v(room.view().flag_count(),false);for(auto id:enabled){bool found=false;for(uint32_t i=0;i<v.size();++i)if(room.view().string(room.view().flag(i).name_string)==id){found=true;v[i]=true;}CHECK(found);}return v;};
 auto prepare=[&](FieldScene&s,unsigned owner,const std::vector<bool>&v){const auto p=motion[owner].view().actor().interact_center;CHECK(s.prepare(room.view(),map.view(),links,v,p,{0,-1},{400,240},error));for(auto&m:motion)CHECK(s.bind_npc(m.view(),host,error));CHECK(s.npcs_ready());CHECK(s.bind_story(story,items.view(),host,error));CHECK(s.finish_transition());};
 auto tick=[&](FieldScene&s){WalkInput input;CHECK(s.before_physics(input));CHECK(s.world.advance(input));CHECK(s.after_physics());CHECK(s.world.idle_frame(1.0/60));CHECK(s.idle_frame(1.0/60,false));CHECK(s.world.end_scene_frame());};
 auto unchanged=[&](FieldScene&s,const std::vector<bool>&v){for(uint32_t i=0;i<v.size();++i)CHECK(s.world.story_flag(room.view().string(room.view().flag(i).name_string))==v[i]);};
 {
  FieldScene s;auto v=flags({});prepare(s,0,v);CHECK(s.try_interaction(true,error));CHECK(s.world.story_talker().kind==DialogueTalkerKind::OriginalNpc&&s.world.story_talker().index==1&&!s.world.actor_bound(1));CHECK(s.world.pending_dialogue_id()==2000);CHECK(s.world.set_story_talking(true));std::set<uint16_t>frames;
  for(unsigned i=0;i<45;++i){tick(s);frames.insert(s.actor_pose(1).frame);CHECK(s.world.pending_dialogue_id()==2000);}CHECK(frames.size()>1);CHECK(s.world.set_story_talking(false));for(unsigned i=0;i<45;++i)tick(s);CHECK(s.world.pending_dialogue_id()==2000);CHECK(s.world.finish_story_dialogue());tick(s);CHECK(s.world.stage()==OpeningStage::Walking&&!s.world.house_paused());unchanged(s,v);
 }
 for(unsigned selected=0;selected<3;++selected){FieldScene s;auto v=flags({});prepare(s,1,v);CHECK(s.try_interaction(true,error));CHECK(s.world.pending_dialogue_id()==2001&&s.world.story_choices_waiting());const auto program=room.view().program(s.world.story_program_index());DialogueChoices choices;
  CHECK(choices.prepare(*story.choices(),s.world.pending_choice_group(),room.view().string(program.source_path_string),program.command_count,error));CHECK(choices.phase()==DialogueChoicesPhase::WaitingText);CHECK(choices.text_completed(error));
  if(selected==1)CHECK(choices.step(1.0/60,{1,0,false,false},error));CHECK(choices.step(1.0/60,{0,0,selected!=2,selected==2},error));DialogueChoicesEvent event;bool received=false;
  while(choices.poll_event(event))if(event.kind==DialogueChoicesEventKind::Selected){CHECK(s.world.choose_story_option(event.target_pc,s.world.story_generation()));CHECK(event.clear_dialogue&&event.sound_after_target);received=true;}CHECK(received);CHECK(s.world.pending_dialogue_id()==(selected==0?2002u:2003u));CHECK(!s.world.story_choices_waiting());CHECK(s.world.finish_story_dialogue());tick(s);CHECK(s.world.stage()==OpeningStage::Walking);unchanged(s,v);
 }
 for(bool badge:{false,true}){FieldScene s;auto v=badge?flags({"courage_badge_received"}):flags({});prepare(s,4,v);CHECK(s.try_interaction(true,error));CHECK(s.world.pending_dialogue_id()==2005);CHECK(s.world.finish_story_dialogue());if(!badge){CHECK(s.world.pending_dialogue_id()==2006);CHECK(s.world.finish_story_dialogue());}tick(s);CHECK(s.world.stage()==OpeningStage::Walking);unchanged(s,v);}
 for(unsigned owner:{0u,1u}){FieldScene s;auto v=owner?flags({"pippi_rescued"}):flags({"zoo_freed"});prepare(s,owner,v);CHECK(s.try_interaction(true,error));CHECK(s.phase()==FieldPhase::Unsupported&&s.world.house_paused()&&s.world.pending_dialogue_id()==UINT32_MAX);CHECK(s.error().find(owner?"mayor_pippi_rescue":"abbott_2")!=std::string::npos);unchanged(s,v);CHECK(s.idle_frame(1.0/60,true));CHECK(s.phase()==FieldPhase::Walking&&!s.world.house_paused());}
 {FieldScene s;auto v=flags({"pippi_delivered","zoo_freed"});prepare(s,2,v);CHECK(s.try_interaction(true,error));CHECK(s.world.pending_dialogue_id()==2004);CHECK(s.world.finish_story_dialogue());tick(s);unchanged(s,v);}
 std::vector<uint8_t>bytes;CHECK(encore::read_file(file("mayor-initial.encstory").c_str(),bytes,128*1024,error));const auto at=get(bytes,64+5*16+4),choice=get(bytes,64+6*16+4);
 auto reject=[&](std::vector<uint8_t>b){sign(b);CHECK(!data.load(b.data(),b.size(),error));CHECK(data.view().choices()&&data.view().choices()->groups().size()==1);};
 for(unsigned cap:{1u,2u,4u}){auto b=bytes;put(b,20,cap);reject(b);}
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{52,6},{at+20,2},{at+24,UINT32_MAX},{at+4,UINT32_MAX},{at+32+4,0},{choice+20,2},{choice+16,0}}){auto b=bytes;put(b,edit.first,edit.second);reject(b);}
 for(size_t n=0;n<bytes.size();n+=7)CHECK(!data.load(bytes.data(),n,error));
 {
  std::vector<uint8_t>b;CHECK(encore::read_file(file("mayor-initial-story.encroom").c_str(),b,1024*1024,error));
  const auto first=get(b,128+(20-1)*24+4);bool found=false;
  // Command section20 is independent of its source programme's choice table.
  for(uint32_t i=0;i<room.view().command_count();++i)if(room.view().command(i).opcode==uint16_t(DialogueActionKind::AwaitChoices)){
   const auto stride=get(b,128+(20-1)*24+12);CHECK(stride==48&&size_t(first)+i*stride+12<=b.size());put(b,first+i*stride+8,1);found=true;break;
  }
  CHECK(found);put(b,52,0);put(b,52,encore::crc32(b.data(),b.size()));RoomData wrong;
  CHECK(wrong.load(b.data(),b.size(),error));CHECK(!story.bind(wrong.view(),map.view(),items.view(),error));
 }
 std::printf("Mayor initial story: %u checks; original unbound NPC speech, yes/no/cancel, Pippi flag branch, unavailable source guards and atomic new capability; host only, scene not staged\n",checks);
}
