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
bool identity(std::string_view s){if(s.empty()||s.size()>128)return false;for(char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;return true;}
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
bool supported_doses(const NativeSessionData&data,const SessionItem&item,uint32_t initial){
 const auto p=std::find_if(data.consumables().begin(),data.consumables().end(),[&](const NativeSessionConsumable&p){return p.item_id==item.item_id;});
 return p==data.consumables().end()?item.doses==initial:!item.equipped&&item.doses>0&&item.doses<=initial&&initial==p->max_doses;
}
bool supported_inventory(const NativeSessionData&data,ItemView items,const SessionSnapshot&s,std::string&e){
 if(data.storage_capacity()){
  if(s.characters[0].inventory.size()>items.metadata().capacity||s.storage.size()>data.storage_capacity())return fail(e,"Native session inventory/storage capacity rejected");
  std::vector<uint32_t>counts(data.storage_policies().size());
  auto check=[&](const SessionItem&item,bool stored){
   size_t at=0;while(at<data.storage_policies().size()&&data.storage_policies()[at].item_id!=item.item_id)++at;
   if(at==data.storage_policies().size())return fail(e,"Native session unknown storage item");
   const auto&p=data.storage_policies()[at];uint32_t definition=item_no_index;
   for(uint32_t j=0;j<items.count(ItemSection::Definitions);++j)if(items.string(items.definition(j).source)==item.item_id){if(definition!=item_no_index)return fail(e,"Native session ambiguous storage definition");definition=j;}
   if(definition==item_no_index||!supported_doses(data,item,p.doses)||++counts[at]>p.total_count||(stored&&item.equipped))return fail(e,"Native session storage doses/count/equipment rejected");
   if(item.equipped&&(!(items.definition(definition).flags&uint32_t(ItemDefinitionFlag::Equipment))||std::all_of(p.boosts.begin(),p.boosts.end(),[](int32_t n){return n==0;})))return fail(e,"Native session unsupported storage equipment");
   if(!p.required){auto a=std::find_if(data.acquisitions().begin(),data.acquisitions().end(),[&](const NativeSessionAcquisition&a){return a.item_id==item.item_id;});if(a==data.acquisitions().end())return fail(e,"Native session storage acquisition binding missing");bool flag=false;for(auto&f:s.flags)if(f.id==a->flag_id)flag=f.value;if(!flag)return fail(e,"Native session storage acquisition flag rejected");}
   return true;
  };
  for(auto&i:s.characters[0].inventory)if(!check(i,false))return false;
  for(auto&i:s.storage)if(!check(i,true))return false;
  for(size_t i=0;i<counts.size();++i)if(data.storage_policies()[i].required&&counts[i]!=data.storage_policies()[i].total_count)return fail(e,"Native session required item conservation rejected");
  return true;
 }
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
  if(!definition||item.equipped||!supported_doses(data,item,policy.doses)||++counts[at]>policy.max_count||!flag)return fail(e,"Native session acquired item doses/equipment/count/flag rejected");
 }
 return true;
}
bool enabled_flag(const SessionSnapshot&s,const std::string&id){for(const auto&f:s.flags)if(f.id==id)return f.value;return false;}
bool known_skill(const NativeSessionData&data,const std::string&skill){for(const auto&row:data.levels())if(contains(row.skills,skill))return true;for(const auto&p:data.skill_policies())if(p.skill_id==skill)return true;return false;}
bool supported_story_keys(const NativeSessionData&data,const SessionSnapshot&s,std::string&e){
 if(data.key_policies().empty())return same_items(s.key_items,data.defaults().key_items)||fail(e,"Native session legacy key state rejected");
 std::vector<uint32_t>counts(data.key_policies().size(),0);
 for(const auto&item:s.key_items){const auto at=std::find_if(data.key_policies().begin(),data.key_policies().end(),[&](const auto&p){return p.item_id==item.item_id;});if(at==data.key_policies().end())return fail(e,"Native session unknown key item");const auto index=size_t(at-data.key_policies().begin());if(item.equipped||item.doses!=at->doses||++counts[index]>at->max_count||(!at->required&&!enabled_flag(s,at->flag_id)))return fail(e,"Native session key doses/count/source flag rejected");}
 for(size_t i=0;i<counts.size();++i)if(data.key_policies()[i].required&&counts[i]!=data.key_policies()[i].max_count)return fail(e,"Native session required key absent");return true;
}
bool supported_story_skills(const NativeSessionData&data,const NativeSessionLevel&row,const SessionSnapshot&s,std::string&e){
 const auto&c=s.characters[0];if(data.skill_order().empty())return c.learned_skills==row.skills||fail(e,"Native session legacy level skill mismatch");
 std::set<std::string>unique;size_t previous=0;bool first=true;
 for(const auto&skill:c.learned_skills){const auto order=std::find(data.skill_order().begin(),data.skill_order().end(),skill);if(order==data.skill_order().end()||!unique.insert(skill).second)return fail(e,"Native session unknown/duplicate skill");const auto rank=size_t(order-data.skill_order().begin());if(!first&&rank<=previous)return fail(e,"Native session skills not source sorted");first=false;previous=rank;
  if(!contains(row.skills,skill)){const auto p=std::find_if(data.skill_policies().begin(),data.skill_policies().end(),[&](const auto&p){return p.character_id==c.character_id&&p.skill_id==skill;});if(p==data.skill_policies().end()||!enabled_flag(s,p->flag_id))return fail(e,"Native session story skill owner/flag rejected");}
 }
 for(const auto&required:row.skills)if(!unique.count(required))return fail(e,"Native session required level skill absent");return true;
}
}

