#include "encore/battle_outcome.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <utility>
using namespace encore::upstream;
namespace {
unsigned checks=0;
#define check(v,e) do {++checks;if(!(v)){std::cerr<<"check "<<checks<<" line "<<__LINE__<<": "<<(e)<<'\n';std::exit(1);}} while(false)
uint32_t u32(const std::vector<uint8_t>&v,size_t o){return uint32_t(v[o])|uint32_t(v[o+1])<<8|uint32_t(v[o+2])<<16|uint32_t(v[o+3])<<24;}
void put(std::vector<uint8_t>&v,size_t o,uint32_t n){for(unsigned i=0;i<4;++i)v[o+i]=uint8_t(n>>(8*i));}
size_t directory(RoundSection s){return 64+(uint32_t(s)-1)*16;}
size_t section(const std::vector<uint8_t>&v,RoundSection s){return u32(v,directory(s)+4);}
void fix(std::vector<uint8_t>&v){put(v,16,0);uint32_t crc=~0u;for(auto byte:v){crc^=byte;for(unsigned j=0;j<8;++j)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));}put(v,16,~crc);}
std::vector<uint8_t>read(const char*path){std::ifstream in(path,std::ios::binary);check(bool(in),"open test input");return {(std::istreambuf_iterator<char>(in)),{}};}
void schema(std::vector<uint8_t>&v,uint32_t version){for(auto o:{8,24,28})put(v,o,version);fix(v);}
void rejected(BattleRoundData&owner,std::vector<uint8_t>bytes,uint32_t id){std::string error;fix(bytes);check(!owner.load(bytes.data(),bytes.size(),error),"malformed v5 must fail closed");check(!error.empty()&&owner.view().binding().battle_id==id,"rejected load preserves checked owner");}
void loader_checks(const std::vector<uint8_t>&pillow,const std::vector<uint8_t>&doll){
 std::string error;BattleRoundData p,d;
 check(p.load(pillow.data(),pillow.size(),error),error.c_str());check(d.load(doll.data(),doll.size(),error),error.c_str());
 const auto pe=section(pillow,RoundSection::Encounter),pg=section(pillow,RoundSection::Growth),pv=section(pillow,RoundSection::Victory);
 const auto pm=section(pillow,RoundSection::Media),events=section(pillow,RoundSection::Events);
 check(p.view().version()==5&&d.view().version()==5,"both optional-route reward packs use v5");
 check(u32(pillow,20)==17&&(u32(pillow,directory(RoundSection::Encounter))>>16)==40,"v5 retains 17 sections and 40-byte encounter");
 check(!p.view().encounter().boss&&!p.view().encounter().keep_actor&&p.view().count(RoundSection::BossShakes)==0,"Pillow is removable nonboss");
 check(p.view().encounter().boss_flash_media==round_no_index&&p.view().victory().reward_exp==5&&p.view().victory().reward_cash==5,"source Pillow reward and no boss flash");
 check(p.view().string(p.view().encounter().post_win_script)=="Podunk/cutscenes/minnie_leave","source Minnie post-win script");
 check(p.view().victory().next_level_exp==9&&p.view().encounter().following_level_exp==27,"external bounded promotion thresholds");
 for(auto change:std::vector<std::pair<size_t,uint32_t>>{
  {8,6},{20,16},{24,4},{28,4},{directory(RoundSection::Encounter),15u|(36u<<16)},
  {pe,1},{pe,2},{pe+4,2},{pe+8,0},{pe+12,0},{pe+16,1},{pe+20,9},{pe+24,0},{pe+28,0},{pe+32,0},{pe+36,2},
  {pg,8},{pg+16,u32(pillow,pg)},{pg+4,0x80000000u},{pg+8,0},{pg+8,0x80000000u},{pg+12,0},
  {pv,9},{pv+20,0},{pv+20,27},{pv+24,0x80000000u},{pv+28,1},{pm+8,uint32_t(RoundMediaRole::BossFlash)},
  {events+8,uint32_t(RoundEventKind::BossFlashStart)},{events+8,uint32_t(RoundEventKind::BossKillEnemies)}}){
  auto bad=pillow;put(bad,change.first,change.second);rejected(p,std::move(bad),3);
 }
 auto future=pillow;schema(future,6);rejected(p,std::move(future),3);
 auto wrong_version=pillow;schema(wrong_version,4);rejected(p,std::move(wrong_version),3);
 check(!p.load(pillow.data(),335,error),"truncated v5 directory rejected");check(p.view().version()==5,"truncation preserves v5 owner");
 // A real shake record appended to the last section must still be rejected for a nonboss.
 auto nonboss_shake=pillow;const auto ds=section(doll,RoundSection::BossShakes),sd=directory(RoundSection::BossShakes);
 put(nonboss_shake,sd+4,uint32_t(nonboss_shake.size()));put(nonboss_shake,sd+8,1);put(nonboss_shake,sd+12,20);
 nonboss_shake.insert(nonboss_shake.end(),doll.begin()+ds,doll.begin()+ds+20);put(nonboss_shake,12,uint32_t(nonboss_shake.size()));rejected(p,std::move(nonboss_shake),3);
 const auto de=section(doll,RoundSection::Encounter),dm=section(doll,RoundSection::Media);
 const auto flash=d.view().encounter().boss_flash_media,flash_event=d.view().media(flash).first_event;
 const auto defeat=d.view().presentation(RoundPresentationSlot::EnemyDefeat),defeat_event=d.view().media(defeat).first_event;
 for(auto change:std::vector<std::pair<size_t,uint32_t>>{{de,2},{de+4,2},{de+12,round_no_index},{ds+4,0},
  {dm+flash*80+8,uint32_t(RoundMediaRole::HitEffect)},
  {section(doll,RoundSection::Events)+flash_event*12+8,uint32_t(RoundEventKind::Finished)},
  {section(doll,RoundSection::Events)+defeat_event*12+8,uint32_t(RoundEventKind::Finished)}}){
  auto bad=doll;put(bad,change.first,change.second);rejected(d,std::move(bad),2);
 }
 auto missing_shake=doll;missing_shake.resize(missing_shake.size()-20);put(missing_shake,12,uint32_t(missing_shake.size()));put(missing_shake,sd+8,2);put(missing_shake,sd+12,40);rejected(d,std::move(missing_shake),2);
 auto kept_nonboss=pillow;put(kept_nonboss,pe+4,1);fix(kept_nonboss);BattleRoundData boolean_policy;check(boolean_policy.load(kept_nonboss.data(),kept_nonboss.size(),error),"v5 permits explicit nonboss actor retention");
 auto removed_boss=doll;put(removed_boss,de+4,0);fix(removed_boss);check(boolean_policy.load(removed_boss.data(),removed_boss.size(),error),"v5 boss presentation does not imply actor retention");
 // v4 remains an exact-baseline contract; v3 keeps its explicitly shorter encounter.
 auto v4=doll;schema(v4,4);BattleRoundData legacy;check(legacy.load(v4.data(),v4.size(),error),error.c_str());check(legacy.view().version()==4,"v4 remains supported");
 auto bad_v4=v4;put(bad_v4,section(v4,RoundSection::Victory)+20,1);rejected(legacy,std::move(bad_v4),2);
 auto v3=v4;v3.erase(v3.begin()+de+36,v3.begin()+de+40);put(v3,12,uint32_t(v3.size()));put(v3,directory(RoundSection::Encounter),15u|(36u<<16));put(v3,directory(RoundSection::Encounter)+12,36);
 for(auto s:{RoundSection::Growth,RoundSection::BossShakes})put(v3,directory(s)+4,u32(v3,directory(s)+4)-4);
 schema(v3,3);check(legacy.load(v3.data(),v3.size(),error),error.c_str());check(legacy.view().encounter().stop_area_music_if_overworld==0,"v3 still has no return music policy field");
}
BattleSessionStats session_for(BattleView e,uint32_t exp=3,uint32_t level=1){
 const auto p=e.participant(0);BattleSessionStats s;s.experience=exp;s.level=level;s.bank=s.earned_cash=5;s.cash=7;
 s.hp=51;s.pp=p.pp-2;s.maxhp=p.maxhp;s.maxpp=p.maxpp;s.offense=p.offense;s.defense=p.defense;s.speed=p.speed;s.iq=p.iq;s.guts=p.guts;s.learned_skills={"strike","splitShot"};return s;
}
void set_stat(BattleSessionStats&s,uint32_t stat,int32_t value){switch(RoundStat(stat)){case RoundStat::MaxHp:s.maxhp=value;break;case RoundStat::MaxPp:s.maxpp=value;break;case RoundStat::Offense:s.offense=value;break;case RoundStat::Defense:s.defense=value;break;case RoundStat::Speed:s.speed=value;break;case RoundStat::Iq:s.iq=value;break;case RoundStat::Guts:s.guts=value;break;}}
int32_t stat(const BattleSessionStats&s,uint32_t id){switch(RoundStat(id)){case RoundStat::MaxHp:return s.maxhp;case RoundStat::MaxPp:return s.maxpp;case RoundStat::Offense:return s.offense;case RoundStat::Defense:return s.defense;case RoundStat::Speed:return s.speed;case RoundStat::Iq:return s.iq;case RoundStat::Guts:return s.guts;}return -1;}
void bases(BattleActionPresentation&p,RoundView r,BattleView e){
 for(uint32_t i=0;i<e.count(BattleSection::Layouts);++i){const auto l=e.layout(i);BattlePose q{l.rect,l.color,l.frame,true};if(l.flags&2){q.rect.x-=q.rect.z/2;q.rect.y-=q.rect.w/2;}
  if(l.role==uint32_t(BattleRole::PartySprite)){q.rect.y-=r.parameter(RoundParameter::PartyShown).x;check(p.set_actor_base(0,q),p.error());}
  if(l.role==uint32_t(BattleRole::EnemySprite))check(p.set_actor_base(1,q),p.error());
  if(l.role==uint32_t(BattleRole::PartyPlate)&&l.kind==uint32_t(BattleDrawKind::NinePatch))check(p.set_plate_base(q.rect),p.error());
 }
}
// Reward boundary harness: combat and presentation are real. The existing Lamp
// world request supplies a legal return boundary, without claiming Pillow story
// routing or overworld actor removal (covered by the separate world integration).
struct PresentationHost final:BattleRoundHost {
 BattleActionPresentation&p;
 explicit PresentationHost(BattleActionPresentation&value):p(value){}
 bool emit(const BattleRoundCue&cue,SourceRandom&r)override{return p.emit(cue,r);}
 bool ready(BattleRoundGate gate,uint32_t actor)const override{return p.ready(gate,actor);}
 int32_t current_hp(uint32_t actor)const override{return p.current_hp(actor);}
};
BattleSessionStats execute(RoundView r,BattleView e,RoomView room,const BattleSessionStats&s,bool promote,bool bad_growth=false){
 const double dt=double(float(1.0/60));SourceRandom random(0);OpeningWorld world;check(world.initialize(room),world.error());world.attach_random(random);
 for(unsigned n=0;n<1200&&world.stage()!=OpeningStage::BattleRequested;++n)check(world.advance({-1,0})&&world.idle_frame(dt),world.error());
 check(world.accept_battle_entry()&&world.idle_frame(dt),"reward harness accepts existing world battle boundary");
 BattleActionPresentation p;check(p.begin(r,e,random,&s),p.error());bases(p,r,e);PresentationHost host(p);BattleRound round;BattleOutcome outcome;
 check(outcome.initialize(r,room,&s),outcome.error());check(round.begin(r,e,random,host,&s),round.error());
 check(round.battler(0).target_hp==s.hp&&round.battler(0).target_pp==s.pp,"carried HP and PP are not reset at battle entry");
 unsigned turns=0;
 while(round.phase()!=BattleRoundPhase::VictoryPending&&turns++<20){
  check(round.phase()==BattleRoundPhase::Commands,"conscious session reaches next command");check(round.request_menu(r.binding().basic_menu)&&round.target_input(0,true),round.error());
  for(unsigned n=0;n<4000&&round.phase()==BattleRoundPhase::Running;++n){const double delta=double(float(dt*p.advance_real_time(dt)));check(p.physics_frame(delta)&&p.idle_frame(delta),p.error());check(round.idle_frame(delta),round.error());}
 }
 check(round.phase()==BattleRoundPhase::VictoryPending,"real encounter reaches victory");check(outcome.begin(round,p,world),outcome.error());
 auto frame=[&]{check(p.physics_frame(dt)&&p.idle_frame(dt),p.error());check(outcome.idle_frame(),outcome.error());};
 for(unsigned n=0;n<1000&&outcome.phase()!=BattleOutcomePhase::ExperienceDialogue;++n)frame();
 check(outcome.phase()==BattleOutcomePhase::ExperienceDialogue,"real victory opens EXP acknowledgement");
 const auto hp=outcome.state().hp,pp=outcome.state().pp;const auto draws=random.raw_draw_count();
 for(unsigned n=0;n<300;++n)frame();
 check(outcome.state().experience==s.experience&&outcome.state().bank==s.bank,"reward waits for acknowledgement");
 p.input(false,true);
 if(bad_growth){
  check(!outcome.idle_frame()&&outcome.phase()==BattleOutcomePhase::Error,"unsupported growth state fails before reward commit");
  check(outcome.state().experience==s.experience&&outcome.state().bank==s.bank,"failed growth preflight commits no EXP or cash");return outcome.state();
 }
 check(outcome.idle_frame(),outcome.error());
 check((outcome.phase()==BattleOutcomePhase::LevelDialogue)==promote,"only actual threshold crossing opens growth dialogue");
 unsigned prompts=0;while(outcome.phase()==BattleOutcomePhase::LevelDialogue&&prompts++<10){for(unsigned n=0;n<300;++n)frame();p.input(true,false);check(outcome.idle_frame(),outcome.error());}
 check(outcome.phase()==BattleOutcomePhase::Returning,"all required prompts reach return boundary");const auto result=outcome.state();
 check(result.experience==s.experience+r.victory().reward_exp&&result.level==(promote?r.encounter().promoted_level:s.level),"live EXP sum and conditional level");
 check(result.bank==s.bank+r.victory().reward_cash&&result.earned_cash==s.earned_cash+r.victory().reward_cash&&result.cash==s.cash,"bank and earned cash reward preserves wallet");
 int32_t hp_gain=0,pp_gain=0;for(uint32_t i=0;i<r.count(RoundSection::Growth);++i){const auto g=r.growth(i);check(stat(result,g.stat)==(promote?(r.version()==5?stat(s,g.stat)+int32_t(g.after-g.before):int32_t(g.after)):stat(s,g.stat)),"stats change only on promotion");if(promote&&g.stat==uint32_t(RoundStat::MaxHp))hp_gain=int32_t(g.after-g.before);if(promote&&g.stat==uint32_t(RoundStat::MaxPp))pp_gain=int32_t(g.after-g.before);}
 check(result.hp==hp+hp_gain&&result.pp==pp+pp_gain,"growth adds only maximum gains without full healing");
 const auto&events=outcome.events();check(std::count(events.begin(),events.end(),BattleRewardEvent::LevelCommitted)==int(promote),"no repeated level event");
 if(!promote){check(prompts==0&&result.learned_skills==s.learned_skills,"nonpromotion preserves skill list");check(std::count(events.begin(),events.end(),BattleRewardEvent::SkillLearned)==0,"nonpromotion does not relearn skill");}
 else{check(result.learned_skills.size()==s.learned_skills.size()+1&&result.learned_skills.back()==r.string(r.encounter().learned_skill),"promotion learns source field skill once");}
 check(random.raw_draw_count()==draws,"reward and growth do not consume combat RNG");return result;
}
}
int main(int argc,char**argv){
 check(argc==6,"Pillow round, Pillow entry, Doll round, Doll entry and room paths required");
 const auto pillow=read(argv[1]),doll=read(argv[3]);loader_checks(pillow,doll);
 std::string error;BattleRoundData pd,dd;BattleData pe,de;RoomData rd;
 check(pd.load_file(argv[1],error),error.c_str());check(pe.load_file(argv[2],error),error.c_str());check(dd.load_file(argv[3],error),error.c_str());check(de.load_file(argv[4],error),error.c_str());check(rd.load_file(argv[5],error),error.c_str());
 const auto p=pd.view(),d=dd.view();const auto room=rd.view();const auto original=session_for(pe.view());
 // Loader validates bounded shape; RoomView validates the actual source table.
 for(auto change:std::vector<std::pair<size_t,uint32_t>>{{section(pillow,RoundSection::Victory)+36,10},{section(pillow,RoundSection::Encounter)+20,28},{section(pillow,RoundSection::Victory)+32,p.victory().level_cap-1}}){
  auto bytes=pillow;put(bytes,change.first,change.second);fix(bytes);BattleRoundData wrong_threshold;check(wrong_threshold.load(bytes.data(),bytes.size(),error),error.c_str());BattleOutcome outcome;check(!outcome.initialize(wrong_threshold.view(),room,&original),"room rejects altered progression threshold/cap");
 }
 for(auto encounter:{p,d}){
  BattleOutcome outcome;check(outcome.initialize(encounter,room),outcome.error());
  auto reject=[&](const BattleSessionStats&s){check(!outcome.initialize(encounter,room,&s),"invalid session reward input rejected");};
  auto s=original;s.level=0;reject(s);s=original;s.level=3;reject(s);s=original;s.experience=9;reject(s);s=original;s.level=2;s.experience=8;reject(s);
  s=original;s.level=2;s.experience=27;reject(s);s=original;s.level=2;s.experience=27-encounter.victory().reward_exp;reject(s);s=original;s.experience=UINT32_MAX;reject(s);
  s=original;s.bank=INT32_MAX;reject(s);s=original;s.earned_cash=INT32_MAX;reject(s);s=original;s.cash=0x80000000u;reject(s);s=original;s.hp=0;reject(s);s=original;s.pp=s.maxpp+1;reject(s);s=original;s.offense=-1;reject(s);
  s=original;s.experience=0;check(outcome.initialize(encounter,room,&s),"level1 lower endpoint supported");s.experience=8;check(outcome.initialize(encounter,room,&s),"level1 upper endpoint supported");s.level=2;s.experience=9;check(outcome.initialize(encounter,room,&s),"level2 lower endpoint supported");s.experience=26-encounter.victory().reward_exp;check(outcome.initialize(encounter,room,&s),"sum just below following threshold supported");
 }
 auto no_cross=original;no_cross.offense+=1;execute(p,pe.view(),room,no_cross,false);
 auto early=original;early.experience=0;execute(p,pe.view(),room,early,false);
 const auto pillow_result=execute(p,pe.view(),room,original,false);check(pillow_result.experience==8,"Lamp 3 plus Pillow 5 stays level1");
 const auto doll_result=execute(d,de.view(),room,pillow_result,true);check(doll_result.experience==16&&doll_result.level==2,"Lamp to Pillow to Doll commits 3 to 8 to 16");
 auto crossing=original;crossing.experience=4;execute(p,pe.view(),room,crossing,true);
 auto promoted=original;promoted.level=2;promoted.experience=11;for(uint32_t i=0;i<p.count(RoundSection::Growth);++i){const auto g=p.growth(i);set_stat(promoted,g.stat,int32_t(g.after));}promoted.offense+=1;promoted.learned_skills.push_back(std::string(p.string(p.encounter().learned_skill)));execute(p,pe.view(),room,promoted,false);execute(d,de.view(),room,promoted,false);
 auto equipped=crossing;equipped.maxhp=70;equipped.hp=51;equipped.maxpp=30;equipped.pp=17;equipped.offense=14;equipped.defense=7;
 const auto offset_result=execute(p,pe.view(),room,equipped,true);check(offset_result.maxhp==73&&offset_result.maxpp==31&&offset_result.offense==15&&offset_result.defense==7,"v5 promotion preserves live effective-stat offsets");
 for(auto field:{RoundStat::MaxHp,RoundStat::MaxPp,RoundStat::Iq}){auto overflow=crossing;set_stat(overflow,uint32_t(field),INT32_MAX);execute(p,pe.view(),room,overflow,true,true);}
 auto v4=doll;schema(v4,4);BattleRoundData legacy;check(legacy.load(v4.data(),v4.size(),error),error.c_str());BattleOutcome legacy_outcome;check(legacy_outcome.initialize(legacy.view(),room,&original),"v4 accepts exact baseline");check(!legacy_outcome.initialize(legacy.view(),room,&pillow_result),"v4 still rejects changed baseline EXP");auto old_mismatch=original;old_mismatch.offense+=1;execute(legacy.view(),de.view(),room,old_mismatch,true,true);
 std::cout<<checks<<" Pillow/Doll schema5 checks: strict parser, legacy schemas, carried session, conditional promotion, bounded rewards\n";
}
