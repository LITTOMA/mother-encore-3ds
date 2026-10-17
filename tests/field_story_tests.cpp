#include "encore/field_story.hpp"
#include "encore/field_scene.hpp"
#include "encore/battle_outcome.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
using namespace encore::upstream;
namespace {
unsigned checks=0;void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Field story check %u: %s\n",checks,why);std::exit(1);}}
void check(bool ok,const std::string& why){check(ok,why.c_str());}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
void put(std::vector<uint8_t>&b,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(n>>(8*i));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
size_t section(const std::vector<uint8_t>&b,unsigned k){return get(b,64+(k-1)*16+4);}
struct Admission:FieldStoryHost {bool allowed=true;unsigned calls=0;bool can_start(FieldStoryView,uint32_t,std::string&e)override{++calls;if(!allowed){e="Controlled unsupported live session";return false;}e.clear();return true;}};
}
int main(int argc,char**argv){
 check(argc==9,"story room map links items introduction round entry paths required");std::string error;std::vector<uint8_t>bytes;check(encore::read_file(argv[1],bytes,128*1024,error),error.c_str());FieldStoryData data;check(data.load(bytes.data(),bytes.size(),error),error.c_str());const auto view=data.view();RoomData room;FieldMapData map;WorldLinksData links;ItemData items;IntroductionData introduction;
 check(room.load_file(argv[2],error)&&map.load_file(argv[3],error)&&links.load_file(argv[4],error)&&items.load_file(argv[5],error)&&introduction.load_file(argv[6],error),error.c_str());check(view.bind(room.view(),map.view(),items.view(),error),error.c_str());
 check(view.programme_count()==2&&view.text_count()==10&&view.encounter_count()==1&&view.override_count()==1,"scoped source bridge and call bindings");
 check(view.text(view.encounter(4).basic)=="Data/BattleSkills/attack.yaml"&&view.text(view.basic_override(3).source)=="Data/BattleSkills/attack_bat.yaml","source weapon override identity");
 check(view.dialogue(1000,"en").id==1000&&view.dialogue(1004,"zh_Hans_CN").id==1004&&!view.dialogue(1000,"invented").id,"both admitted locales; no implicit unknown locale");
 {std::vector<uint32_t>emotes;for(uint32_t i=0;i<room.view().command_count();++i){const auto c=room.view().command(i);if(c.opcode==uint16_t(DialogueActionKind::EmoteActor)&&c.actor_index==2)emotes.push_back(c.target_index);}check(emotes.size()==2,"two source dog emotes");ActorActionState actor;check(initialize_actor(actor,room.view(),2,{512,272},{-1,0}),"dog emote fixture");check(actor_play_emote(actor,emotes[0]),"first source emote");check(!actor_play_emote(actor,emotes[1]),"unreviewed active interruption stays rejected");for(unsigned i=0;i<60;++i)check(actor_idle_animations(actor,1.0/60),"source emote clock");check(actor.emote_playing&&actor.emote_frame==64,"finished exclamation retains original last frame");check(actor_play_emote(actor,emotes[1])&&actor_idle_animations(actor,1.0/60)&&actor.emote_frame==85,"finished source animation can be replaced by angry");}
 auto reject=[&](std::vector<uint8_t>b,const char*why){sign(b);check(!data.load(b.data(),b.size(),error),why);check(data.view().programme_count()==2,"failed load preserves current owner");};
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,2},{28,1},{52,4},{56,1},{60,1},{64,2},{64+12,2},{64+8,0}}){auto b=bytes;put(b,edit.first,edit.second);reject(b,"new header/directory/version negative path");}
 const auto programmes=section(bytes,2),texts=section(bytes,3),encounters=section(bytes,4),overrides=section(bytes,5);
 for(float value:{0.f,-1.f,61.f,INFINITY,NAN}){auto b=bytes;uint32_t bits;std::memcpy(&bits,&value,4);put(b,encounters+32,bits);reject(b,"invalid return music duration rejected");}
 for(auto value:{0u,4u,UINT32_MAX}){auto b=bytes;put(b,encounters+36,value);reject(b,"unknown return music curve rejected");}
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{programmes+20,1},{programmes+24,0},{texts,0},{texts+8,0xffff},{texts+20,UINT32_MAX},{encounters,0},{encounters+4,0},{encounters+24,16},{overrides,0}}){auto b=bytes;put(b,edit.first,edit.second);reject(b,"invalid programme/text/encounter/override rejected");}
 {auto b=bytes;put(b,encounters+28,1);sign(b);FieldStoryData changed;check(changed.load(b.data(),b.size(),error),error.c_str());check(!changed.view().bind(room.view(),map.view(),items.view(),error),"encounter cannot borrow the nonbattle programme");}
 std::vector<bool>flags(room.view().flag_count(),false);Admission host;FieldScene field;check(field.prepare(room.view(),map.view(),links,flags,{440,272},{1,0},{400,240},error),error.c_str());check(field.bind_story(view,items.view(),host,error),error.c_str());check(field.finish_transition(),field.error());
 SourceRandom random(991);field.world.attach_random(random);check(field.world.warp_same_scene({450,272},{1,0}),"source bridge boundary fixture");host.allowed=false;const auto rng=random.state();check(field.after_physics(),field.error());check(host.calls==1&&field.phase()==FieldPhase::Unsupported&&random.state()==rng&&!field.world.story_flag("beat_dog_bridge"),"unsupported preflight does not consume RNG or advance story");check(field.idle_frame(1.0/60,true),field.error());host.allowed=true;
 check(field.world.warp_same_scene({450,272},{1,0})&&field.after_physics(),field.error());check(field.world.stage()==OpeningStage::ScriptRunning&&host.calls==2,"source area dispatches checked bridge programme");
 for(unsigned tick=0;tick<600&&!field.world.battle_request().requested;++tick){const bool ok=field.world.advance({})&&field.world.idle_frame(1.0/60);if(!ok){const auto&trace=field.world.action_trace();std::fprintf(stderr,"Bridge tick=%u phrase=%u trace=%u last=%u\n",tick,field.world.phrase(),unsigned(trace.size()),trace.empty()?999u:unsigned(trace.back().action.kind));}check(ok,field.world.error());}
 check(field.world.battle_request().requested&&field.world.battle_request().actor_index==2&&field.world.content().battle(field.world.battle_request().record_index).stable_id==4,"bridge cutscene queues actual dog encounter");check(!field.world.story_flag("beat_dog_bridge"),"bridge programme cannot grant a win");

 BattleRoundData round_data;BattleData battle_data;check(round_data.load_file(argv[7],error)&&battle_data.load_file(argv[8],error),error.c_str());const auto round_view=round_data.view();const auto battle_view=battle_data.view();
 struct Loot:BattleLootHost {unsigned grants=0,sounds=0;bool validate_sound(std::string_view,std::string&e)const override{e.clear();return true;}bool can_receive(std::string_view,uint32_t,std::string&e)const override{e.clear();return true;}bool reserve(std::string_view,uint32_t,SourceRandom&r,uint32_t&uid,std::string&e)override{uid=r.randi();e.clear();return true;}bool grant(std::string_view,uint32_t,uint32_t,std::string&e)override{++grants;e.clear();return true;}bool play_sound(std::string_view,std::string&e)override{++sounds;e.clear();return true;}}loot_host;
 BattleLoot loot;check(loot.prepare(round_view,random,loot_host,error),error.c_str());
 BattleSessionStats stats;stats.level=2;stats.experience=11;stats.hp=stats.maxhp=65;stats.pp=stats.maxpp=27;stats.offense=15;stats.defense=12;stats.speed=6;stats.iq=6;stats.guts=8;stats.learned_skills={"strike","splitShot","telepathy"};
 check(BattleOutcome::preflight(round_view,room.view(),&stats,error),error.c_str());
 BattleEntry entry;BattleEntrySnapshot snapshot{{180,120},{240,120},false,field.world.actor(2).frame};check(entry.begin(battle_view,room.view(),field.world.battle_request(),snapshot),entry.error());check(field.world.accept_battle_entry(),"source field battle entry acknowledgement");
 for(unsigned tick=0;tick<600&&entry.phase()!=BattleEntryPhase::Commands;++tick)check(entry.idle_frame(1.0/60,false),entry.error());check(entry.phase()==BattleEntryPhase::Commands,"original entrance reaches command menu");
 BattleActionPresentation presentation;check(presentation.begin(round_view,battle_view,random,&stats),presentation.error());
 for(uint32_t i=0;i<battle_view.count(BattleSection::Layouts);++i){const auto layout=battle_view.layout(i);auto pose=entry.pose(i);if(layout.flags&2){pose.rect.x-=pose.rect.z/2;pose.rect.y-=pose.rect.w/2;}if(layout.role==uint32_t(BattleRole::PartySprite))check(presentation.set_actor_base(round_view.binding().player_participant,pose),"source party pose");if(layout.role==uint32_t(BattleRole::EnemySprite))check(presentation.set_actor_base(round_view.binding().enemy_participant,pose),"source enemy pose");if(layout.role==uint32_t(BattleRole::PartyPlate)&&layout.kind==uint32_t(BattleDrawKind::NinePatch))check(presentation.set_plate_base(pose.rect),"source plate pose");}
 WorldBattleHost round_host;round_host.bind(presentation,field.world,round_view);BattleRound round;BattleOutcome outcome;check(outcome.initialize(round_view,room.view(),&stats,&loot),outcome.error());check(round.begin(round_view,battle_view,random,round_host,&stats,"Data/BattleSkills/attack_bat.yaml"),round.error());
 bool victorious=false;
 for(unsigned tick=0;tick<18000&&outcome.phase()!=BattleOutcomePhase::Complete;++tick){
  if(round.phase()==BattleRoundPhase::Commands)check(round.request_menu(round_view.binding().basic_menu),round.error());if(round.phase()==BattleRoundPhase::Targeting)check(round.target_input(0,true),round.error());
  if(presentation.dialogue().finished())presentation.input(true,false);
  const double delta=double(float((1.0/60)*presentation.advance_real_time(1.0/60)));check(presentation.physics_frame(delta)&&presentation.idle_frame(delta),presentation.error());check(round.idle_frame(delta),round.error());
  if(round.phase()==BattleRoundPhase::DefeatPending){std::fprintf(stderr,"controlled bridge seed defeated at tick %u\n",tick);break;}
  if(round.phase()==BattleRoundPhase::VictoryPending&&outcome.phase()==BattleOutcomePhase::Idle){victorious=true;check(!field.world.instance_visible(2)&&!field.world.story_flag("beat_dog_bridge"),"actual defeat erases dynamic actor before reward flag");check(outcome.begin(round,presentation,field.world),outcome.error());}
  for(const auto event:presentation.take_return_events()){
   if(event==RoundEventKind::HideBattleBackground)check(field.world.resume_battle_camera(),"field return camera");
   else if(event==RoundEventKind::JumpPartyToWorld){const auto c=field.world.cutscene_camera().center(),p=field.world.player().position;check(presentation.begin_party_return({p.x-c.x+200,p.y-c.y+120},{80,60}),"field party return jump");}
   else if(event==RoundEventKind::PartyReturnLanded){const auto turn=round_view.parameter(RoundParameter::ReturnPartyTurn);check(field.world.land_battle_player({turn.x,turn.y},uint16_t(round_view.parameter(RoundParameter::ReturnPartyFrames).y)),"field return landing");}
   else if(event==RoundEventKind::RotatePartyOriginal)check(field.world.rotate_battle_player(round_view.parameter(RoundParameter::ReturnPartyTurn).z),"field return direction");
  }
  check(outcome.idle_frame(),outcome.error());const bool ok=field.world.advance({})&&field.world.idle_frame(delta);check(ok,field.world.error());
 }
 check(victorious&&outcome.phase()==BattleOutcomePhase::Complete&&field.world.stage()==OpeningStage::Walking,"seeded source battle returns control to actual field owner");check(outcome.state().experience==19&&outcome.state().bank==20&&outcome.state().earned_cash==20&&field.world.story_flag("beat_dog_bridge"),"source 8 EXP/20 cash and bridge flag commit once");check(loot_host.grants==unsigned(loot.selected())&&loot_host.sounds==unsigned(loot.selected()),"controlled loot interface grants and sounds exactly once");
 for(unsigned tick=0;tick<300&&field.world.stage()==OpeningStage::Walking;++tick){WalkInput input{1,0};check(field.before_physics(input),field.error());const bool ok=field.world.advance(input)&&field.after_physics()&&field.world.idle_frame(1.0/60);check(ok,field.world.error());check(field.phase()!=FieldPhase::Unsupported,"no bridge boundary after actual victory");}
 check(field.world.stage()==OpeningStage::ScriptRunning&&field.world.story_program_index()==1,"ordinary eastward movement reaches original mom call after real win");
 unsigned call_lines=0;uint32_t call_previous=UINT32_MAX;
 for(unsigned tick=0;tick<2400&&field.world.stage()!=OpeningStage::Walking;++tick){const bool ok=field.world.advance({})&&field.world.idle_frame(1.0/60);check(ok,field.world.error());const auto id=field.world.pending_dialogue_id();if(id!=UINT32_MAX&&id!=call_previous){call_previous=id;++call_lines;}if(field.world.story_input_allowed())check(field.world.finish_story_dialogue(),"normal call acknowledgement");}
 check(call_lines==5&&field.world.story_flag("pippis_mom_call")&&field.world.stage()==OpeningStage::Walking,"battle, source boundary and all five call phrases form one playable chain");
 // Continue by ordinary physics input onto the newly connected west road.
 // These are test waypoints derived from the unchanged original collision map.
 const auto walk_to=[&](Vec2 target){
  unsigned tick=0;for(;tick<1600;++tick){const auto at=field.world.player().position;const float dx=target.x-at.x,dy=target.y-at.y;if(std::abs(dx)<.1f&&std::abs(dy)<.1f)break;WalkInput input{};if(std::abs(dx)>=.1f)input.x=dx>0?1:-1;else input.y=dy>0?1:-1;
   check(field.before_physics(input)&&field.world.advance(input)&&field.after_physics()&&field.world.idle_frame(1.0/60),field.world.error());check(field.phase()==FieldPhase::Walking&&field.world.stage()==OpeningStage::Walking,"ordinary road movement does not reenter completed source triggers");}
  if(tick==1600){const auto at=field.world.player().position;std::fprintf(stderr,"blocked road waypoint %.0f,%.0f at %.3f,%.3f\n",target.x,target.y,at.x,at.y);}
  check(tick<1600,"post-battle west-road waypoint is reachable without changing position/flags");
 };
 for(const Vec2 target:std::vector<Vec2>{{536,272},{536,480},{408,480},{408,488},{400,488},{400,512},{344,512},{344,520},{336,520},{336,528},{184,528},{184,536},{176,536},{176,560},{-560,560},{-560,496}})walk_to(target);
 check(field.world.player().position.x<=-558&&field.world.player().position.y<=498,"normal movement connects bridge win and mom call to west road");
 const auto preflights=host.calls,grants=loot_host.grants,sounds=loot_host.sounds;
 for(const Vec2 target:std::vector<Vec2>{{-560,560},{176,560},{176,536},{184,536},{184,528},{336,528},{336,520},{344,520},{344,512},{400,512},{400,488},{408,488},{408,480},{536,480},{536,272},{400,272},{600,272},{536,272}})walk_to(target);
 check(host.calls==preflights&&loot_host.grants==grants&&loot_host.sounds==sounds&&outcome.state().experience==19&&outcome.state().bank==20&&outcome.state().earned_cash==20&&field.world.story_flag("beat_dog_bridge")&&field.world.story_flag("pippis_mom_call"),"repeated normal trigger crossings cannot replay battle/call or grant rewards twice");
 // An independent source programme fixture checks all five player-controlled
 // lines and the final movement; combat-to-call reachability is tested separately.
 OpeningWorld call;check(call.initialize_restored(room.view(),flags,std::vector<bool>(flags.size(),true),{510,272},{1,0}),call.error());call.attach_random(random);check(call.begin_house_program(1),call.error());unsigned lines=0;uint32_t previous=UINT32_MAX;
 for(unsigned tick=0;tick<2400&&call.stage()!=OpeningStage::Walking;++tick){const bool ok=call.advance({})&&call.idle_frame(1.0/60);check(ok,call.error());const auto id=call.pending_dialogue_id();if(id!=UINT32_MAX&&id!=previous){check(view.dialogue(id,"en").id==id,"source call text binding");previous=id;++lines;}if(call.story_input_allowed())check(call.finish_story_dialogue(),"fresh text completion continuation");}
 check(lines==5&&call.stage()==OpeningStage::Walking&&call.story_flag("pippis_mom_call"),"call completes exactly five source lines and one flag");check(call.actor(1).position.x==640&&call.actor(1).position.y==304,"mom finishes source step sequence at 640,304");
 std::printf("Field story: %u checks; strict v1 parser, binding, preflight rollback and original typed programmes; not an emulator/hardware run\n",checks);
}
