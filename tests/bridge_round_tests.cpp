#include "encore/battle_action_presentation.hpp"
#include "encore/battle_loot.hpp"
#include "encore/battle_outcome.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Bridge check %u: %s\n",checks,why);std::exit(1);}}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
void put(std::vector<uint8_t>&b,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(n>>(i*8));}
void scalar(std::vector<uint8_t>&b,size_t p,float n){uint32_t bits;std::memcpy(&bits,&n,4);put(b,p,bits);}
void sign(std::vector<uint8_t>&b){put(b,16,0);put(b,16,encore::crc32(b.data(),b.size()));}
size_t section(const std::vector<uint8_t>&b,unsigned k){return get(b,64+(k-1)*16+4);}
// Deliberate interface fixture: UID entropy itself is covered by the independent
// source Item.new allocator tests. This host consumes one controlled draw.
struct LootHost final:BattleLootHost {
 bool space=true,sound=true,reserve_ok=true,grant_ok=true;unsigned reserves=0,grants=0,sounds=0;
 bool validate_sound(std::string_view,std::string&e)const override{e=sound?"":"missing sound";return sound;}
 bool can_receive(std::string_view,uint32_t,std::string&e)const override{e=space?"":"full inventory";return space;}
 bool reserve(std::string_view,uint32_t,SourceRandom&r,uint32_t&uid,std::string&e)override{++reserves;uid=r.randi();e=reserve_ok?"":"UID allocation rejected";return reserve_ok;}
 bool grant(std::string_view,uint32_t,uint32_t,std::string&e)override{++grants;e=grant_ok?"":"grant rejected";return grant_ok;}
 bool play_sound(std::string_view,std::string&e)override{++sounds;e.clear();return true;}
};
}
int main(int argc,char**argv){
 check(argc==5,"bridge round, entry, original room and native pool oracle required");std::string error;std::vector<uint8_t>bytes;
 check(encore::read_file(argv[1],bytes,2*1024*1024,error),error.c_str());BattleRoundData data;
 check(data.load(bytes.data(),bytes.size(),error),error.c_str());auto view=data.view();const auto binding=view.binding();
 check(view.version()==6&&binding.battle_id==4,"explicit original bridge namespace");
 check(view.victory().reward_exp==8&&view.victory().reward_cash==20&&view.victory().reward_item_count==1,"source dog reward pool");
 check(view.string(binding.win_flag)=="beat_dog_bridge","source progression flag");
 check(view.skill(1).power==5&&view.skill(1).variance==10&&view.skill(1).miss_chance==5,"original bite mechanics");
 check(view.skill(2).power==5&&view.skill(2).variance==2&&view.skill(2).miss_chance==0,"original bash mechanics");
 check(view.enemy_choice(0).skill==1&&view.enemy_choice(0).weight==2&&view.enemy_choice(1).skill==2&&view.enemy_choice(1).weight==1,"original weighted AI");
 const auto loot=view.loot();check(view.string(loot.item)=="FleaBag"&&loot.chance==40&&loot.doses==1&&!loot.flags,"one ordinary source drop");
 const auto effect=view.pre_hit(1);check(effect.media!=round_no_index&&effect.apply_time==.25f&&view.media(effect.media).duration==.55f,"source pre-hit lifetime and method key");
 check(view.pre_hit(2).media==round_no_index&&view.pre_hit(100).media==round_no_index,"ordinary/unknown skills never borrow a pre-hit binding");
 const auto image=view.resource(view.media(effect.media).resource);check(image.width==1001&&image.height==85&&image.columns==11&&image.rows==1,"actual source frame grid, independent of Control height94");
 auto reject=[&](std::vector<uint8_t>b,const char*why){sign(b);check(!data.load(b.data(),b.size(),error),why);check(data.view().bytes_identity()==view.bytes_identity(),"failed parser admission retains current owner");};
 for(size_t n:{size_t(0),size_t(63),bytes.size()-1})check(!data.load(bytes.data(),n,error),"truncated v6 rejected");
 {auto b=bytes;put(b,8,7);put(b,24,7);put(b,28,7);reject(b,"unknown round schema rejected");}
 {auto b=bytes;put(b,8,5);put(b,24,5);put(b,28,5);reject(b,"loot/pre-hit cannot downgrade to v5");}
 const auto lp=section(bytes,18),pp=section(bytes,19);
 const auto actions=section(bytes,20);check(view.count(RoundSection::ActionMedia)==5,"every admitted skill has checked media policy");
 check(view.string(view.skill(4).source)=="Data/BattleSkills/attack_bat.yaml"&&view.action_media(4).use_sound==1,"source weapon-specific action and swing");
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{actions,99},{actions+12,0},{actions+4,UINT32_MAX-1},{actions+8,2}}){auto b=bytes;put(b,edit.first,edit.second);reject(b,"invalid or duplicate skill presentation rejected");}
 for(float value:{-1.f,0.f,101.f,INFINITY,NAN}){auto b=bytes;scalar(b,lp+4,value);reject(b,"invalid source loot chance rejected");}
 for(const auto value:{0u,2u,UINT32_MAX}){auto b=bytes;put(b,lp+8,value);reject(b,"unsupported source doses rejected");}
 {auto b=bytes;put(b,lp+28,1);reject(b,"unknown loot policy rejected");}
 {auto b=bytes;put(b,lp,0);reject(b,"missing item identity rejected");}
 {auto b=bytes;put(b,lp+12,0);reject(b,"missing present text rejected");}
 {auto b=bytes;put(b,pp,32);reject(b,"unknown pre-hit skill rejected");}
 {auto b=bytes;put(b,pp+4,0);reject(b,"wrong pre-hit media role rejected");}
 for(float time:{-1.f,0.f,1.f,INFINITY,NAN}){auto b=bytes;scalar(b,pp+8,time);reject(b,"invalid pre-hit time rejected");}
 {auto b=bytes;const auto row=std::vector<uint8_t>(b.begin()+pp,b.begin()+pp+12);b.insert(b.end(),row.begin(),row.end());put(b,12,uint32_t(b.size()));put(b,64+18*16+8,2);put(b,64+18*16+12,24);reject(b,"duplicate pre-hit owner rejected");}
 auto* reference=std::fopen(argv[4],"rb");check(reference!=nullptr,"native source pool oracle exists");char header[128];check(std::fgets(header,sizeof(header),reference)!=nullptr,"native source pool header");
 unsigned selected=0,empty=0;
 for(uint64_t seed=1;seed<=256;++seed){SourceRandom actual(seed),expected(seed);static_cast<void>(expected.randi());const bool chosen=expected.rand_range(0,100)<=loot.chance;const auto uid=chosen?expected.randi():0;
  LootHost host;BattleLoot pool;check(pool.prepare(view,actual,host,error),error.c_str());check(pool.ready()&&pool.matches(view)&&pool.selected()==chosen&&pool.uid()==uid,"source candidate/percentage/UID interface order");
  check(actual.state()==expected.state()&&actual.raw_draw_count()==expected.raw_draw_count(),"no dropped or additional random calls");
  unsigned long long source_seed=0;unsigned source_selected=0,source_uid=0,tail[3]{};
  check(std::fscanf(reference,"%llu,%u,%u,%u,%u,%u\n",&source_seed,&source_selected,&source_uid,&tail[0],&tail[1],&tail[2])==6&&source_seed==seed,"ordered native source seed record");
  check(source_selected==unsigned(chosen)&&source_uid==uid,"unchanged original BattleItemPool candidate/percentage and controlled UID interface agree");
  for(auto value:tail)check(actual.randi()==value,"native original pool continuation random draws agree");
  check(host.reserves==(chosen?1u:0u)&&host.grants==0,"Item.new reservation precedes reward/inventory grant");
  const auto before=actual.state();check(!pool.prepare(view,actual,host,error)&&before==actual.state(),"duplicate prepare cannot reroll loot");
  check(pool.grant(error)&&pool.granted()&&host.grants==(chosen?1u:0u),"only selected items enter inventory");
  check(!pool.grant(error),"reward grant is single use");
  if(chosen){++selected;check(pool.play_sound(error)&&host.sounds==1&&!pool.play_sound(error),"taking prompt sound is single use");}
  else{++empty;check(!pool.play_sound(error)&&host.sounds==0,"empty pool produces no acquisition sound");}
 }
 check(std::fgetc(reference)==EOF&&std::fclose(reference)==0,"native source oracle consumed exactly");
 check(selected&&empty,"controlled seeds exercise both source probability outcomes");
 for(unsigned failure=0;failure<3;++failure){SourceRandom random(1);const auto before=random.state(),draws=random.raw_draw_count();LootHost host;BattleLoot pool;
  host.space=failure!=0;host.sound=failure!=1;host.reserve_ok=failure!=2;
  if(failure==2){while(true){auto probe=random;static_cast<void>(probe.randi());if(probe.rand_range(0,100)<=loot.chance)break;random.seed(random.state());}}
  const auto retained=random.state(),retained_draws=random.raw_draw_count();check(!pool.prepare(view,random,host,error)&&!pool.ready(),"failed preflight/reservation does not publish a pool");
  check(random.state()==retained&&random.raw_draw_count()==retained_draws,"failed admission preserves shared random stream");
  if(failure<2)check(retained==before&&retained_draws==draws&&host.reserves==0,"unsupported full-inventory/audio branch stops before RNG");
 }
 BattleData entry;RoomData room;check(entry.load_file(argv[2],error),error.c_str());check(room.load_file(argv[3],error),error.c_str());
 check(entry.view().participant(1).hp==84&&entry.view().participant(1).offense==13&&entry.view().participant(1).defense==20,"original dog entry statistics");
 check(entry.view().background(0).effect_scale==0&&!(entry.view().background(0).flags&1u),"original disabled barrel branch retains its unused zero scale");
 {std::vector<uint8_t>original;check(encore::read_file(argv[2],original,1024*1024,error),error.c_str());const auto background=section(original,10);
  auto b=original;put(b,background+4,get(b,background+4)|1u);sign(b);check(!entry.load(b.data(),b.size(),error),"zero scale is not admitted for an active barrel branch");
  b=original;scalar(b,background+24,-1);sign(b);check(!entry.load(b.data(),b.size(),error),"negative background scale stays unsupported");
  b=original;scalar(b,background+24,NAN);sign(b);check(!entry.load(b.data(),b.size(),error),"nonfinite unused coefficient stays rejected");
 }
 SourceRandom random(812);BattleActionPresentation presentation;check(presentation.begin(view,entry.view(),random),presentation.error());
 check(!presentation.ready(BattleRoundGate::PreHitDone,1),"pre-hit cannot finish before it starts");
 check(presentation.emit({BattleRoundCueKind::PreHit,1,0,1},random),presentation.error());const auto unchanged=random.state();
 for(unsigned tick=1;tick<=16;++tick){check(presentation.idle_frame(1.0/60),presentation.error());check(presentation.ready(BattleRoundGate::PreHitDone,1)==(tick==16),"native Godot deferred method signal occurs at tick16, after quarter-second key");}
 check(random.state()==unchanged,"pre-hit visual/timing adds no random calls");
 BattleOutcome outcome;check(!outcome.initialize(view,room.view()),"caller cannot lose an unbound original reward pool");
 LootHost host;BattleLoot pool;check(pool.prepare(view,random,host,error),error.c_str());check(outcome.initialize(view,room.view(),nullptr,&pool),outcome.error());
 BattleRoundData other;check(other.load(bytes.data(),bytes.size(),error),error.c_str());check(!pool.matches(other.view()),"resource ownership is exact, independent of equal battle IDs");
 std::printf("Bridge round: %u checks; v6 parser, original stats/AI, loot transaction and native deferred pre-hit timing; not a field/hardware run\n",checks);
}
