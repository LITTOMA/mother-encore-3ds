#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;
#define check(ok,why) do{++checks;if(!(ok)){std::cerr<<"House binding check "<<checks<<" line "<<__LINE__<<": "<<(why)<<'\n';std::exit(1);}}while(false)
struct Trace {std::string source,text,events,actions,audio;Vec2 anchor;uint64_t state=0,draws=0;std::set<uint32_t>seen;uint32_t body=0,actor=0;};
Trace run(const std::string&path,const char*room_path,const char*font_path){
 HouseData data;RoomData room;BattleData font;std::string error;
 check(data.load_file(path.c_str(),error)&&room.load_file(room_path,error)&&font.load_file(font_path,error),error);
 const auto h=data.view();SourceRandom random(123);OpeningWorld world;HousePresentation p;HouseRuntime runtime;
 check(world.initialize(room.view(),{400,240}),world.error());world.attach_random(random);
 check(p.begin(h,font.view(),random),p.error());check(runtime.initialize(h,world,p),runtime.error());
 // Original Phone-ring state selects Carol's source phone hint through the
 // externally preserved House override and Room programme identities.
 const auto carol=h.npc(0);check(h.string(carol.source_path)=="Objects/npc"&&carol.room_actor_index==5&&carol.body_id==9,"Preserved original Carol binding");
 check(world.set_story_flag("phone_ring",true,false),world.error());
 check(world.warp_same_scene({192,724},{0,-1}),world.error());
 const double dt=double(float(1./60));std::ostringstream audio;
 auto step=[&](bool accept,bool cancel){WalkInput input;check(runtime.before_physics(input),runtime.error());check(p.physics_frame(dt,world.player().position),p.error());check(world.advance(input),world.error());check(runtime.after_physics(),runtime.error());check(world.idle_frame(dt),world.error());check(p.idle_frame(dt),p.error());check(runtime.idle_frame(dt,accept,cancel),runtime.error());for(const auto&e:p.take_audio_events())audio<<uint32_t(e.kind)<<':'<<e.voice<<':'<<e.pitch<<';';};
 step(true,false);check(runtime.phase()==HousePhase::StoryRunning,"Actual House ray starts original NPC interaction");
 Trace result;result.source=h.string(carol.source_path);result.body=carol.body_id;result.actor=carol.room_actor_index;bool shown=false;unsigned ticks=0;
 for(;ticks<2400;++ticks){
  const auto pose=p.dialogue_pose();if(pose.visible){shown=true;result.anchor=pose.anchor;for(const auto&line:pose.lines)if(!line.text.empty())result.text=line.text;}
  const bool accept=p.dialogue_active()&&!p.dialogue_closing()&&(p.dialogue_finished()||p.dialogue_stopped());step(accept,!accept);
  if(shown&&!runtime.blocks_player()&&!p.dialogue_active()&&world.stage()==OpeningStage::Walking)break;
 }
 check(shown&&ticks<2400&&!result.text.empty(),"Original Carol source programme prints and returns walking control");
 check(!world.story_flag("doll_defeated")&&!world.story_flag("pillow_attack"),"NPC interaction does not invent battle progress");
 std::ostringstream events,actions;
 for(const auto&e:runtime.events())events<<uint32_t(e.kind)<<':'<<e.object<<':'<<e.physics_tick<<':'<<e.idle_frame<<':'<<e.position.x<<':'<<e.position.y<<';';
 for(const auto&e:world.action_trace()){const auto&a=e.action;actions<<uint32_t(a.kind)<<':'<<a.actor<<':'<<a.phrase<<':'<<a.vector.x<<':'<<a.vector.y<<':'<<a.value<<':'<<a.duration<<':'<<a.target_index<<':'<<a.auxiliary_index<<':'<<a.flags<<':'<<e.physics_tick<<':'<<e.idle_frame<<';';}
 result.events=events.str();result.actions=actions.str();result.audio=audio.str();result.seen=runtime.seen_dialogue_keys();result.state=random.state();result.draws=random.raw_draw_count();
 check(!result.events.empty()&&!result.actions.empty()&&result.draws>0&&!result.seen.empty(),"Real source events, voice RNG and seen identity exercised");return result;
}
}
int main(int argc,char**argv){
 check(argc==4,"House fixture directory, actual Room and font packs required");const std::string directory=argv[1];
 const auto a=run(directory+"/baseline.enchouse",argv[2],argv[3]),b=run(directory+"/adapter.enchouse",argv[2],argv[3]);
 check(a.anchor.x==.5f&&b.anchor.x==.75f&&a.anchor.y==1&&b.anchor.y==1,"Same executable consumes external House anchor change");
 check(a.source==b.source&&a.body==b.body&&a.actor==b.actor&&a.seen==b.seen,"Stable original NPC/save identity unchanged");
 check(a.text==b.text&&a.events==b.events&&a.actions==b.actions&&a.audio==b.audio,"Source text, execution callbacks and audio unchanged");
 check(a.state==b.state&&a.draws==b.draws,"Source shared RNG consumption unchanged");
 HouseData data;std::string error;check(data.load_file((directory+"/adapter.enchouse").c_str(),error),error);check(!data.load_file((directory+"/bad-crc.enchouse").c_str(),error),"Corrupt resource rejected");check(data.view().parameter(HouseParameter::DisplayReference).z==.75f,"Failed checked reload preserves last good House resource");
 std::cout<<"House binding consumer: "<<checks<<" checks passed; actual original NPC program, external adapter and transactional reload\n";
}