bool NativeSessionData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<24||n>session_save_max_bytes)return fail(e,"Native session pack size rejected");
 const auto schema=integer(p+8),capability=integer(p+20);
 if(std::memcmp(p,"ENCNSESS",8)||schema<1||schema>7||schema!=capability||integer(p+12)!=n)return fail(e,"Native session schema/size/capability rejected");
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
 if(schema>=5){
  d.storage_capacity_=r.u32();const auto n=r.count(16);std::set<std::string>unique;
  if(!d.storage_capacity_||d.storage_capacity_>4096||!n)return fail(e,"Native session storage policy capacity rejected");
  for(uint32_t i=0;i<n;++i){NativeSessionStoragePolicy p;p.item_id=r.text();p.doses=r.u32();p.total_count=r.u32();const auto required=r.u32();p.required=required!=0;for(auto&b:p.boosts){auto x=r.u32();if(x>65535)return fail(e,"Native session storage boost rejected");b=int32_t(x);}
   if(p.item_id.empty()||!unique.insert(p.item_id).second||!p.doses||p.doses>65535||p.total_count!=1||required>1)return fail(e,"Native session storage item policy rejected");
   const auto&initial=d.defaults_.characters[0].inventory;auto item=std::find_if(initial.begin(),initial.end(),[&](const SessionItem&i){return i.item_id==p.item_id;});
   if(p.required){if(item==initial.end()||item->doses!=p.doses)return fail(e,"Native session required storage item binding rejected");}
   else{if(item!=initial.end()||std::any_of(p.boosts.begin(),p.boosts.end(),[](int32_t n){return n!=0;}))return fail(e,"Native session acquired storage equipment rejected");auto a=std::find_if(d.acquisitions_.begin(),d.acquisitions_.end(),[&](const NativeSessionAcquisition&a){return a.item_id==p.item_id;});if(a==d.acquisitions_.end()||a->doses!=p.doses||a->max_count!=p.total_count)return fail(e,"Native session storage acquired item binding rejected");}
   d.storage_policies_.push_back(std::move(p));
  }
  for(auto&i:d.defaults_.characters[0].inventory)if(!unique.count(i.item_id))return fail(e,"Native session storage missing initial item");
  for(auto&a:d.acquisitions_)if(!unique.count(a.item_id))return fail(e,"Native session storage missing acquired item");
 }
 if(schema>=6){
  const auto n=r.count(16);std::set<std::string>items;
  for(uint32_t i=0;i<n;++i){NativeSessionConsumable policy;policy.item_id=r.text();policy.max_doses=r.u32();
   auto storage=std::find_if(d.storage_policies_.begin(),d.storage_policies_.end(),[&](const NativeSessionStoragePolicy&p){return p.item_id==policy.item_id;});
   if(policy.item_id.empty()||!items.insert(policy.item_id).second||!policy.max_doses||policy.max_doses>65535||storage==d.storage_policies_.end()||storage->required||storage->doses!=policy.max_doses||std::any_of(storage->boosts.begin(),storage->boosts.end(),[](int32_t n){return n!=0;}))return fail(e,"Native session consumable policy rejected");
   d.consumables_.push_back(std::move(policy));
  }
  const auto statuses=r.count(32);std::set<std::string>ids;
  for(uint32_t i=0;i<statuses;++i){NativeSessionStatusPolicy policy;policy.id=r.text();const auto passive=r.u32(),turns=r.u32();policy.passive_healing=passive!=0;policy.default_saved_turns=turns;
   if(policy.id.empty()||!ids.insert(policy.id).second||passive>1||turns>INT32_MAX)return fail(e,"Native session status policy rejected");
   d.status_policies_.push_back(std::move(policy));
  }
  if(d.consumables_.empty()||d.status_policies_.empty())return fail(e,"Native session empty item-use capability rejected");
 }
 if(schema>=7){
  const auto count=r.count(64);std::set<std::string>keys;
  if(!count)return fail(e,"Native session empty key policies");
  for(uint32_t i=0;i<count;++i){NativeSessionKeyPolicy policy;policy.item_id=r.text();policy.flag_id=r.text();policy.doses=r.u32();policy.max_count=r.u32();const auto required=r.u32();policy.required=required!=0;
   uint32_t initial_count=0;for(const auto&key:d.defaults_.key_items)if(key.item_id==policy.item_id){if(key.equipped||key.doses!=policy.doses)return fail(e,"Native session initial key policy mismatch");++initial_count;}
   bool source_flag=false;for(const auto&f:d.defaults_.flags)if(f.id==policy.flag_id&&!f.value)source_flag=true;
   if(!identity(policy.item_id)||!keys.insert(policy.item_id).second||!policy.doses||policy.doses>65535||policy.max_count!=1||required>1||(policy.required?(initial_count!=policy.max_count||!policy.flag_id.empty()):(initial_count||!identity(policy.flag_id)||!source_flag||!contains(d.mutable_flags_,policy.flag_id))))return fail(e,"Native session key policy rejected");
   d.key_policies_.push_back(std::move(policy));
  }
  for(const auto&key:d.defaults_.key_items)if(!keys.count(key.item_id))return fail(e,"Native session initial key policy absent");
  const auto skills=r.count(64);std::set<std::string>learned;
  if(!skills)return fail(e,"Native session empty story skill policy");
  for(uint32_t i=0;i<skills;++i){NativeSessionSkillPolicy policy{r.text(),r.text(),r.text()};bool source_flag=false;for(const auto&f:d.defaults_.flags)if(f.id==policy.flag_id&&!f.value)source_flag=true;
   if(policy.character_id!=d.leader_id()||!identity(policy.skill_id)||!identity(policy.flag_id)||!learned.insert(policy.skill_id).second||!source_flag||!contains(d.mutable_flags_,policy.flag_id))return fail(e,"Native session story skill policy rejected");
   d.skill_policies_.push_back(std::move(policy));
  }
  const auto ordering=r.count(512);std::set<std::string>order;if(!ordering)return fail(e,"Native session skill order absent");
  for(uint32_t i=0;i<ordering;++i){auto skill=r.text();if(!identity(skill)||!order.insert(skill).second)return fail(e,"Native session source skill order rejected");d.skill_order_.push_back(std::move(skill));}
  for(const auto&policy:d.skill_policies_)if(!order.count(policy.skill_id))return fail(e,"Native session story skill order missing");
  for(const auto&level:d.levels_){size_t previous=0;bool first=true;for(const auto&skill:level.skills){const auto found=std::find(d.skill_order_.begin(),d.skill_order_.end(),skill);if(found==d.skill_order_.end())return fail(e,"Native session level skill order missing");const auto rank=size_t(found-d.skill_order_.begin());if(!first&&rank<=previous)return fail(e,"Native session level skills not source sorted");previous=rank;first=false;}}
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
 if(!c.permanent_boosts.empty()||!same_affinities(c.affinity_multipliers,initial.affinity_multipliers))return fail(e,"Native session unsupported permanent boost/affinity state");
 for(const auto&status:c.status){const auto p=std::find_if(data.status_policies().begin(),data.status_policies().end(),[&](const NativeSessionStatusPolicy&p){return p.id==status.status_id;});
  if(p==data.status_policies().end()||(!p->passive_healing&&status.passive_healing_turns!=p->default_saved_turns))return fail(e,"Native session unknown status or unsupported saved turn state");
 }
 const auto*row=level_row(data,c.level);
 if(!row||c.experience<row->minimum_exp||c.experience>=row->next_exp||level_for_experience(room,int32_t(c.experience))!=c.level||row->minimum_exp!=room.experience(row->level-1)||row->next_exp!=room.experience(row->level))return fail(e,"Native session unsupported source progression");
 std::array<int32_t,7>derived{};if(!native_session_derived_stats(data,items,c,derived,e))return false;
 if(c.hp>derived[0]||c.pp>derived[1])return fail(e,"Native session HP/PP disagree with source level");
 if(!supported_story_skills(data,*row,s,e))return false;
 if(!supported_inventory(data,items,s,e))return false;
 if(!supported_story_keys(data,s,e))return false;
 if((!data.storage_capacity()&&!s.storage.empty())||!s.object_flags.empty()||!same_integers(s.keys,d.keys)||!s.rare_drops.empty())return fail(e,"Native session unsupported key/storage/object/counter state");
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

bool native_session_derived_stats(const NativeSessionData&data,ItemView items,const SessionCharacter&c,std::array<int32_t,7>&out,std::string&e){
 const auto*row=level_row(data,c.level);if(!row||!items.valid())return fail(e,"Native session derived stats resource rejected");auto next=row->stats;
 if(data.storage_capacity()){
  std::array<int64_t,7>delta{};
  auto apply=[&](const SessionItem&i,int sign){if(!i.equipped)return true;auto p=std::find_if(data.storage_policies().begin(),data.storage_policies().end(),[&](const NativeSessionStoragePolicy&p){return p.item_id==i.item_id;});if(p==data.storage_policies().end())return false;for(size_t j=0;j<7;++j)delta[j]+=sign*p->boosts[j];return true;};
  for(auto&i:data.defaults().characters[0].inventory)if(!apply(i,-1))return fail(e,"Native session initial equipment policy missing");
  std::set<uint32_t>slots;
  for(auto&i:c.inventory){if(!i.equipped)continue;uint32_t definition=item_no_index;for(uint32_t j=0;j<items.count(ItemSection::Definitions);++j)if(items.string(items.definition(j).source)==i.item_id)definition=j;
   if(definition==item_no_index||!(items.definition(definition).flags&uint32_t(ItemDefinitionFlag::Equipment))||!slots.insert(items.definition(definition).equipment_slot).second||!apply(i,1))return fail(e,"Native session equipped item/slot rejected");}
  for(size_t j=0;j<7;++j){auto value=int64_t(next[j])+delta[j];if(value<0||value>INT32_MAX||(j==0&&value==0))return fail(e,"Native session derived stat bounds rejected");next[j]=int32_t(value);}
 }
 out=next;e.clear();return true;
}

bool build_native_session_snapshot(const NativeSessionData&data,RoomView room,HouseView house,RoundView round,ItemView items,const NativeSnapshotInput&input,SessionSnapshot&out,std::string&e){
 if(!data.valid()||!input.stats||!input.inventory||!input.inventory->valid()||!items.valid())return fail(e,"Native session needs explicit live stats and inventory");
 if(!supported_roster(data,input.state))return fail(e,"Native session input character scope rejected");
 if(!validate_session_snapshot(input.state,e))return false;
 if(!supported_inventory(data,items,input.state,e))return false;

 for(const auto&skill:input.state.characters[0].learned_skills)if(!known_skill(data,skill))return fail(e,"Native session persistent skill identity rejected");
 SessionSnapshot next=input.state;auto&c=next.characters[0];const auto&live=*input.stats;const auto*row=level_row(data,live.level);
 if(!row)return fail(e,"Native session live level row unavailable");
 c.level=live.level;c.experience=live.experience;c.hp=live.hp;c.pp=live.pp;next.cash=live.cash;next.bank=live.bank;next.earned_cash=live.earned_cash;
 // BattleSessionStats historically tracks skills learned during this slice.
 // Schema7 preserves acquired story skills from the authoritative snapshot.
 // Legacy schemas retain the original defaults-plus-live-delta behavior.
 c.learned_skills=data.skill_order().empty()?data.defaults().characters[0].learned_skills:input.state.characters[0].learned_skills;std::set<std::string>unique;
 for(const auto&skill:live.learned_skills){if(!unique.insert(skill).second)return fail(e,"Native session duplicate live skill");if(!known_skill(data,skill))return fail(e,"Native session live skill identity rejected");if(!contains(c.learned_skills,skill))c.learned_skills.push_back(skill);}
 if(!data.skill_order().empty())std::sort(c.learned_skills.begin(),c.learned_skills.end(),[&](const auto&a,const auto&b){return std::find(data.skill_order().begin(),data.skill_order().end(),a)<std::find(data.skill_order().begin(),data.skill_order().end(),b);});
 if(input.inventory->size()>items.metadata().capacity)return fail(e,"Native session live inventory capacity rejected");
 c.inventory.clear();const auto source=input.inventory->content();
 for(uint32_t i=0;i<input.inventory->size();++i){const auto&instance=input.inventory->instance(i);if(instance.definition>=source.count(ItemSection::Definitions)||instance.definition>=items.count(ItemSection::Definitions))return fail(e,"Native session live item definition rejected");const auto definition=source.definition(instance.definition),expected=items.definition(instance.definition);if(source.string(definition.source)!=items.string(expected.source))return fail(e,"Native session live item pack mismatch");c.inventory.push_back({std::string(source.string(definition.source)),instance.equipped!=0,instance.doses,instance.id});}
 if(data.storage_capacity()){
  if(!input.storage||!input.storage->valid()||input.storage->capacity()!=data.storage_capacity())return fail(e,"Native session needs explicit live storage");
  next.storage.clear();const auto source=input.storage->content();
  for(const auto&instance:input.storage->instances()){if(instance.definition>=source.count(ItemSection::Definitions)||instance.definition>=items.count(ItemSection::Definitions))return fail(e,"Native session stored definition rejected");const auto d=source.definition(instance.definition),expected=items.definition(instance.definition);if(source.string(d.source)!=items.string(expected.source))return fail(e,"Native session stored item pack mismatch");next.storage.push_back({std::string(source.string(d.source)),instance.equipped!=0,instance.doses,instance.id});}
 }
 std::array<int32_t,7>expected{};if(!native_session_derived_stats(data,items,c,expected,e)||expected!=std::array<int32_t,7>{{live.maxhp,live.maxpp,live.offense,live.defense,live.speed,live.iq,live.guts}})return fail(e,"Native session live derived stats disagree with source/equipment");
 if(!validate_native_session_snapshot(data,room,house,round,items,next,e))return false;
 out=std::move(next);e.clear();return true;
}
}
