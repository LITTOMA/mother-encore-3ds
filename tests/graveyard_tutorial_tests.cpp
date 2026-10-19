// Full source graph with the real persistent action metadata. No GPU/contact claim.
#include "encore/field_story.hpp"
#include "encore/world.hpp"
#include "encore/field_swing_resources.hpp"
#include "encore/resource_catalog.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Iggy %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
struct Host:StoryKeyItemHost {
 OpeningWorld*w=nullptr;unsigned grants=0;
 bool validate_key_item(std::string_view id,uint32_t n,std::string&e)override{e.clear();return id=="BatOld"&&n==1;}
 bool grant_key_item(std::string_view id,uint32_t n,std::string&e)override{CHECK(w&&!w->story_flag("bat")&&w->phrase()==12);++grants;return validate_key_item(id,n,e);}
};
}
int main(int argc,char**argv){
 CHECK(argc==6);RoomData room;FieldStoryData story;FieldMapData map;ItemData items;ResourceCatalog catalog;FieldSwingResources player;
 CHECK(catalog.load_file((std::string(argv[5])+"/system/native.encresources").c_str(),error)&&player.load(catalog,argv[5],error));
 CHECK(room.load_file(argv[1],error)&&story.load_file(argv[2],error)&&map.load_file(argv[3],error)&&items.load_file(argv[4],error));
 const auto r=room.view();const auto s=story.view();CHECK(r.program_count()==7&&s.programme_count()==7&&s.text_count()==56&&s.bind(r,map.view(),items.view(),error));
 const auto programme=s.programme(6);CHECK(s.text(programme.boundary)=="Cutscenes/Cutscene Area5"&&s.text(programme.source)=="Podunk/cutscenes/gravedigger_swing");
 CHECK(s.choices()&&s.choices()->groups().size()==2);const auto&g=s.choices()->groups()[1];CHECK(g.cancel_target_pc==g.options[1].target_pc&&g.options[0].target_pc!=g.cancel_target_pc&&g.program_command_count==83);
 for(auto locale:{"en","zh_Hans_CN"}){auto line=s.dialogue(3111,locale);CHECK(line.id==3111&&s.text(line.voice).empty()&&s.text(line.speaker).empty()&&!s.text(line.body).empty());line=s.dialogue(3113,locale);CHECK(s.text(line.body).find("[ui_cancel]")!=std::string_view::npos);}
 for(unsigned branch=0;branch<3;++branch){
  OpeningWorld w;CHECK(w.initialize(r)&&w.warp_same_scene({1300,4712},{0,1})&&w.bind_player_action(player,error));Host h;h.w=&w;CHECK(w.bind_story_key_items(h,error));CHECK(w.begin_house_program(6));unsigned choices=0,seen=0,final_ticks=0;uint32_t last=UINT32_MAX;bool saw_minimum=false,saw_final=false;
  auto frame_step=[&]{CHECK(w.advance({})&&w.idle_frame(1.0/60)&&w.end_scene_frame());for(auto event:w.take_player_action_events())CHECK(event==PlayerActionEvent::Pause||event==PlayerActionEvent::Resume);};
  for(unsigned tick=0;tick<6000&&!w.story_completed();++tick){
   frame_step();
   if(w.phrase()==15&&!saw_final){CHECK(w.story_flag("bat")&&h.grants==1);saw_final=true;}
   if(saw_final)++final_ticks;
   const auto id=w.pending_dialogue_id();
   if(id!=UINT32_MAX){
    CHECK(s.dialogue(id,"en").id==id);
    if(id!=last){last=id;++seen;for(unsigned hold=0;hold<6;++hold){frame_step();CHECK(w.pending_dialogue_id()==id);}}
    if(id==3104&&!w.story_input_allowed())saw_minimum=true;
    if(w.story_input_allowed()){const bool ok=w.finish_story_dialogue();if(!ok)std::fprintf(stderr,"branch=%u tick=%u id=%u phrase=%u pc=%u world=%s\n",branch,tick,id,w.phrase(),w.story_next_command_index(),w.error());CHECK(ok);}
   }
   if(w.story_choices_waiting()&&id==w.pending_dialogue_id()){
    CHECK(w.pending_choice_group()==1&&!w.story_flag("bat")&&h.grants==0);const auto generation=w.story_generation();
    CHECK(!w.choose_story_option(g.options[0].target_pc,generation+1));
    const auto target=choices==0&&branch? (branch==1?g.options[1].target_pc:g.cancel_target_pc):g.options[0].target_pc;
    CHECK(w.choose_story_option(target,generation));++choices;
   }
  }
  CHECK(w.story_completed()&&w.stage()==OpeningStage::Walking&&saw_minimum&&saw_final&&h.grants==1&&w.story_flag("bat"));
  std::printf("Iggy branch%u: choices=%u text-visits=%u final-ticks=%u\n",branch,choices,seen,final_ticks);
  CHECK(choices==(branch?2u:1u)&&seen==(branch?15u:13u)&&final_ticks>=180);
  CHECK(std::abs(w.player().position.x-1432)<.001&&std::abs(w.player().position.y-4719)<.001);
  CHECK(std::abs(w.actor(3).position.x-1456)<.001&&std::abs(w.actor(3).position.y-4744)<.001&&w.actor(3).direction.x==0&&w.actor(3).direction.y==-1);
  // actor.gd:update_npcs yields idle_frame before releasing the replaced
  // player. Dialogue completion alone is not the control handoff boundary.
  CHECK(w.actor_cleanup_pending()&&w.actor_bound(r.scene().player_instance_index));
  bool accepted=false;CHECK(w.player_action_input(player.action().strings()[2],true,w.story_flag("bat"),false,false,accepted,error)&&!accepted);
  CHECK(w.idle_frame(0)&&w.actor_cleanup_pending()&&w.actor_bound(r.scene().player_instance_index));
  frame_step();CHECK(!w.actor_cleanup_pending()&&!w.actor_bound(r.scene().player_instance_index));
  // The newly learned button works after the source's deferred handoff on
  // every yes/no/cancel path, without replaying the grant.
  CHECK(w.player_action_input(player.action().strings()[2],true,w.story_flag("bat"),false,false,accepted,error)&&accepted);auto events=w.take_player_action_events();CHECK(!events.empty()&&events.back()==PlayerActionEvent::Start);const auto position=w.player().position;
  for(unsigned frame=0;frame<80&&w.player_action().attacking();++frame){CHECK(w.advance({1,0,false,false,false})&&w.idle_frame(1.0/60)&&w.end_scene_frame());CHECK(w.player().position.x==position.x&&w.player().position.y==position.y);}
  CHECK(!w.player_action().attacking()&&h.grants==1);
 }
 std::printf("Iggy tutorial: %u checks; full 16-phrase graph, yes/no/cancel, minimum wait, grant before later flag, two-actor paths, persistent control restoration and newly unlocked Bat; shared core, no GPU/contact claim\n",checks);
}
