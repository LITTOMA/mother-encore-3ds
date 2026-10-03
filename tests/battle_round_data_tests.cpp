#include "encore/battle_round_data.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstdio>
#include <vector>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t get(const std::vector<uint8_t>&b,size_t o){return uint32_t(b[o])|(uint32_t(b[o+1])<<8)|(uint32_t(b[o+2])<<16)|(uint32_t(b[o+3])<<24);}
static void put(std::vector<uint8_t>&b,size_t o,uint32_t v){for(unsigned i=0;i<4;++i)b[o+i]=uint8_t(v>>(8*i));}
static void fix(std::vector<uint8_t>&b){put(b,16,0);uint32_t c=~0u;for(auto v:b){c^=v;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}put(b,16,~c);}
static size_t section(const std::vector<uint8_t>&b,RoundSection s){return get(b,64+(uint32_t(s)-1)*16+4);}
int main(int argc,char**argv){if(argc!=2)return 2;std::vector<uint8_t>b;std::string error;CHECK(encore::read_file(argv[1],b,2*1024*1024,error));BattleRoundData data;CHECK(data.load(b.data(),b.size(),error));auto v=data.view();auto binding=v.binding();auto basic=v.skill(binding.basic_skill);CHECK(basic.power==10&&basic.variance==5&&basic.crit_chance==5);CHECK(v.string(v.text(basic.name).text)=="Bash");CHECK(v.string(v.text(basic.dialog).text)=="Ninten attacks!");CHECK(v.enemy_choice(0).weight==2&&v.enemy_choice(1).weight==1);CHECK(v.skill(binding.guard_skill).traits==uint32_t(RoundTrait::Guard));CHECK(v.rule(RoundRule::EnemyChoiceDelay)==.3);CHECK(v.rule(RoundRule::HpFrameSeconds)==1.0/30.0);CHECK(v.rule(RoundRule::PercentScale)==100);CHECK(v.rule(RoundRule::TextSecondsPerChar)==.02);CHECK(v.rule(RoundRule::TextSlowerMultiplier)==.7);
 CHECK(v.encounter().stop_area_music_if_overworld==0);auto victory=v.victory();CHECK(victory.initial_exp==0&&victory.initial_level==1&&victory.initial_bank==0&&victory.initial_cash==0&&victory.initial_earned_cash==0);CHECK(victory.reward_exp==3&&victory.reward_cash==5&&victory.reward_item_count==0);CHECK(victory.enemy_body_id==14);CHECK(victory.level_cap==30&&victory.next_level_exp==9&&victory.max_exp==20925);CHECK(v.string(v.text(victory.exp_text).text)=="Ninten gained 3 exp.");CHECK(v.string(victory.earned_cash_flag)=="earned_cash");CHECK(victory.acknowledgment==uint32_t(RoundAcknowledgment::AcceptOrCancelAfterFinished));CHECK(victory.currency_policy==uint32_t(RoundCurrencyPolicy::BankAndEarnedCash));CHECK(v.rule(RoundRule::VictoryBannerSeconds)==3);auto camera_return=v.parameter(RoundParameter::ReturnCamera);CHECK(camera_return.x==float(.3)&&camera_return.y==0&&camera_return.z==0&&camera_return.w==0);CHECK(get(b,8)==2&&get(b,20)==14&&get(b,24)==2&&get(b,28)==2);CHECK(v.count(RoundSection::Victory)==1);
 for(size_t n=0;n<b.size();++n)CHECK(!data.load(b.data(),n,error));
 CHECK(data.view().skill(binding.basic_skill).power==10);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{8,1},{8,99},{20,13},{20,99},{24,1},{24,0},{28,1},{28,99},{52,1},{64+4,0},{64+8,100000},{64+16+12,79}}){auto bad=b;put(bad,edit.first,edit.second);fix(bad);CHECK(!data.load(bad.data(),bad.size(),error));}
 auto skill=section(b,RoundSection::Skills),choice=section(b,RoundSection::EnemyChoices),rule=section(b,RoundSection::Rules),bind=section(b,RoundSection::Bindings),text=section(b,RoundSection::Texts);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{skill,0},{skill+4,2},{skill+8,9999},{skill+20,999},{skill+24,99},{skill+28,99},{skill+32,99},{skill+36,2},{skill+40,0xffffffff},{skill+44,0xffffffff},{skill+52,101},{skill+56,1},{skill+60,1},{skill+64,101},{skill+68,9999},{skill+76,1},{choice,999},{choice+4,0},{rule,999},{rule+8,0x7ff80000},{bind+12,999},{bind+20,0},{bind+56,2},{text+4,99},{text+8,2}}){auto bad=b;put(bad,edit.first,edit.second);fix(bad);CHECK(!data.load(bad.data(),bad.size(),error));}
 if(v.count(RoundSection::Media)){
  auto media=section(b,RoundSection::Media),track=section(b,RoundSection::Tracks),key=section(b,RoundSection::Keys),resource=section(b,RoundSection::Resources),parameter=section(b,RoundSection::Parameters),event=section(b,RoundSection::Events),presentation=section(b,RoundSection::PresentationBindings);
  CHECK(v.presentation(RoundPresentationSlot::EnemyDefeat)<v.count(RoundSection::Media));
  auto timeline=v.media(v.presentation(RoundPresentationSlot::ReturnTimeline)),jump=v.media(v.presentation(RoundPresentationSlot::PartyJumpToWorld));CHECK(timeline.event_count==5&&jump.event_count==1);
  for(uint32_t i=0;i<5;++i){CHECK(v.event(timeline.first_event+i).kind==uint32_t(RoundEventKind::TurnPartyToWorld)+i);}
  CHECK(v.event(jump.first_event).kind==uint32_t(RoundEventKind::PartyReturnLanded));
  for(auto index:{timeline.first_event,jump.first_event}){auto bad=b;put(bad,event+index*12+8,3);fix(bad);CHECK(!data.load(bad.data(),bad.size(),error));}
  for(uint32_t i=0;i<v.count(RoundSection::Parameters);++i){auto kind=get(b,parameter+i*20);if(kind==uint32_t(RoundParameter::ReturnPartyFrames)||kind==uint32_t(RoundParameter::ReturnPartyTurn)||kind==uint32_t(RoundParameter::ReturnCamera)){auto bad=b;put(bad,parameter+i*20+16,0x3f800000);fix(bad);CHECK(!data.load(bad.data(),bad.size(),error));}}

  for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{resource,0},{resource+4,2},{resource+8,99},{resource+12,0},{resource+20,0},{media+8,99},{media+12,999},{media+20,99999},{media+20,0},{media+32,8},{media+36,0x7fc00000},{track,999},{track+4,99},{track+12,0},{track+16,99},{track+20,99},{track+24,99},{key,0x7fc00000},{key+8,0x7fc00000},{parameter,999},{parameter+4,0x7fc00000},{event+8,99},{presentation,999},{presentation+4,999}}){auto bad=b;put(bad,edit.first,edit.second);fix(bad);CHECK(!data.load(bad.data(),bad.size(),error));}
 }
 auto reward=section(b,RoundSection::Victory);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{reward,6},{reward+4,0},{reward+4,30},{reward+8,0x7fffffff},{reward+12,0xffffffff},{reward+16,0x7fffffff},{reward+20,0},{reward+20,9},{reward+24,0xffffffff},{reward+28,1},{reward+32,0},{reward+32,1001},{reward+36,0},{reward+40,8},{reward+40,0xffffffff},{reward+44,0},{reward+44,99999},{reward+48,0},{reward+48,2},{reward+52,0},{reward+52,2},{reward+56,0},{reward+56,binding.win_flag},{reward+60,0},{64+13*16+8,0}}){auto bad=b;put(bad,edit.first,edit.second);fix(bad);CHECK(!data.load(bad.data(),bad.size(),error));}
 auto old_stride=b;old_stride[64+13*16+2]=56;fix(old_stride);CHECK(!data.load(old_stride.data(),old_stride.size(),error));CHECK(data.view().victory().reward_exp==3);
 auto copy=b;CHECK(data.load(copy.data(),copy.size(),error));std::fill(copy.begin(),copy.end(),0);CHECK(data.view().skill(binding.basic_skill).power==10);auto corrupt=b;corrupt.back()^=1;CHECK(!data.load(corrupt.data(),corrupt.size(),error));CHECK(data.view().rule(RoundRule::EnemyChoiceDelay)==.3);CHECK(!RoundView{}.valid());CHECK(RoundView{}.count(RoundSection::Skills)==0);CHECK(RoundView{}.presentation(RoundPresentationSlot::PartyIdle)==round_no_index);
 std::printf("BattleRoundData: %u checks; original bindings, f64 tuning, negative schema/opcode/span/ownership and rollback\n",checks);
}
