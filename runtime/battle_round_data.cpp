#include "encore/battle_round_data.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {
namespace {
constexpr uint32_t section_count=17,header_bytes=288,max_bytes=2*1024*1024;
constexpr uint32_t strides[]={1,80,8,12,64,20,60,80,28,24,12,8,20,64,40,16,20};
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
uint16_t u16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
int32_t i32(const uint8_t*p){auto b=u32(p);int32_t v;std::memcpy(&v,&b,4);return v;}
float f32(const uint8_t*p){auto b=u32(p);float v;std::memcpy(&v,&b,4);return v;}
double f64(const uint8_t*p){uint64_t b=uint64_t(u32(p))|(uint64_t(u32(p+4))<<32);double v;std::memcpy(&v,&b,8);return v;}
BattleValue value(const uint8_t*p){return {f32(p),f32(p+4),f32(p+8),f32(p+12)};}
bool finite(BattleValue v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.w);}
uint32_t crc(const uint8_t*p,size_t n,const encore::PreparationControl* control=nullptr){uint32_t c=~0u;for(size_t i=0;i<n;++i){if(!(i&4095)&&control&&control->stopped())return 0;c^=(i>=16&&i<20)?0:p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
bool utf8(const uint8_t*p,size_t n){for(size_t i=0;i<n;){uint32_t c=p[i++];if(c<0x80)continue;unsigned extra;uint32_t low;if(c>=0xc2&&c<=0xdf){extra=1;low=0x80;c&=31;}else if(c>=0xe0&&c<=0xef){extra=2;low=0x800;c&=15;}else if(c>=0xf0&&c<=0xf4){extra=3;low=0x10000;c&=7;}else return false;if(extra>n-i)return false;while(extra--){auto b=p[i++];if((b&0xc0)!=0x80)return false;c=(c<<6)|(b&63);}if(c<low||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return false;}return true;}
bool safe_path(std::string_view p){if(p.empty()||p.front()=='/'||p.find(':')!=p.npos||p.find('\\')!=p.npos)return false;size_t a=0;while(a<=p.size()){auto b=p.find('/',a);if(b==p.npos)b=p.size();auto s=p.substr(a,b-a);if(s.empty()||s=="."||s=="..")return false;if(b==p.size())break;a=b+1;}return true;}
}
uint32_t RoundView::version()const{return bytes_?u32(bytes_+8):0;}
uint32_t RoundView::count(RoundSection s)const{auto i=uint32_t(s);return bytes_&&i>=1&&i<=u32(bytes_+20)?u32(bytes_+64+(i-1)*16+8):0;}
const uint8_t*RoundView::record(RoundSection s,uint32_t i)const{auto k=uint32_t(s);if(!bytes_||k<1||k>u32(bytes_+20)||i>=count(s))return nullptr;auto*d=bytes_+64+(k-1)*16;return bytes_+u32(d+4)+size_t(i)*u16(d+2);}
std::string_view RoundView::string(uint32_t o)const{auto*p=record(RoundSection::Strings,o);if(!p)return {};auto*e=static_cast<const uint8_t*>(std::memchr(p,0,count(RoundSection::Strings)-o));return e?std::string_view(reinterpret_cast<const char*>(p),size_t(e-p)):std::string_view{};}
RoundSkill RoundView::skill(uint32_t i)const{RoundSkill r;auto*p=record(RoundSection::Skills,i);if(!p)return r;r.id=u32(p);r.source=u32(p+4);r.name=u32(p+8);r.description=u32(p+12);r.dialog=u32(p+16);r.action_type=u32(p+20);r.target_type=u32(p+24);r.skill_type=u32(p+28);r.damage_type=u32(p+32);r.traits=u32(p+36);r.power=i32(p+40);r.variance=i32(p+44);r.priority=i32(p+48);r.miss_chance=i32(p+52);r.pp_cost=i32(p+56);r.hp_cost=i32(p+60);r.crit_chance=i32(p+64);r.user_media=u32(p+68);r.hit_media=u32(p+72);r.fail_chance=i32(p+76);return r;}
RoundEnemyChoice RoundView::enemy_choice(uint32_t i)const{auto*p=record(RoundSection::EnemyChoices,i);return p?RoundEnemyChoice{u32(p),u32(p+4)}:RoundEnemyChoice{};}
double RoundView::rule(RoundRule key)const{for(uint32_t i=0;i<count(RoundSection::Rules);++i){auto*p=record(RoundSection::Rules,i);if(u32(p)==uint32_t(key))return f64(p+4);}return 0;}
BattleValue RoundView::parameter(RoundParameter key)const{for(uint32_t i=0;i<count(RoundSection::Parameters);++i){auto*p=record(RoundSection::Parameters,i);if(u32(p)==uint32_t(key))return value(p+4);}return {};}
uint32_t RoundView::presentation(RoundPresentationSlot key)const{for(uint32_t i=0;i<count(RoundSection::PresentationBindings);++i){auto*p=record(RoundSection::PresentationBindings,i);if(u32(p)==uint32_t(key))return u32(p+4);}return round_no_index;}
RoundBinding RoundView::binding()const{auto*p=record(RoundSection::Bindings,0);if(!p)return {};return {u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),u32(p+24),u32(p+28),u32(p+32),u32(p+36),u32(p+40),u32(p+44),u32(p+48),u32(p+52),u32(p+56),u32(p+60)};}
RoundText RoundView::text(uint32_t i)const{auto*p=record(RoundSection::Texts,i);return p?RoundText{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16)}:RoundText{};}
RoundVictory RoundView::victory()const{auto*p=record(RoundSection::Victory,0);return p?RoundVictory{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),u32(p+24),u32(p+28),u32(p+32),u32(p+36),u32(p+40),u32(p+44),u32(p+48),u32(p+52),u32(p+56),u32(p+60)}:RoundVictory{};}
RoundEncounter RoundView::encounter()const{
 auto*p=record(RoundSection::Encounter,0);if(!p)return {}; // Explicit v2 has no encounter section.
 RoundEncounter result{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),u32(p+24),u32(p+28),u32(p+32)};
 // Only checked v3/v4/v5 views reach this accessor; unknown schemas fail load.
 if(version()>=4)result.stop_area_music_if_overworld=u32(p+36);
 return result;
}
RoundGrowth RoundView::growth(uint32_t i)const{auto*p=record(RoundSection::Growth,i);return p?RoundGrowth{u32(p),u32(p+4),u32(p+8),u32(p+12)}:RoundGrowth{};}
RoundBossShake RoundView::boss_shake(uint32_t i)const{auto*p=record(RoundSection::BossShakes,i);return p?RoundBossShake{f32(p),f32(p+4),f32(p+8),f32(p+12),f32(p+16)}:RoundBossShake{};}
BattleResource RoundView::resource(uint32_t i)const{BattleResource r;auto*p=record(RoundSection::Resources,i);if(p){r={u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),u32(p+24),{}};std::memcpy(r.sha256,p+28,32);}return r;}
RoundMedia RoundView::media(uint32_t i)const{auto*p=record(RoundSection::Media,i);if(!p)return {};return {u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),u32(p+24),u32(p+28),u32(p+32),f32(p+36),value(p+40),value(p+56),{f32(p+72),f32(p+76)}};}
RoundTrack RoundView::track(uint32_t i)const{auto*p=record(RoundSection::Tracks,i);return p?RoundTrack{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),u32(p+24)}:RoundTrack{};}
BattleKey RoundView::key(uint32_t i)const{auto*p=record(RoundSection::Keys,i);return p?BattleKey{f32(p),f32(p+4),value(p+8)}:BattleKey{};}
RoundEvent RoundView::event(uint32_t i)const{auto*p=record(RoundSection::Events,i);return p?RoundEvent{u32(p),f32(p+4),u32(p+8)}:RoundEvent{};}
bool BattleRoundData::load(const uint8_t*input,size_t size,std::string&error,const encore::PreparationControl* control){
 auto fail=[&](const char*s){error=s;return false;};
 if(!input||size<header_bytes||size>max_bytes)return fail("Round pack size rejected");
 const auto version=u32(input+8),sections=u32(input+20);
 if(std::memcmp(input,"ENCRND01",8)||(version!=2&&version!=3&&version!=4&&version!=5)||u32(input+12)!=size||sections!=(version==2?14u:17u)||u32(input+24)!=version||u32(input+28)!=version||size<64+16*sections)return fail("Round schema/capability/rules rejected");
 for(unsigned i=52;i<64;++i)if(input[i])return fail("Round reserved header bytes set");
 const auto checksum=crc(input,size,control);if(control&&control->stopped())return fail("Checked pack preparation cancelled");
 if(checksum!=u32(input+16))return fail("Round CRC mismatch");
 size_t end=64+16*sections;
 for(uint32_t s=1;s<=sections;++s){auto*d=input+64+(s-1)*16;auto off=u32(d+4),n=u32(d+8),bytes=u32(d+12);const auto stride=version==3&&s==uint32_t(RoundSection::Encounter)?36u:strides[s-1];if(u16(d)!=s||u16(d+2)!=stride||uint64_t(n)*stride!=bytes)return fail("Round directory rejected");if(!n){if(off||bytes)return fail("Round empty section rejected");continue;}if(off%4||off<end||off>size||bytes>size-off)return fail("Round section span rejected");for(size_t j=end;j<off;++j)if(input[j])return fail("Round padding rejected");end=size_t(off)+bytes;}
 if(end!=size)return fail("Round trailing bytes rejected");
 RoundView v;v.bytes_=input;v.size_=size;
 auto count=[&](RoundSection s){return v.count(s);};
 if(!count(RoundSection::Strings)||count(RoundSection::Strings)>65536||!count(RoundSection::Skills)||count(RoundSection::Skills)>32||!count(RoundSection::EnemyChoices)||count(RoundSection::EnemyChoices)>32||count(RoundSection::Rules)!=uint32_t(RoundRule::Count)-1||count(RoundSection::Bindings)!=1||count(RoundSection::Victory)!=1||!count(RoundSection::Texts)||count(RoundSection::Texts)>256||count(RoundSection::Resources)>128||!count(RoundSection::Media)||count(RoundSection::Media)>256||count(RoundSection::Tracks)>2048||count(RoundSection::Keys)>8192||count(RoundSection::Events)>1024||count(RoundSection::PresentationBindings)>uint32_t(RoundPresentationSlot::Count)-1||count(RoundSection::Parameters)>uint32_t(RoundParameter::Count)-1)return fail("Round section capacity rejected");
 auto*pool=v.record(RoundSection::Strings,0);auto pool_size=count(RoundSection::Strings);if(pool[0]||pool[pool_size-1]||!utf8(pool,pool_size))return fail("Round strings rejected");
 auto str=[&](uint32_t o){return o<pool_size&&(o==0||pool[o-1]==0)&&v.string(o).size()<=4096;};
 for(auto section:{RoundSection::Skills,RoundSection::Texts,RoundSection::Resources,RoundSection::Media}){std::set<uint32_t>ids;for(uint32_t i=0;i<count(section);++i){auto id=u32(v.record(section,i));if(!id||!ids.insert(id).second)return fail("Round duplicate/zero ID");}}
 for(uint32_t i=0;i<count(RoundSection::Skills);++i){auto s=v.skill(i);if(!str(s.source)||!safe_path(v.string(s.source))||s.name>=count(RoundSection::Texts)||s.description>=count(RoundSection::Texts)||s.dialog>=count(RoundSection::Texts))return fail("Round skill text rejected");if((s.action_type!=0&&s.action_type!=4)||(s.target_type!=0&&s.target_type!=5)||s.skill_type>2||s.damage_type>1||(s.traits&~1u))return fail("Round skill opcode rejected");if(s.power<0||s.power>100000||s.variance<0||s.variance>10000||s.priority<-100||s.priority>100||s.miss_chance<0||s.miss_chance>100||s.pp_cost||s.hp_cost||s.fail_chance||s.crit_chance<0||s.crit_chance>100)return fail("Round skill numeric path rejected");if((s.user_media!=round_no_index&&s.user_media>=count(RoundSection::Media))||(s.hit_media!=round_no_index&&s.hit_media>=count(RoundSection::Media)))return fail("Round skill media rejected");if(!((s.action_type==0&&s.target_type==0&&(s.skill_type==1||s.skill_type==2)&&s.damage_type==1&&!s.traits)||(s.action_type==4&&s.target_type==5&&!s.skill_type&&!s.damage_type&&!s.power&&!s.variance)))return fail("Round unsupported skill path");if((s.traits&1)&&(s.dialog||s.action_type!=4))return fail("Round guard path rejected");}
 std::set<uint32_t>choices;for(uint32_t i=0;i<count(RoundSection::EnemyChoices);++i){auto c=v.enemy_choice(i);if(c.skill>=count(RoundSection::Skills)||!c.weight||c.weight>100000||!choices.insert(c.skill).second)return fail("Round enemy choice rejected");}
 std::set<uint32_t>rules;for(uint32_t i=0;i<count(RoundSection::Rules);++i){auto*p=v.record(RoundSection::Rules,i);auto k=u32(p);auto f=f64(p+4);if(!k||k>=uint32_t(RoundRule::Count)||!rules.insert(k).second||!std::isfinite(f)||f<=0||f>100000)return fail("Round rule rejected");}
 for(auto key:{RoundRule::PercentScale,RoundRule::HpTransitionFrames}){auto f=v.rule(key);if(std::floor(f)!=f)return fail("Round integer rule rejected");}
 auto b=v.binding();if(!b.battle_id||b.player_participant!=0||b.enemy_participant!=1||b.basic_skill>=count(RoundSection::Skills)||b.guard_skill>=count(RoundSection::Skills)||!b.basic_menu||!b.items_menu||!b.guard_menu||b.basic_menu==b.items_menu||b.basic_menu==b.guard_menu||b.items_menu==b.guard_menu||!str(b.locale)||v.string(b.locale).empty()||b.enemy_name>=count(RoundSection::Texts)||b.enemy_article>=count(RoundSection::Texts)||b.enemy_outro>=count(RoundSection::Texts)||b.mortal_damage>=count(RoundSection::Texts)||b.no_effect>=count(RoundSection::Texts)||b.show_intro_outro>1||!str(b.win_flag))return fail("Round binding rejected");if(v.skill(b.basic_skill).skill_type!=1||!(v.skill(b.guard_skill).traits&1))return fail("Round bound skill role rejected");
 for(uint32_t i=0;i<count(RoundSection::Texts);++i){auto t=v.text(i);if(t.role>uint32_t(version==2?RoundTextRole::Experience:RoundTextRole::Learning)||!str(t.key)||!str(t.source_text)||!str(t.text))return fail("Round text rejected");}auto empty=v.text(0);if(empty.role||empty.key||empty.source_text||empty.text)return fail("Round empty text rejected");
 auto reward=v.victory();
 if(!reward.enemy_body_id||!reward.initial_level||reward.initial_level>=reward.level_cap||reward.level_cap>1000||!reward.reward_exp||reward.reward_item_count||reward.next_level_exp>reward.max_exp||reward.max_exp>INT32_MAX||(version==2&&uint64_t(reward.initial_exp)+reward.reward_exp>=reward.next_level_exp)||(version==5&&reward.initial_exp>=reward.next_level_exp)||reward.initial_bank>INT32_MAX||reward.initial_cash>INT32_MAX||reward.initial_earned_cash>INT32_MAX||uint64_t(reward.initial_bank)+reward.reward_cash>INT32_MAX||uint64_t(reward.initial_earned_cash)+reward.reward_cash>INT32_MAX)return fail("Round unsupported victory progression/reward state");
 if(reward.exp_text>=count(RoundSection::Texts)||v.text(reward.exp_text).role!=uint32_t(RoundTextRole::Experience)||v.string(v.text(reward.exp_text).text).empty()||reward.acknowledgment!=uint32_t(RoundAcknowledgment::AcceptOrCancelAfterFinished)||reward.currency_policy!=uint32_t(RoundCurrencyPolicy::BankAndEarnedCash)||!str(reward.earned_cash_flag)||v.string(reward.earned_cash_flag).empty()||reward.earned_cash_flag==b.win_flag)return fail("Round victory binding/policy rejected");
 if(version>=3){
  if(count(RoundSection::Encounter)!=1||count(RoundSection::Growth)!=7||(version<5&&count(RoundSection::BossShakes)!=3))return fail("Round encounter capacity rejected");
  const auto e=v.encounter();
  if(e.stop_area_music_if_overworld>1)return fail("Round area music policy rejected");
  if((version<5?(e.boss!=1||e.keep_actor!=1):(e.boss>1||e.keep_actor>1))||!str(e.post_win_script)||!safe_path(v.string(e.post_win_script))||(e.boss&&e.boss_flash_media>=count(RoundSection::Media))||e.promoted_level!=reward.initial_level+1||e.promoted_level>=reward.level_cap||e.following_level_exp<=reward.next_level_exp||e.following_level_exp>reward.max_exp||(version<5&&uint64_t(reward.initial_exp)+reward.reward_exp<reward.next_level_exp)||uint64_t(reward.initial_exp)+reward.reward_exp>=e.following_level_exp||e.level_text>=count(RoundSection::Texts)||v.text(e.level_text).role!=uint32_t(RoundTextRole::LevelUp)||!str(e.learned_skill)||v.string(e.learned_skill).empty()||e.learned_text>=count(RoundSection::Texts)||v.text(e.learned_text).role!=uint32_t(RoundTextRole::Learning)||!v.string(b.win_flag).empty())return fail("Round encounter policy rejected");
  std::set<uint32_t>stats;for(uint32_t i=0;i<count(RoundSection::Growth);++i){auto g=v.growth(i);if(g.stat<1||g.stat>7||!stats.insert(g.stat).second||g.before>INT32_MAX||g.after<g.before||g.after>INT32_MAX||g.text>=count(RoundSection::Texts)||(g.after==g.before?g.text!=0:v.text(g.text).role!=uint32_t(RoundTextRole::StatGrowth)))return fail("Round growth rejected");}
  if(version==5){
   if(e.boss){if(count(RoundSection::BossShakes)!=3)return fail("Round boss shake capacity rejected");}
   else{
    if(count(RoundSection::BossShakes)||e.boss_flash_media!=round_no_index)return fail("Round nonboss encounter policy rejected");
    for(uint32_t i=0;i<count(RoundSection::Events);++i){const auto kind=v.event(i).kind;if(kind==uint32_t(RoundEventKind::BossFlashStart)||kind==uint32_t(RoundEventKind::BossKillEnemies))return fail("Round nonboss callback rejected");}
    for(uint32_t i=0;i<count(RoundSection::Media);++i)if(v.media(i).role==uint32_t(RoundMediaRole::BossFlash))return fail("Round nonboss flash media rejected");
   }
  }
  float prior=-1;for(uint32_t i=0;i<count(RoundSection::BossShakes);++i){auto q=v.boss_shake(i);if(!std::isfinite(q.time)||!std::isfinite(q.magnitude)||!std::isfinite(q.length)||!std::isfinite(q.interval)||!std::isfinite(q.weight)||q.time<=prior||q.time<0||q.magnitude<=1||q.magnitude>100||q.interval<=0||q.length<q.interval||q.length/q.interval>1000||q.weight<=0||q.weight>1)return fail("Round boss shake rejected");prior=q.time;}
  if(e.boss){
  for(uint32_t i=0;i<count(RoundSection::Events);++i){const auto event=v.event(i);if((event.kind==uint32_t(RoundEventKind::BossFlashStart)&&event.media!=v.presentation(RoundPresentationSlot::EnemyDefeat))||(event.kind==uint32_t(RoundEventKind::BossKillEnemies)&&event.media!=e.boss_flash_media))return fail("Round boss callback owner rejected");}
  const auto flash=v.media(e.boss_flash_media),defeat=v.media(v.presentation(RoundPresentationSlot::EnemyDefeat));
  if(flash.role!=uint32_t(RoundMediaRole::BossFlash)||flash.resource!=round_no_index||flash.event_count!=1||defeat.event_count!=1||v.event(flash.first_event).kind!=uint32_t(RoundEventKind::BossKillEnemies)||v.event(defeat.first_event).kind!=uint32_t(RoundEventKind::BossFlashStart)||v.event(defeat.first_event).time>=defeat.duration||prior>=v.event(defeat.first_event).time)return fail("Round boss callback contract rejected");
  }
 }
 for(uint32_t i=0;i<count(RoundSection::Resources);++i){auto r=v.resource(i);if(!str(r.path)||!safe_path(v.string(r.path))||r.kind<1||r.kind>3||!r.width||r.width>8192||!r.height||r.height>8192||!r.columns||r.columns>256||!r.rows||r.rows>256||(r.kind==1&&(r.width%r.columns||r.height%r.rows)))return fail("Round resource rejected");}
 std::set<uint32_t>owned_tracks,owned_events;
 for(uint32_t i=0;i<count(RoundSection::Media);++i){auto m=v.media(i);if(!str(m.name)||m.role<1||m.role>uint32_t(version==2?RoundMediaRole::WorldParty:RoundMediaRole::BossFlash)||(m.resource!=round_no_index&&m.resource>=count(RoundSection::Resources))||m.first_track>count(RoundSection::Tracks)||m.track_count>count(RoundSection::Tracks)-m.first_track||m.first_event>count(RoundSection::Events)||m.event_count>count(RoundSection::Events)-m.first_event||m.flags&~7u||!std::isfinite(m.duration)||m.duration<0||m.duration>120||(m.duration==0&&(m.track_count||m.event_count||(m.flags&1)))||!finite(m.rect)||!finite(m.color)||!std::isfinite(m.anchor.x)||!std::isfinite(m.anchor.y)||m.color.x<0||m.color.x>1||m.color.y<0||m.color.y>1||m.color.z<0||m.color.z>1||m.color.w<0||m.color.w>1||m.anchor.x<0||m.anchor.x>1||m.anchor.y<0||m.anchor.y>1)return fail("Round media rejected");for(uint32_t j=0;j<m.track_count;++j)if(v.track(m.first_track+j).media!=i||!owned_tracks.insert(m.first_track+j).second)return fail("Round media track ownership rejected");float prior=-1;for(uint32_t j=0;j<m.event_count;++j){auto e=v.event(m.first_event+j);if(e.media!=i||!owned_events.insert(m.first_event+j).second||e.time<prior)return fail("Round media event ownership/order rejected");prior=e.time;}}
 if(owned_tracks.size()!=count(RoundSection::Tracks)||owned_events.size()!=count(RoundSection::Events))return fail("Round orphan tracks/events rejected");
 std::set<uint32_t>used_keys;
 for(uint32_t i=0;i<count(RoundSection::Tracks);++i){auto t=v.track(i);if(t.media>=count(RoundSection::Media)||!t.property||t.property>(version==2?17u:18u)||!t.count||t.first>count(RoundSection::Keys)||t.count>count(RoundSection::Keys)-t.first||t.update>1||t.interpolation>uint32_t(RoundInterpolation::QuadIn)||t.mode>2)return fail("Round track opcode/span rejected");float prior=-1;for(uint32_t j=0;j<t.count;++j){if(!used_keys.insert(t.first+j).second)return fail("Round overlapping keys rejected");auto k=v.key(t.first+j);if(!std::isfinite(k.time)||k.time<0||k.time<=prior||k.time>v.media(t.media).duration||!std::isfinite(k.ease)||std::abs(k.ease)>100||!finite(k.value))return fail("Round key rejected");prior=k.time;if(t.property==6){auto m=v.media(t.media);if(m.resource>=count(RoundSection::Resources))return fail("Round frame resource rejected");auto r=v.resource(m.resource);if(k.value.x<0||k.value.x>=float(uint64_t(r.columns)*r.rows)||std::floor(k.value.x)!=k.value.x)return fail("Round frame rejected");}if((t.property==5||t.property==10||t.property==14)&&(k.value.x<0||k.value.x>1))return fail("Round alpha rejected");if(t.property==7&&k.value.x!=0&&k.value.x!=1)return fail("Round visibility rejected");if(t.property==17&&k.value.x!=0)return fail("Round unsupported outline rejected");}}
 if(used_keys.size()!=count(RoundSection::Keys))return fail("Round orphan keys rejected");
 for(uint32_t i=0;i<count(RoundSection::Events);++i){auto e=v.event(i);if(e.media>=count(RoundSection::Media)||!std::isfinite(e.time)||e.time<0||e.time>v.media(e.media).duration||!e.kind||e.kind>uint32_t(version==2?RoundEventKind::PartyReturnLanded:RoundEventKind::BossKillEnemies))return fail("Round event opcode/span rejected");}
 std::set<uint32_t>bindings;for(uint32_t i=0;i<count(RoundSection::PresentationBindings);++i){auto*p=v.record(RoundSection::PresentationBindings,i);auto k=u32(p),index=u32(p+4);if(!k||k>=uint32_t(RoundPresentationSlot::Count)||!bindings.insert(k).second||index>=count(RoundSection::Media))return fail("Round presentation binding rejected");}
 std::set<uint32_t>params;for(uint32_t i=0;i<count(RoundSection::Parameters);++i){auto*p=v.record(RoundSection::Parameters,i);auto k=u32(p);if(!k||k>=uint32_t(RoundParameter::Count)||!params.insert(k).second||!finite(value(p+4)))return fail("Round parameter rejected");}
 if(!params.empty()){
  auto text=v.parameter(RoundParameter::TextTiming),speed=v.parameter(RoundParameter::TextTagSpeeds),digits=v.parameter(RoundParameter::HpDigits),hit=v.parameter(RoundParameter::PartyHit),shown=v.parameter(RoundParameter::PartyShown),random=v.parameter(RoundParameter::FlyingNumberRandom),glyph=v.parameter(RoundParameter::DamageGlyphGrid),pointer=v.parameter(RoundParameter::TargetPointerOffset),glow=v.parameter(RoundParameter::TargetGlow),margins=v.parameter(RoundParameter::DialogueMargins),layout=v.parameter(RoundParameter::DialogueTextLayout),smash=v.parameter(RoundParameter::SmashTiming),bounce=v.parameter(RoundParameter::PartyBounceMotion),plate=v.parameter(RoundParameter::PlateHitIntensity);
  if(text.x<=0||text.y<=0||text.z<=0||text.w<=0||speed.x<=0||speed.y<=0||speed.z<=0)return fail("Round text timing parameter rejected");
  if(digits.x<2||std::floor(digits.x)!=digits.x||digits.y!=v.rule(RoundRule::HpTransitionFrames))return fail("Round digit parameter rejected");
  if(hit.x<=0||hit.y<=0||hit.z<=0||hit.w<=0||shown.x<=0||shown.y<=0||shown.z<=0||shown.w<=0||random.x<0||random.y<random.x)return fail("Round motion parameter rejected");
  if(glyph.x<0||std::floor(glyph.x)!=glyph.x||glyph.y<=0||glyph.z<=0||glyph.w<=0||pointer.x<=0)return fail("Round glyph/pointer parameter rejected");
  if(glow.x<0||glow.x>1||glow.y<0||glow.y>1||glow.z<0||glow.z>1||glow.w<0||glow.w>1||margins.x<0||margins.y<0||margins.z<0||margins.w<0||layout.z<=0||layout.w<=0)return fail("Round text/color parameter rejected");
  if(smash.x<=0||smash.x>1||smash.y<=0||bounce.x<=0||bounce.y<=0||bounce.z<=0||bounce.w<=0||plate.x<=0||plate.y<=0||plate.z<0)return fail("Round effect timing parameter rejected");
 }
 if(count(RoundSection::Media)){
  auto geometry=v.parameter(RoundParameter::ReturnPartyGeometry);if(geometry.z<=0||geometry.w<=0)return fail("Round return geometry rejected");
  auto camera=v.parameter(RoundParameter::ReturnCamera);if(camera.x<=0||camera.x>120||camera.y||camera.z||camera.w)return fail("Round bounded return camera rejected");
  auto turn=v.parameter(RoundParameter::ReturnPartyTurn);if(turn.x*turn.x+turn.y*turn.y!=1||turn.z<=0||turn.w)return fail("Round return party turn rejected");
  for(auto pair: {std::pair<RoundPresentationSlot,RoundMediaRole>{RoundPresentationSlot::PartyVictory,RoundMediaRole::PartySprite},{RoundPresentationSlot::VictoryBanner,RoundMediaRole::HitEffect},{RoundPresentationSlot::ReturnTop,RoundMediaRole::TransitionRect},{RoundPresentationSlot::ReturnBottom,RoundMediaRole::TransitionRect},{RoundPresentationSlot::ReturnPlate,RoundMediaRole::PartyPlate},{RoundPresentationSlot::ReturnTimeline,RoundMediaRole::TransitionRect},{RoundPresentationSlot::PartyJumpToWorld,RoundMediaRole::WorldParty}})if(v.presentation(pair.first)>=count(RoundSection::Media)||v.media(v.presentation(pair.first)).role!=uint32_t(pair.second))return fail("Round victory presentation role rejected");
  auto timeline=v.presentation(RoundPresentationSlot::ReturnTimeline),jump=v.presentation(RoundPresentationSlot::PartyJumpToWorld);auto tm=v.media(timeline),jm=v.media(jump);
  if(tm.event_count!=5||jm.event_count!=1)return fail("Round victory callback count rejected");
  for(uint32_t i=0;i<tm.event_count;++i)if(v.event(tm.first_event+i).kind!=uint32_t(RoundEventKind::TurnPartyToWorld)+i)return fail("Round victory callback order rejected");
  if(v.event(jm.first_event).kind!=uint32_t(RoundEventKind::PartyReturnLanded))return fail("Round return landed callback rejected");
  for(uint32_t i=0;i<count(RoundSection::Events);++i){auto e=v.event(i);if(e.kind>=uint32_t(RoundEventKind::TurnPartyToWorld)&&e.kind<=uint32_t(RoundEventKind::PartyReturnLanded)&&e.media!=(e.kind==uint32_t(RoundEventKind::PartyReturnLanded)?jump:timeline))return fail("Round victory callback owner rejected");}
  if(jm.resource>=count(RoundSection::Resources)||v.resource(jm.resource).kind!=1)return fail("Round return party resource rejected");
  auto frames=v.parameter(RoundParameter::ReturnPartyFrames);auto res=v.resource(jm.resource);auto frame_count=float(uint64_t(res.columns)*res.rows);
  if(frames.x<0||frames.x>=frame_count||std::floor(frames.x)!=frames.x||frames.y<0||frames.y>=frame_count||std::floor(frames.y)!=frames.y||frames.z||frames.w)return fail("Round return party frame rejected");
 }
 if(count(RoundSection::Media)){if(bindings.size()!=uint32_t(RoundPresentationSlot::Count)-1||params.size()!=uint32_t(RoundParameter::Count)-1)return fail("Round incomplete presentation");}else if(!bindings.empty()||!params.empty()||count(RoundSection::Tracks)||count(RoundSection::Events)||count(RoundSection::Resources))return fail("Round missing media");
 if(control&&control->stopped())return fail("Checked pack preparation cancelled");
 std::vector<uint8_t>copy(input,input+size);bytes_.swap(copy);error.clear();return true;
}
bool BattleRoundData::load_file(const char*path,std::string&error){std::vector<uint8_t>bytes;if(!encore::read_file(path,bytes,max_bytes,error))return false;return load(bytes.data(),bytes.size(),error);}
}
