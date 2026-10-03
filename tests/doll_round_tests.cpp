#include "encore/battle_outcome.hpp"
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <fstream>
#include <algorithm>
using namespace encore::upstream;
unsigned checks=0;
#define check(v,e) do {++checks;if(!(v)){std::cerr<<"check "<<checks<<" line "<<__LINE__<<": "<<(e)<<'\n';std::exit(1);}} while(false)
namespace {
void bases(BattleActionPresentation&p,RoundView r,BattleView e){for(uint32_t i=0;i<e.count(BattleSection::Layouts);++i){auto l=e.layout(i);BattlePose q{l.rect,l.color,l.frame,true};if(l.flags&2){q.rect.x-=q.rect.z/2;q.rect.y-=q.rect.w/2;}if(l.role==uint32_t(BattleRole::PartySprite)){q.rect.y-=r.parameter(RoundParameter::PartyShown).x;check(p.set_actor_base(0,q),p.error());}if(l.role==uint32_t(BattleRole::EnemySprite))check(p.set_actor_base(1,q),p.error());if(l.role==uint32_t(BattleRole::PartyPlate)&&l.kind==uint32_t(BattleDrawKind::NinePatch))check(p.set_plate_base(q.rect),p.error());}}
}
int main(int argc,char**argv){
 check(argc==4,"Doll round, entry and room paths required");
 // Exercise inherited area ownership, fresh-world no-owner, and data-disabled cleanup.
 for(unsigned scenario=0;scenario<3;++scenario){
 const bool lamp_setup=scenario!=1,cleanup_enabled=scenario!=2;BattleRoundData data;BattleData entry_data;RoomData room_data;std::string error;
 check(data.load_file(argv[1],error),error.c_str());check(entry_data.load_file(argv[2],error),error.c_str());check(room_data.load_file(argv[3],error),error.c_str());
 std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>original((std::istreambuf_iterator<char>(in)),{});
 auto u32=[](const std::vector<uint8_t>&v,size_t o){return uint32_t(v[o])|uint32_t(v[o+1])<<8|uint32_t(v[o+2])<<16|uint32_t(v[o+3])<<24;};
 auto put=[](std::vector<uint8_t>&v,size_t o,uint32_t n){for(unsigned i=0;i<4;++i)v[o+i]=uint8_t(n>>(8*i));};
 auto fix=[&](std::vector<uint8_t>&bytes){put(bytes,16,0);uint32_t crc=~0u;for(auto byte:bytes){crc^=byte;for(unsigned j=0;j<8;++j)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));}put(bytes,16,~crc);};
 // Preserve these legacy v4 rejection and exact-baseline expectations even
 // when the production Doll adapter emits conditional-progression schema5.
 // The dedicated Pillow runtime test exercises the production v5 contract.
 for(auto offset:{8,24,28})put(original,offset,4);
 fix(original);check(data.load(original.data(),original.size(),error),error.c_str());
 auto reject=[&](size_t offset,uint32_t value){auto bad=original;put(bad,offset,value);fix(bad);check(!data.load(bad.data(),bad.size(),error),"Invalid v4 pack rejected");check(data.view().binding().battle_id==2,"Rejected v4 load preserves prior owner");};
 const auto encounter=u32(original,64+14*16+4),growth=u32(original,64+15*16+4),shakes=u32(original,64+16*16+4);
 for(auto change:std::vector<std::pair<size_t,uint32_t>>{{8,5},{20,16},{24,2},{28,2},{encounter,0},{encounter+4,0},{encounter+8,0},{encounter+12,99999},{encounter+16,1},{encounter+20,10},{encounter+24,0},{encounter+28,0},{encounter+32,0},{encounter+36,2},{encounter+36,0xffffffff},{growth,8},{growth+4,99},{growth+8,0},{growth+12,0},{shakes+4,0},{shakes+12,0}})reject(change.first,change.second);
 check(!data.load(original.data(),335,error),"Truncated v4 directory rejected");
 // Legacy v3 is a distinct 36-byte encounter schema, never a v4 fallback.
 auto legacy=original;legacy.erase(legacy.begin()+encounter+36,legacy.begin()+encounter+40);
 for(auto offset:{8,24,28})put(legacy,offset,3);
 put(legacy,12,uint32_t(legacy.size()));put(legacy,64+14*16,15u|(36u<<16));put(legacy,64+14*16+12,36);
 for(unsigned section=15;section<17;++section){const auto offset=64+section*16+4;put(legacy,offset,u32(legacy,offset)-4);}fix(legacy);
 BattleRoundData old_data;check(old_data.load(legacy.data(),legacy.size(),error),error.c_str());check(old_data.view().encounter().stop_area_music_if_overworld==0,"Explicit v3 has no return cleanup policy");
 auto wrong_stride=original;put(wrong_stride,64+14*16,15u|(36u<<16));fix(wrong_stride);check(!old_data.load(wrong_stride.data(),wrong_stride.size(),error),"v4 cannot reinterpret legacy stride");
 if(!cleanup_enabled){auto changed=original;put(changed,encounter+36,0);fix(changed);check(data.load(changed.data(),changed.size(),error),error.c_str());}
 auto r=data.view();auto e=entry_data.view();auto room=room_data.view();auto b=r.binding();auto policy=r.encounter();
 check(b.battle_id==e.metadata().room_battle_id&&b.battle_id==2,"Separate Doll binding");check(r.enemy_choice(0).weight==5&&r.enemy_choice(1).weight==1,"Source Doll weighted AI");
 check(policy.stop_area_music_if_overworld==uint32_t(cleanup_enabled),"External encounter owns the return music cleanup policy");
 check(r.string(policy.post_win_script)=="Podunk/cutscenes/doll_defeated"&&policy.keep_actor&&policy.boss,"Typed post-win source/retention");
 BattleSessionStats session;auto player=e.participant(0);session.experience=3;session.level=1;session.bank=5;session.earned_cash=5;session.hp=51;session.pp=player.pp;session.maxhp=player.maxhp;session.maxpp=player.maxpp;session.offense=player.offense;session.defense=player.defense;session.speed=player.speed;session.iq=player.iq;session.guts=player.guts;session.learned_skills={"strike","splitShot"};
 SourceRandom random(0);BattleActionPresentation p;check(p.begin(r,e,random,&session),p.error());bases(p,r,e);check(p.hp().current_hp()==51,"Presentation retains session HP");
 OpeningWorld world;check(world.initialize(room),world.error());world.attach_random(random);const double dt=double(float(1.0/60));
 if(lamp_setup){
  for(unsigned n=0;n<1200&&world.stage()!=OpeningStage::BattleRequested;++n)check(world.advance({-1,0})&&world.idle_frame(dt),world.error());
  check(world.stage()==OpeningStage::BattleRequested&&world.battle_request().overworld_music,"Lamp source leaves global overworld battle music enabled");
  check(world.area_music_resource()!=kRoomNoIndex,"Original deferred Lamp area callback acquires ownership");
  // Seed the already-covered Lamp reward/return boundary through public operations.
  check(world.accept_battle_entry()&&world.idle_frame(dt)&&world.set_battle_story_flag(),world.error());
  check(world.land_battle_player({0,-1},uint16_t(r.parameter(RoundParameter::ReturnPartyFrames).y))&&world.finish_battle_return(),world.error());
 }
 const auto owned_music=world.area_music_resource();
 auto stop_count=[&]{return std::count_if(world.audio_requests().begin(),world.audio_requests().end(),[](const OpeningAudioRequest&request){return request.kind==AudioRequestKind::StopMusicResource;});};
 check(stop_count()==0,"Area music remains owned until the source return boundary");
 check(world.warp_same_scene({64,128},{0,-1}),world.error());check(world.begin_house_program(1),world.error());
 unsigned text_frames=0;for(unsigned n=0;n<3000&&world.stage()!=OpeningStage::BattleRequested;++n){if(world.pending_dialogue_id()!=kRoomNoIndex){if(++text_frames>=180){check(world.finish_story_dialogue(),world.error());text_frames=0;}}check(world.advance({})&&world.idle_frame(dt),world.error());}
 check(world.stage()==OpeningStage::BattleRequested&&world.battle_request().keep_actor_after_battle,"Original Doll program requests retained actor encounter");check(world.battle_request().overworld_music==lamp_setup,"Doll inherits the existing global music state");check(world.accept_battle_entry()&&world.idle_frame(dt),world.error());
 WorldBattleHost host;host.bind(p,world,r);BattleRound round;check(round.begin(r,e,random,host,&session),round.error());
 check(round.battler(0).target_hp==51,"Round keeps HP instead of healing to baseline");auto draws=random.raw_draw_count();check(round.request_menu(b.items_menu)&&round.phase()==BattleRoundPhase::Items,"Items is an interactive phase");check(random.raw_draw_count()==draws&&round.decisions().empty(),"Opening Items neither queues action nor draws RNG");check(round.return_from_items()&&round.phase()==BattleRoundPhase::Commands&&round.take_menu_return(),"Items cancel restores command menu");
 check(round.request_menu(b.basic_menu)&&round.target_input(0,false,true)&&round.phase()==BattleRoundPhase::Commands,"Bash target cancel works");check(random.raw_draw_count()==draws,"Target cancel preserves RNG");
 check(round.request_menu(b.guard_menu),round.error());check(round.battler(0).defending,"Defend enters source guard state");
 auto tick=[&]{double delta=double(float(dt*p.advance_real_time(dt)));check(p.physics_frame(delta)&&p.idle_frame(delta),p.error());check(round.idle_frame(delta),round.error());return delta;};
 for(unsigned n=0;n<3000&&round.phase()==BattleRoundPhase::Running;++n)tick();check(round.phase()==BattleRoundPhase::Commands,"Doll responds to Defend and returns menu");
 unsigned turns=0,boss_frames=0;double boss_elapsed=0;int32_t stopped_hp=-1;bool boss_started=false;uint64_t before_growth=0;
 while(round.phase()!=BattleRoundPhase::VictoryPending&&turns++<10){check(round.phase()==BattleRoundPhase::Commands,"Conscious party continues");check(round.request_menu(b.basic_menu)&&round.target_input(0,true),round.error());
  for(unsigned n=0;n<4000&&round.phase()==BattleRoundPhase::Running;++n){const auto delta=tick();if(round.battler(1).target_hp==0){if(!boss_started){boss_started=true;stopped_hp=p.hp().current_hp();}else{++boss_frames;boss_elapsed+=delta;check(p.hp().current_hp()==stopped_hp,"Boss defeat stops rolling HP immediately");}if(boss_frames<500)check(round.phase()!=BattleRoundPhase::VictoryPending,"Boss visual callback cannot skip to victory");}}
 }
 check(round.phase()==BattleRoundPhase::VictoryPending&&boss_elapsed>=8.96&&boss_elapsed<=9.04,"Separate 4.5s + 4.5s boss callback controls victory");check(world.instance_visible(world.battle_request().actor_index),"Doll overworld actor retained at defeat");check(!world.story_flag("doll_defeated"),"Defeat alone cannot set post-win script flags");
 BattleOutcome outcome;check(outcome.initialize(r,room,&session),outcome.error());check(outcome.begin(round,p,world),outcome.error());before_growth=random.raw_draw_count();
 auto frame=[&]{check(world.advance({})&&world.idle_frame(dt),world.error());check(p.physics_frame(dt)&&p.idle_frame(dt),p.error());for(auto kind:p.take_return_events()){if(kind==RoundEventKind::HideBattleBackground)check(world.resume_battle_camera(),world.error());if(kind==RoundEventKind::JumpPartyToWorld){auto c=world.cutscene_camera().center();check(p.begin_party_return({world.player().position.x-c.x+200,world.player().position.y-c.y+120},{80,60}),p.error());}if(kind==RoundEventKind::PartyReturnLanded){auto turn=r.parameter(RoundParameter::ReturnPartyTurn);check(world.land_battle_player({turn.x,turn.y},uint16_t(r.parameter(RoundParameter::ReturnPartyFrames).y)),world.error());}if(kind==RoundEventKind::RotatePartyOriginal)check(world.rotate_battle_player(r.parameter(RoundParameter::ReturnPartyTurn).z),world.error());}check(outcome.idle_frame(),outcome.error());};
 for(unsigned n=0;n<1000&&outcome.phase()!=BattleOutcomePhase::ExperienceDialogue;++n)frame();check(outcome.phase()==BattleOutcomePhase::ExperienceDialogue,"Boss victory opens source EXP text");
 for(unsigned n=0;n<300;++n)frame();check(outcome.state().experience==3&&outcome.state().bank==5,"EXP and bank wait for real acknowledgement");check(stop_count()==0&&world.area_music_resource()==owned_music,"Music cleanup waits for acknowledgments before return");p.input(false,true);check(outcome.idle_frame(),outcome.error());check(outcome.phase()==BattleOutcomePhase::LevelDialogue,"EXP acknowledgement enters growth dialogue");
 check(outcome.state().experience==11&&outcome.state().level==2&&outcome.state().bank==5,"Growth committed before cash and after EXP acknowledgement");check(outcome.state().maxhp==65&&outcome.state().maxpp==27&&outcome.state().offense==12&&outcome.state().defense==12&&outcome.state().speed==6&&outcome.state().iq==6&&outcome.state().guts==8,"Source level2 deterministic effective stats");check(outcome.state().hp==stopped_hp+3&&outcome.state().pp==27,"Source maximum growth restores only positive gain");check(p.current_pp()==27,"Source refresh_battle_plate updates PP presentation after growth");
 unsigned prompts=0;while(outcome.phase()==BattleOutcomePhase::LevelDialogue&&prompts++<10){for(unsigned n=0;n<300;++n)frame();check(outcome.phase()==BattleOutcomePhase::LevelDialogue&&outcome.state().bank==5,"Each growth/learning line waits for input");check(stop_count()==0&&world.area_music_resource()==owned_music,"Area owner survives each level prompt");p.input(true,false);check(outcome.idle_frame(),outcome.error());}
 check(prompts==6&&outcome.phase()==BattleOutcomePhase::Returning,"Level + four changed stats + Telepathy acknowledged");check(outcome.state().bank==15&&outcome.state().earned_cash==15&&outcome.state().cash==0,"Source Doll cash adds to Lamp bank");check(outcome.state().learned_skills.size()==3&&outcome.state().learned_skills.back()=="telepathy","Field skill retained in session");
 const bool should_stop=lamp_setup&&cleanup_enabled;
 check(stop_count()==int(should_stop),"Return cleanup requires both external policy and inherited overworld music");
 check(world.area_music_resource()==(should_stop?kRoomNoIndex:owned_music),"Return clears only the owned area music attachment");
 const auto&reward_events=outcome.events();const auto count=reward_events.size();
 check(reward_events.back()==BattleRewardEvent::ReturnStarted,"Return event follows synchronous cleanup");
 if(should_stop){
  check(count>=3&&reward_events[count-3]==BattleRewardEvent::CurrencyCommitted&&reward_events[count-2]==BattleRewardEvent::AreaMusicStopped,"Area stop occurs after currency and before transition return");
  const auto&request=world.audio_requests().back();check(request.kind==AudioRequestKind::StopMusicResource&&request.resource_index==owned_music&&request.duration==0,"Stop request immediately targets the original area resource");
 }else check(std::find(reward_events.begin(),reward_events.end(),BattleRewardEvent::AreaMusicStopped)==reward_events.end(),"No cleanup event when policy or inherited mode is absent");
 for(unsigned n=0;n<300&&outcome.phase()==BattleOutcomePhase::Returning;++n)frame();check(outcome.phase()==BattleOutcomePhase::PostWinRequested&&world.stage()==OpeningStage::BattleRequested,"Authentic post-win boundary stays paused for source script");check(!world.story_flag("doll_defeated")&&!world.story_flag("doll_melody")&&world.instance_visible(world.battle_request().actor_index),"No fabricated post-win flags or actor removal");check(random.raw_draw_count()==before_growth,"Growth/rewards/return consume no RNG");
 const auto post_start_position=world.player().position;
 check(outcome.advance_post_win(),outcome.error());
 check(world.stage()==OpeningStage::ScriptRunning,"Real reward handoff starts the external source program");
 unsigned post_text_ticks=0,post_text_count=0;bool first_flags=false,second_flags=false;
 for(unsigned n=0;n<3000&&outcome.phase()==BattleOutcomePhase::PostWinRequested;++n){
  if(world.pending_dialogue_id()!=kRoomNoIndex){
   if(++post_text_ticks==1)++post_text_count;
   if(post_text_ticks>=180){check(world.finish_story_dialogue(),world.error());post_text_ticks=0;}
  }
  const bool post_ok=world.advance({})&&world.idle_frame(dt);
  if(!post_ok&&!world.action_trace().empty()){const auto a=world.action_trace().back().action;std::cerr<<"postwin failed phrase="<<a.phrase<<" action="<<dialogue_action_name(a.kind)<<" actor="<<a.actor<<" value="<<a.value<<" duration="<<a.duration<<"\n";}
  check(post_ok,world.error());
  first_flags|=world.story_flag("doll_defeated")&&!world.story_flag("poltergeist");
  second_flags|=world.story_flag("pillow_attack");
  check(outcome.advance_post_win(),outcome.error());
 }
 check(outcome.phase()==BattleOutcomePhase::Complete&&world.stage()==OpeningStage::Walking,"Post-win eight phrases return player control");
 check(first_flags&&second_flags&&post_text_count==2,"Original post-win flags and two text phrases execute");
 check(!world.story_flag("doll_melody")&&!world.story_flag("minnie_leave"),"No premature melody or Pillow victory flags");
 check(world.player().position.x==post_start_position.x&&world.player().position.y==post_start_position.y,"Post-win preserves player location");
 check(outcome.state().experience==11&&outcome.state().bank==15&&outcome.state().hp==stopped_hp+3,"Story continuation preserves committed rewards");
 check(stop_count()==int(should_stop),"Return and post-win cannot repeat area music removal");
 // Reuse the inherited encounter contract as a state probe after the source
 // post-win script explicitly cleared the global mode; this is not a new route.
 check(world.begin_house_program(1),world.error());text_frames=0;
 for(unsigned n=0;n<3000&&world.stage()!=OpeningStage::BattleRequested;++n){if(world.pending_dialogue_id()!=kRoomNoIndex){if(++text_frames>=180){check(world.finish_story_dialogue(),world.error());text_frames=0;}}check(world.advance({})&&world.idle_frame(dt),world.error());}
 check(world.stage()==OpeningStage::BattleRequested&&!world.battle_request().overworld_music,"Post-win explicit false persists into the next inherited battle request");
 }
 std::cout<<checks<<" Doll round checks: real actions, retained session, boss callback, acknowledged deterministic growth, post-win eight-phrase continuation\n";
}
