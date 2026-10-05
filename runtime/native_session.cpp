#include "encore/native_session.hpp"
#include "encore/progression.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>

namespace encore::upstream { namespace {
uint32_t integer(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(int j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
bool fail(std::string&e,const char*s){e=s;return false;}
struct Reader {
 const uint8_t*p;size_t n;bool ok=true;
 uint32_t u32(){if(n<4){ok=false;return 0;}const auto v=integer(p);p+=4;n-=4;return v;}
 double number(){if(n<8){ok=false;return 0;}uint64_t u=uint64_t(integer(p))|uint64_t(integer(p+4))<<32;double v;std::memcpy(&v,&u,8);p+=8;n-=8;if(!std::isfinite(v))ok=false;return v;}
 uint32_t count(uint32_t limit){const auto v=u32();if(v>limit){ok=false;return 0;}return v;}
 std::string text(){const auto length=count(4096);if(length>n){ok=false;return {};}std::string s(reinterpret_cast<const char*>(p),length);p+=length;n-=length;for(unsigned char c:s)if(c<32||c>126)ok=false;return s;}
};
bool same_items(const std::vector<SessionItem>&a,const std::vector<SessionItem>&b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(a[i].item_id!=b[i].item_id||a[i].equipped!=b[i].equipped||a[i].doses!=b[i].doses)return false;return true;}
bool same_affinities(const std::vector<SessionRealValue>&a,const std::vector<SessionRealValue>&b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(a[i].id!=b[i].id||a[i].value!=b[i].value)return false;return true;}
bool same_integers(const std::vector<SessionIntegerValue>&a,const std::vector<SessionIntegerValue>&b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(a[i].id!=b[i].id||a[i].value!=b[i].value)return false;return true;}
bool same_character(const SessionCharacter&a,const SessionCharacter&b,bool names=false,bool uids=false){
 if(a.character_id!=b.character_id||(names&&a.nickname!=b.nickname)||a.level!=b.level||a.experience!=b.experience||a.hp!=b.hp||a.pp!=b.pp||!same_integers(a.permanent_boosts,b.permanent_boosts)||!same_affinities(a.affinity_multipliers,b.affinity_multipliers)||a.learned_skills!=b.learned_skills||a.status.size()!=b.status.size()||!same_items(a.inventory,b.inventory))return false;
 for(size_t i=0;i<a.status.size();++i)if(a.status[i].status_id!=b.status[i].status_id||a.status[i].passive_healing_turns!=b.status[i].passive_healing_turns)return false;
 if(uids)for(size_t i=0;i<a.inventory.size();++i)if(a.inventory[i].uid!=b.inventory[i].uid)return false;
 return true;
}
bool supported_roster(const NativeSessionData&data,const SessionSnapshot&s){
 const auto&startup=data.startup_characters();
 if(s.party!=data.defaults().party||s.characters.empty()||s.characters[0].character_id!=data.leader_id())return false;
 if(s.characters.size()==1)return true; // Legacy saves never receive injected records.
 if(s.characters.size()!=startup.size())return false;
 for(size_t i=1;i<startup.size();++i)if(!same_character(s.characters[i],startup[i]))return false;
 return true;
}
const NativeSessionLevel* level_row(const NativeSessionData&data,int64_t level){for(const auto&row:data.levels())if(row.level==level)return &row;return nullptr;}
bool contains(const std::vector<std::string>&v,const std::string&s){return std::find(v.begin(),v.end(),s)!=v.end();}
bool supported_inventory(const NativeSessionData&data,ItemView items,const SessionSnapshot&s,std::string&e){
 const auto&inventory=s.characters[0].inventory;const auto&initial=data.defaults().characters[0].inventory;
 if(initial.size()!=items.count(ItemSection::Instances)||inventory.size()<initial.size()||inventory.size()>items.metadata().capacity)return fail(e,"Native session unsupported inventory size");
 for(size_t i=0;i<initial.size();++i){const auto&item=inventory[i];const auto instance=items.initial_instance(uint32_t(i));
  if(instance.definition>=items.count(ItemSection::Definitions))return fail(e,"Native session initial inventory definition rejected");
  const auto definition=items.definition(instance.definition);
  if(item.item_id!=initial[i].item_id||item.equipped!=initial[i].equipped||item.doses!=initial[i].doses||item.item_id!=items.string(definition.source)||item.equipped!=(instance.equipped!=0)||item.doses!=instance.doses)return fail(e,"Native session initial item identity/doses/equipment mismatch");
 }
 std::vector<uint32_t>counts(data.acquisitions().size());
 for(size_t i=initial.size();i<inventory.size();++i){const auto&item=inventory[i];size_t at=0;while(at<data.acquisitions().size()&&data.acquisitions()[at].item_id!=item.item_id)++at;
  if(at==data.acquisitions().size())return fail(e,"Native session unknown acquired item");
  const auto&policy=data.acquisitions()[at];bool definition=false;for(uint32_t j=0;j<items.count(ItemSection::Definitions);++j)if(items.string(items.definition(j).source)==item.item_id)definition=true;
  bool flag=false;for(const auto&f:s.flags)if(f.id==policy.flag_id)flag=f.value;
  if(!definition||item.equipped||item.doses!=policy.doses||++counts[at]>policy.max_count||!flag)return fail(e,"Native session acquired item doses/equipment/count/flag rejected");
 }
 return true;
}
}

bool NativeSessionData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<24||n>session_save_max_bytes)return fail(e,"Native session pack size rejected");
 const auto schema=integer(p+8),capability=integer(p+20);
 if(std::memcmp(p,"ENCNSESS",8)||schema<1||schema>4||schema!=capability||integer(p+12)!=n)return fail(e,"Native session schema/size/capability rejected");
 if(crc(p+24,n-24)!=integer(p+16))return fail(e,"Native session CRC mismatch");
 NativeSessionData d;Reader r{p+24,n-24};d.compatibility_={r.u32(),r.u32(),r.u32()};
 const auto template_size=r.count(uint32_t(session_save_max_bytes));
 if(!r.ok||template_size>r.n||!decode_session_save(r.p,template_size,d.compatibility_,d.defaults_,e))return fail(e,"Native session initial snapshot rejected");
 r.p+=template_size;r.n-=template_size;
 if(d.defaults_.characters.size()!=1||d.defaults_.party.size()!=1||d.defaults_.party[0]!=d.defaults_.characters[0].character_id||!d.defaults_.characters[0].permanent_boosts.empty()||!d.defaults_.characters[0].status.empty()||!d.defaults_.object_flags.empty()||!d.defaults_.seen_dialogue_flags.empty()||!d.defaults_.encountered.empty()||!d.defaults_.rare_drops.empty()||!d.defaults_.storage.empty()||!d.defaults_.saved_at.empty())return fail(e,"Native session initial scope rejected");
 const auto count=r.count(32);for(uint32_t i=0;i<count;++i){NativeSessionLevel row;row.level=r.u32();row.minimum_exp=r.u32();row.next_exp=r.u32();for(auto&stat:row.stats){const auto value=r.u32();if(value>INT32_MAX)return fail(e,"Native session negative stat rejected");stat=int32_t(value);}const auto skills=r.count(256);std::set<std::string>unique;for(uint32_t j=0;j<skills;++j){auto s=r.text();if(s.empty()||!unique.insert(s).second)return fail(e,"Native session skill identity rejected");row.skills.push_back(std::move(s));}
  if(!row.level||row.level>32||row.minimum_exp>=row.next_exp||row.next_exp>INT32_MAX||!row.stats[0]||(i&&row.level<=d.levels_.back().level))return fail(e,"Native session progression row rejected");
  d.levels_.push_back(std::move(row));}
 if(d.levels_.empty())return fail(e,"Native session needs source stat rows");
 d.saved_flag_=r.text();d.earned_cash_flag_=r.text();std::set<std::string>unique;
 auto count_flags=r.count(4096);for(uint32_t i=0;i<count_flags;++i){auto s=r.text();if(s.empty()||!unique.insert(s).second)return fail(e,"Native session mutable flag rejected");d.mutable_flags_.push_back(std::move(s));}
 if(d.saved_flag_.empty()||d.earned_cash_flag_.empty()||!contains(d.mutable_flags_,d.saved_flag_)||!contains(d.mutable_flags_,d.earned_cash_flag_))return fail(e,"Native session source flag bindings rejected");
 const auto areas=r.count(4096);std::set<uint32_t>ids;for(uint32_t i=0;i<areas;++i){auto id=r.u32();if(!id||!ids.insert(id).second)return fail(e,"Native session camera scope rejected");d.camera_area_ids_.push_back(id);}if(d.camera_area_ids_.empty())return fail(e,"Native session needs supported room bounds");
 d.startup_characters_=d.defaults_.characters;
 if(schema>=2){
  const auto startup_size=r.count(uint32_t(session_save_max_bytes));SessionSnapshot startup;
  if(!r.ok||startup_size>r.n||!decode_session_save(r.p,startup_size,d.compatibility_,startup,e))return fail(e,"Native session startup snapshot rejected");
  r.p+=startup_size;r.n-=startup_size;
  if(startup.characters.size()<2||!startup.characters[0].nickname.empty()||!same_character(startup.characters[0],d.defaults_.characters[0],false,true))return fail(e,"Native session startup leader/scope rejected");
  for(size_t i=1;i<startup.characters.size();++i){const auto&character=startup.characters[i];if(!character.nickname.empty()||!character.permanent_boosts.empty()||!character.status.empty())return fail(e,"Native session startup inactive scope rejected");}
  d.startup_characters_=startup.characters;startup.characters=d.defaults_.characters;
  std::vector<uint8_t>legacy_bytes,startup_bytes;
  if(!encode_session_save(d.defaults_,d.compatibility_,legacy_bytes,e)||!encode_session_save(startup,d.compatibility_,startup_bytes,e)||legacy_bytes!=startup_bytes)return fail(e,"Native session startup changed legacy world defaults");
 }
 d.text_speeds_={d.defaults_.settings.text_speed};d.menu_flavors_={d.defaults_.settings.menu_flavor};d.button_prompts_={d.defaults_.settings.button_prompts};
 if(schema>=3){
  d.text_speeds_.clear();auto n=r.count(16);for(uint32_t i=0;i<n;++i){const double v=r.number();if(v<=0||v>1||std::find(d.text_speeds_.begin(),d.text_speeds_.end(),v)!=d.text_speeds_.end())return fail(e,"Native session text speed choice rejected");d.text_speeds_.push_back(v);}
  for(auto*list:{&d.menu_flavors_,&d.button_prompts_}){list->clear();n=r.count(32);for(uint32_t i=0;i<n;++i){auto v=r.text();if(v.empty()||contains(*list,v))return fail(e,"Native session settings choice rejected");list->push_back(std::move(v));}}
 }
 if(schema>=4){
  const auto count=r.count(16);std::set<std::string>items,flags;
  for(uint32_t i=0;i<count;++i){NativeSessionAcquisition policy;policy.item_id=r.text();policy.doses=r.u32();policy.max_count=r.u32();policy.flag_id=r.text();
   bool initial_item=false;for(const auto&item:d.defaults_.characters[0].inventory)if(item.item_id==policy.item_id)initial_item=true;
   bool initial_false=false;for(const auto&flag:d.defaults_.flags)if(flag.id==policy.flag_id&&!flag.value)initial_false=true;
   if(policy.item_id.empty()||policy.flag_id.empty()||!items.insert(policy.item_id).second||!flags.insert(policy.flag_id).second||!policy.doses||policy.doses>65535||policy.max_count!=1||initial_item||!initial_false||!contains(d.mutable_flags_,policy.flag_id))return fail(e,"Native session acquired item policy rejected");
   d.acquisitions_.push_back(std::move(policy));
  }
 }
 if(!r.ok||r.n||d.text_speeds_.empty()||d.menu_flavors_.empty()||d.button_prompts_.empty()||!d.supports_settings(d.defaults_.settings))return fail(e,"Native session malformed/trailing settings payload");
 const auto*initial=level_row(d,d.defaults_.characters[0].level);
 const auto&c=d.defaults_.characters[0];if(!initial||c.experience!=initial->minimum_exp||c.hp>initial->stats[0]||c.pp>initial->stats[1]||c.learned_skills!=initial->skills)return fail(e,"Native session initial character disagrees with source rows");
 std::set<std::string>flag_ids;for(const auto&f:d.defaults_.flags)flag_ids.insert(f.id);for(const auto&id:d.mutable_flags_)if(!flag_ids.count(id))return fail(e,"Native session mutable flag is unregistered");
 d.valid_=true;*this=std::move(d);e.clear();return true;
}
bool NativeSessionData::load_file(const char*path,std::string&e){
 if(!path)return fail(e,"Missing native session pack path");
 FILE*f=std::fopen(path,"rb");if(!f)return fail(e,"Cannot open native session pack");std::vector<uint8_t>b;uint8_t block[4096];bool good=true;while(true){const auto n=std::fread(block,1,sizeof(block),f);if(b.size()+n>session_save_max_bytes){good=false;break;}b.insert(b.end(),block,block+n);if(n<sizeof(block)){good=!std::ferror(f);break;}}if(std::fclose(f))good=false;if(!good)return fail(e,"Native session bounded file read failed");return load(b.data(),b.size(),e);
}

bool NativeSessionData::supports_settings(const SessionSettings&s)const{
 return std::find(text_speeds_.begin(),text_speeds_.end(),s.text_speed)!=text_speeds_.end()&&contains(menu_flavors_,s.menu_flavor)&&contains(button_prompts_,s.button_prompts)&&s.description==defaults_.settings.description;
}

bool validate_native_session_snapshot(const NativeSessionData&data,RoomView room,HouseView house,RoundView round,ItemView items,const SessionSnapshot&s,std::string&e){
 if(!data.valid()||!room.valid()||!house.valid()||!round.valid()||!items.valid())return fail(e,"Native session requires every checked resource");
 if(!validate_session_snapshot(s,e))return false;
 if(integer(room.bytes()+28)!=data.compatibility().content_family||integer(room.bytes()+32)!=data.compatibility().rules_revision)return fail(e,"Native session room compatibility mismatch");
 const auto&d=data.defaults();const auto scene=room.scene();
 if(s.scene_id!=d.scene_id||s.scene_id!=room.string(scene.source_scene_string)||s.scene_label!=d.scene_label||s.scene_label!=room.string(scene.display_name_string)||s.source_version!=d.source_version||s.source_version!=room.string(scene.version_string))return fail(e,"Native session scene/version identity mismatch");
 if(!supported_roster(data,s))return fail(e,"Native session requires legacy singleton or complete frozen startup roster; party stays singleton");
 for(const auto&character:s.characters)if(character.nickname.size()>house.interaction().max_player_name_length)return fail(e,"Native session nickname exceeds source scope");
 if(s.run_sound!=d.run_sound||s.shadow_effect!=d.shadow_effect||!data.supports_settings(s.settings))return fail(e,"Native session unsupported movement/settings state");
 if(s.rng_policy!=SessionRngPolicy::NotSerialized)return fail(e,"Native session unsupported RNG policy");
 const auto&c=s.characters[0];const auto&initial=d.characters[0];
 if(!c.permanent_boosts.empty()||!c.status.empty()||!same_affinities(c.affinity_multipliers,initial.affinity_multipliers))return fail(e,"Native session unsupported permanent boost/status/affinity state");
 const auto*row=level_row(data,c.level);
 if(!row||c.experience<row->minimum_exp||c.experience>=row->next_exp||level_for_experience(room,int32_t(c.experience))!=c.level||row->minimum_exp!=room.experience(row->level-1)||row->next_exp!=room.experience(row->level))return fail(e,"Native session unsupported source progression");
 if(c.hp>row->stats[0]||c.pp>row->stats[1]||c.learned_skills!=row->skills)return fail(e,"Native session HP/PP/skills disagree with source level");
 if(!supported_inventory(data,items,s,e))return false;
 if(!same_items(s.key_items,d.key_items)||!s.storage.empty()||!s.object_flags.empty()||!same_integers(s.keys,d.keys)||!s.rare_drops.empty())return fail(e,"Native session unsupported key/storage/object/counter state");
 if(s.flags.size()!=room.flag_count()||s.flags.size()!=d.flags.size())return fail(e,"Native session requires the complete story flag registry");
 for(const auto&f:s.flags){auto found=std::find_if(d.flags.begin(),d.flags.end(),[&](const SessionFlag&x){return x.id==f.id;});if(found==d.flags.end())return fail(e,"Native session unknown story flag");bool in_room=false;for(uint32_t i=0;i<room.flag_count();++i)if(room.string(room.flag(i).name_string)==f.id){in_room=true;break;}if(!in_room||(f.value!=found->value&&!contains(data.mutable_flags(),f.id)))return fail(e,"Native session unsupported story flag state");}
 std::set<std::string>seen;for(uint32_t i=0;i<house.count(HouseSection::Npcs);++i){const auto key=house.string(house.npc(i).seen_key);if(!key.empty())seen.emplace(key);}for(uint32_t i=0;i<house.count(HouseSection::Overrides);++i){const auto key=house.string(house.override_dialogue(i).seen_key);if(!key.empty())seen.emplace(key);}for(const auto&f:s.seen_dialogue_flags)if(!seen.count(f.id))return fail(e,"Native session unknown seen dialogue identity");
 std::set<std::string>encountered;for(uint32_t i=0;i<room.battle_count();++i)encountered.emplace(room.string(room.battle(i).enemy_string));for(const auto&f:s.encountered)if(!encountered.count(f.id))return fail(e,"Native session unknown encounter identity");
 bool position_ok=false;for(auto id:data.camera_area_ids()){bool exists=false;for(uint32_t i=0;i<room.camera_area_count();++i){const auto area=room.camera_area(i);if(area.stable_id!=id)continue;exists=true;if(std::abs(s.position_x-area.center.x)<=area.extents.x&&std::abs(s.position_y-area.center.y)<=area.extents.y)position_ok=true;}if(!exists)return fail(e,"Native session scene bounds identity mismatch");}if(!position_ok)return fail(e,"Native session position outside supported house rooms");
 // Source player directions allow normalized or unnormalized eight-way vectors.
 const double x=std::abs(s.direction_x),y=std::abs(s.direction_y);const bool cardinal=(x==0&&y==1)||(x==1&&y==0);const bool diagonal=x==y&&(x==1||std::abs(x-std::sqrt(.5))<1e-6);if(!cardinal&&!diagonal)return fail(e,"Native session unsupported facing direction");
 const auto promoted=round.encounter().promoted_level;
 if(promoted){const auto*growth=level_row(data,promoted);if(!growth||round.count(RoundSection::Growth)!=growth->stats.size()||!contains(growth->skills,std::string(round.string(round.encounter().learned_skill))))return fail(e,"Native session live round progression binding mismatch");for(uint32_t i=0;i<round.count(RoundSection::Growth);++i){auto g=round.growth(i);if(!g.stat||g.stat>growth->stats.size()||g.after!=uint32_t(growth->stats[g.stat-1]))return fail(e,"Native session live round stat rule mismatch");}}
 e.clear();return true;
}

bool build_native_session_snapshot(const NativeSessionData&data,RoomView room,HouseView house,RoundView round,ItemView items,const NativeSnapshotInput&input,SessionSnapshot&out,std::string&e){
 if(!data.valid()||!input.stats||!input.inventory||!input.inventory->valid()||!items.valid())return fail(e,"Native session needs explicit live stats and inventory");
 if(!supported_roster(data,input.state))return fail(e,"Native session input character scope rejected");
 if(!validate_session_snapshot(input.state,e))return false;
 if(!supported_inventory(data,items,input.state,e))return false;
 for(const auto&skill:input.state.characters[0].learned_skills){bool known=false;for(const auto&row:data.levels())if(contains(row.skills,skill))known=true;if(!known)return fail(e,"Native session persistent skill identity rejected");}
 SessionSnapshot next=input.state;auto&c=next.characters[0];const auto&live=*input.stats;const auto*row=level_row(data,live.level);
 if(!row||row->stats!=std::array<int32_t,7>{{live.maxhp,live.maxpp,live.offense,live.defense,live.speed,live.iq,live.guts}})return fail(e,"Native session live derived stats disagree with source/equipment; unsupported boost");
 c.level=live.level;c.experience=live.experience;c.hp=live.hp;c.pp=live.pp;next.cash=live.cash;next.bank=live.bank;next.earned_cash=live.earned_cash;
 // BattleSessionStats historically tracks skills learned during this slice.
 // Keep the external initial list plus that explicit live delta, with no extras.
 c.learned_skills=data.defaults().characters[0].learned_skills;std::set<std::string>unique;
 for(const auto&skill:live.learned_skills){if(!unique.insert(skill).second)return fail(e,"Native session duplicate live skill");if(!contains(c.learned_skills,skill))c.learned_skills.push_back(skill);}
 if(input.inventory->size()>items.metadata().capacity)return fail(e,"Native session live inventory capacity rejected");
 c.inventory.clear();const auto source=input.inventory->content();
 for(uint32_t i=0;i<input.inventory->size();++i){const auto&instance=input.inventory->instance(i);if(instance.definition>=source.count(ItemSection::Definitions)||instance.definition>=items.count(ItemSection::Definitions))return fail(e,"Native session live item definition rejected");const auto definition=source.definition(instance.definition),expected=items.definition(instance.definition);if(source.string(definition.source)!=items.string(expected.source))return fail(e,"Native session live item pack mismatch");c.inventory.push_back({std::string(source.string(definition.source)),instance.equipped!=0,instance.doses,instance.id});}
 if(!validate_native_session_snapshot(data,room,house,round,items,next,e))return false;
 out=std::move(next);e.clear();return true;
}
}
