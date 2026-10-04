#include "encore/new_game_setup.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
uint32_t integer(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(int j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
bool fail(std::string&e,const char*s){e=s;return false;}
bool ascii(std::string_view s){for(unsigned char c:s)if(c<32||c>126)return false;return true;}
bool safe_path(const std::string&s){return !s.empty()&&s.front()!='/'&&s.find("..") == std::string::npos&&s.find(':')==std::string::npos&&s.find('\\')==std::string::npos;}
struct Reader {
 const uint8_t*p;size_t n;bool ok=true;
 uint32_t u32(){if(n<4){ok=false;return 0;}auto v=integer(p);p+=4;n-=4;return v;}
 uint32_t count(uint32_t limit){auto v=u32();if(v>limit){ok=false;return 0;}return v;}
 float real(){auto u=u32();float f;std::memcpy(&f,&u,4);if(!std::isfinite(f))ok=false;return f;}
 double number(){if(n<8){ok=false;return 0;}uint64_t u=uint64_t(integer(p))|uint64_t(integer(p+4))<<32;double f;std::memcpy(&f,&u,8);p+=8;n-=8;if(!std::isfinite(f))ok=false;return f;}
 std::string text(){auto k=count(4096);if(k>n){ok=false;return{};}std::string s(reinterpret_cast<const char*>(p),k);p+=k;n-=k;if(s.find('\0')!=std::string::npos)ok=false;return s;}
 std::vector<std::string>texts(){std::vector<std::string>v;auto k=count(512);for(uint32_t i=0;i<k;++i)v.push_back(text());return v;}
 NamingRect rect(){return {real(),real(),real(),real()};}
 NamingAnimation animation(double length){NamingAnimation a;a.length=length;auto k=count(128);for(uint32_t i=0;i<k;++i){NamingFrame f{number(),u32()};if(f.time<0||f.time>=length||(i&&f.time<=a.keys.back().time))ok=false;a.keys.push_back(f);}if(length<=0||length>60||a.keys.empty()||a.keys.front().time!=0)ok=false;return a;}
};
}
uint32_t NamingAnimation::frame(double elapsed)const{if(keys.empty()||length<=0)return 0;double t=std::fmod(std::max(0.,elapsed),length);uint32_t result=keys.front().frame;for(const auto&k:keys){if(t<k.time)break;result=k.frame;}return result;}
bool NewGameSetupData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<24||n>1024*1024||std::memcmp(p,"ENCNAMES",8)||integer(p+8)!=3||integer(p+12)!=n||integer(p+20)!=2)return fail(e,"Naming schema/size/capability rejected");
 if(crc(p+24,n-24)!=integer(p+16))return fail(e,"Naming CRC mismatch");
 NewGameSetupData d;Reader r{p+24,n-24};d.maximum=r.u32();d.target=r.text();d.initial=r.text();d.other_initial=r.texts();d.defaults=r.texts();d.blacklist=r.texts();d.texts=r.texts();d.sounds=r.texts();d.cancel_delay=r.number();d.error_duration=r.number();d.font_height=r.real();
 auto count=r.count(32);for(uint32_t i=0;i<count;++i)d.layouts.push_back(r.rect());for(auto&x:d.patch)x=r.u32();for(auto&x:d.colors)x=r.u32();count=r.count(32);
 for(uint32_t i=0;i<count;++i){NamingResource a{r.text(),r.u32(),r.u32(),r.u32(),r.u32()};if(!safe_path(a.path)||!a.width||!a.height||a.width>1024||a.height>1024||!a.columns||!a.rows||a.width%a.columns||a.height%a.rows)return fail(e,"Naming texture rejected");d.resources.push_back(a);}
 count=r.count(8);for(uint32_t i=0;i<count;++i){std::vector<NamingKey>panel;auto keys=r.count(512);for(uint32_t j=0;j<keys;++j){NamingKey k;k.codepoint=r.u32();k.kind=r.u32();k.value=r.text();k.rect=r.rect();for(auto&v:k.neighbors)v=r.u32();if(k.kind>3||(!k.kind&&k.value.size()>1)||!ascii(k.value)||k.rect.w<=0||k.rect.h<=0||k.rect.x<0||k.rect.y<0)return fail(e,"Naming key rejected");panel.push_back(k);}if(panel.empty()||panel.front().value.empty())return fail(e,"Naming initial key rejected");for(const auto&key:panel)for(auto next:key.neighbors)if(next>=panel.size()||(!key.value.empty()&&panel[next].value.empty()))return fail(e,"Naming navigation target rejected");d.panels.push_back(panel);}
 for(auto&v:d.actor_position)v=r.real();
 for(auto&v:d.shadow_position)v=r.real();
 d.actor=r.animation(r.number());
 for(auto&v:d.arrow_offset)v=r.real();
 auto length=r.number();d.arrow_move=r.number();d.arrow=r.animation(length);
 count=r.count(4096);std::set<std::pair<uint32_t,uint32_t>>ids;for(uint32_t i=0;i<count;++i){NamingBattleText t;t.battle_id=r.u32();t.index=r.u32();t.key=r.text();t.source_text=r.text();t.expected=r.text();t.parts=r.texts();if(!t.battle_id||!ids.insert({t.battle_id,t.index}).second||t.parts.empty()||t.parts.size()>2||!ascii(t.expected))return fail(e,"Naming battle text binding rejected");for(const auto&s:t.parts)if(!ascii(s))return fail(e,"Naming battle template glyph rejected");d.battle_texts_.push_back(t);}
 count=r.count(32);std::set<std::string>targets;uint32_t foods=0;
 for(uint32_t i=0;i<count;++i){NamingField f;f.kind=r.u32();f.maximum=r.u32();f.resource=r.u32();f.target=r.text();f.initial=r.text();f.prompt=r.text();f.defaults=r.texts();const auto shadow=r.u32();f.shadow=shadow!=0;for(auto&v:f.actor_position)v=r.real();for(auto&v:f.shadow_position)v=r.real();f.actor=r.animation(r.number());
  if(f.kind>1||!f.maximum||f.maximum>128||f.resource>=d.resources.size()||f.target.empty()||!targets.insert(f.target).second||!ascii(f.prompt)||f.defaults.empty()||f.defaults.size()>32||shadow>1)return fail(e,"Naming field metadata rejected");
  if(f.kind==1){++foods;if(i+1!=count)return fail(e,"Naming food must finish supported fields");}
  for(const auto&k:f.actor.keys)if(k.frame>=d.resources[f.resource].columns*d.resources[f.resource].rows)return fail(e,"Naming field actor frame rejected");
  d.fields.push_back(std::move(f));
 }
 auto&b=d.presentation;if(r.u32()!=1)return fail(e,"Naming presentation tail schema rejected");
 b.source_width=r.u32();b.source_height=r.u32();b.box=r.u32();b.cursor=r.u32();b.actor=r.u32();b.shadow=r.u32();
 for(auto&v:b.layouts)v=r.u32();
 for(auto&v:b.texts)v=r.u32();
 for(auto&v:b.sounds)v=r.u32();
 count=r.count(8);for(uint32_t i=0;i<count;++i)b.keyboard.push_back(r.u32());for(auto&v:b.field_bevel)v=r.real();
 auto unique_refs=[](const auto&values,size_t limit){std::set<uint32_t>ids;for(auto v:values)if(v>=limit||!ids.insert(v).second)return false;return true;};
 if(!b.source_width||!b.source_height||b.source_width>1024||b.source_height>1024||!unique_refs(std::array<uint32_t,4>{{b.box,b.cursor,b.actor,b.shadow}},d.resources.size())||!unique_refs(b.layouts,d.layouts.size())||!unique_refs(b.texts,d.texts.size())||!unique_refs(b.sounds,d.sounds.size())||b.keyboard.size()!=d.panels.size()||!unique_refs(b.keyboard,d.resources.size()))return fail(e,"Naming presentation reference rejected");
 for(auto v:b.field_bevel)if(std::abs(v)>64)return fail(e,"Naming field bevel rejected");
 std::set<std::string>paths;for(const auto&resource:d.resources)if(!paths.insert(resource.path).second)return fail(e,"Duplicate naming resource path");
 for(size_t i=0;i<d.panels.size();++i){const auto&resource=d.resources[b.keyboard[i]];if(resource.width!=b.source_width||resource.height!=b.source_height||resource.columns!=1||resource.rows!=1)return fail(e,"Naming keyboard resource extent rejected");for(const auto&key:d.panels[i])if(key.rect.x+key.rect.w>b.source_width||key.rect.y+key.rect.h>b.source_height)return fail(e,"Naming key exceeds source viewport");}
 if(d.texts.empty()||d.fields.size()<2||foods!=1||d.fields.front().target!=d.target||d.fields.front().maximum!=d.maximum||d.fields.front().initial!=d.initial||d.fields.front().defaults!=d.defaults||d.fields.front().prompt!=d.texts[b.texts[0]])return fail(e,"Naming startup field scope rejected");
 for(uint32_t i=0;i<d.fields.size();++i){const auto&f=d.fields[i];if(!f.initial.empty()&&!d.supported_field(i,f.initial))return fail(e,"Naming field initial value rejected");for(const auto&v:f.defaults)if(!d.supported_field(i,v))return fail(e,"Naming field default rejected");}
 if(!r.ok||r.n||!d.maximum||d.maximum>128||d.target.empty()||d.layouts.empty()||d.resources.empty()||d.panels.empty()||d.texts.empty()||d.sounds.empty()||d.defaults.empty()||d.defaults.size()>32||d.blacklist.empty()||d.cancel_delay<0||d.cancel_delay>10||d.error_duration<=0||d.error_duration>10||d.font_height<=0||d.font_height>64||d.arrow_move<=0||d.arrow_move>1||d.battle_texts_.empty())return fail(e,"Naming malformed payload or scope rejected");
 for(const auto&panel:d.panels)if(panel.size()!=d.panels.front().size())return fail(e,"Naming keyboard panel topology differs");
 for(const auto&l:d.layouts)if(l.w<0||l.h<0)return fail(e,"Naming layout rejected");
 for(auto margin:d.patch)if(margin>64)return fail(e,"Naming patch rejected");
 for(const auto&s:d.texts)if(!ascii(s))return fail(e,"Naming prompt glyph rejected");
 for(const auto&s:d.sounds)if(!safe_path(s))return fail(e,"Naming sound path rejected");
 for(const auto&s:d.defaults)if(!d.supported_name(s))return fail(e,"Naming default exceeds input scope");
 if(!d.initial.empty()&&!d.supported_name(d.initial))return fail(e,"Naming initial value rejected");
 for(const auto&k:d.actor.keys)if(k.frame>=d.resources[b.actor].columns*d.resources[b.actor].rows)return fail(e,"Naming actor frame rejected");
 for(const auto&k:d.arrow.keys)if(k.frame>=d.resources[b.cursor].columns*d.resources[b.cursor].rows)return fail(e,"Naming cursor frame rejected");
 for(const auto&t:d.battle_texts_){std::string expected=t.parts[0];if(t.parts.size()==2)expected+=d.defaults[0]+t.parts[1];if(expected!=t.expected)return fail(e,"Naming source text reconstruction rejected");}
 d.valid_=true;*this=std::move(d);e.clear();return true;
}
bool NewGameSetupData::load_file(const char*path,std::string&e){
 if(!path)return fail(e,"Missing naming path");
 FILE*f=std::fopen(path,"rb");if(!f)return fail(e,"Cannot open naming pack");std::vector<uint8_t>b;uint8_t block[4096];bool good=true;while(true){auto n=std::fread(block,1,sizeof(block),f);if(b.size()+n>1024*1024){good=false;break;}b.insert(b.end(),block,block+n);if(n<sizeof(block)){good=!std::ferror(f);break;}}if(std::fclose(f))good=false;return good?load(b.data(),b.size(),e):fail(e,"Naming bounded read failed");
}
bool NewGameSetupData::supported_name(std::string_view name)const{return supported_field(0,name);}
bool NewGameSetupData::supported_field(uint32_t index,std::string_view name)const{
 if(index>=fields.size()||name.empty()||name.size()>fields[index].maximum||!ascii(name))return false;
 for(char c:name){bool found=false;for(const auto&p:panels)for(const auto&k:p)if(k.kind==0&&k.value==std::string(1,c))found=true;if(!found)return false;}return true;
}
bool NewGameSetupData::battle_text(RoundView view,uint32_t index,std::string_view name,std::string&out)const{
 if(!valid_||!view.valid()||index>=view.count(RoundSection::Texts)||name.empty()||name.size()>maximum||!ascii(name))return false;
 const auto row=view.text(index);for(const auto&b:battle_texts_)if(b.battle_id==view.binding().battle_id&&b.index==index){if(view.string(row.key)!=b.key||view.string(row.source_text)!=b.source_text||view.string(row.text)!=b.expected)return false;std::string result=b.parts.front();if(b.parts.size()==2){result+=name;result+=b.parts[1];}out=std::move(result);return true;}return false;
}
bool stage_new_game_startup(const NativeSessionData&data,const RestoreData&restore,
 SourceRandom&random,std::vector<uint32_t>&ledger,const LoadRngClockProvider&clock,
 SessionSnapshot&output,std::string&e){
 if(!data.valid()||!restore.valid()||data.startup_characters().empty())return fail(e,"Startup requires checked roster and inventory order");
 SessionSnapshot next=data.defaults();next.characters=data.startup_characters();
 std::vector<LoadInventoryAllocation>allocations;std::vector<std::pair<uint32_t,std::vector<SessionItem>*>>inventories;
 std::set<std::string>characters;bool keys=false,storage=false;
 for(const auto&row:restore.inventory_load_order()){
  std::vector<SessionItem>*items=nullptr;
  if(row.rebuilds_inventory){
   if(row.kind==RestoreInventoryKind::KeyItems){if(keys)return fail(e,"Duplicate startup key inventory");keys=true;items=&next.key_items;}
   else if(row.kind==RestoreInventoryKind::Storage){if(storage)return fail(e,"Duplicate startup storage inventory");storage=true;items=&next.storage;}
   else {auto found=std::find_if(next.characters.begin(),next.characters.end(),[&](const auto&c){return c.character_id==row.character_id;});if(found==next.characters.end()||!characters.insert(row.character_id).second)return fail(e,"Startup inventory character unavailable");items=&found->inventory;}
   if(items->size()!=row.projected_items.size())return fail(e,"Startup inventory source count disagrees");
   for(size_t i=0;i<items->size();++i){const auto&a=(*items)[i];const auto&b=row.projected_items[i];if(a.item_id!=b.item_id||a.equipped!=b.equipped||a.doses!=b.doses)return fail(e,"Startup inventory source item disagrees");}
   inventories.push_back({row.order_id,items});
  }else if(!row.projected_items.empty())return fail(e,"Unsupported startup non-party inventory");
  allocations.push_back({row.order_id,items?uint32_t(items->size()):0u});
 }
 if(!keys||!storage||characters.size()!=next.characters.size())return fail(e,"Startup inventory source order is incomplete");
 auto next_random=random;auto next_ledger=ledger;std::vector<LoadUidAllocation>trace;
 if(!apply_load_uid_allocations(next_random,next_ledger,allocations,clock,e,&trace))return false;
 for(const auto&entry:trace){auto found=std::find_if(inventories.begin(),inventories.end(),[&](const auto&row){return row.first==entry.order_id;});if(found==inventories.end()||entry.item_index>=found->second->size())return fail(e,"Startup allocation trace disagrees");(*found->second)[entry.item_index].uid=entry.generated_uid;}
 if(!validate_session_snapshot(next,e))return false;
 output=std::move(next);random=next_random;ledger=std::move(next_ledger);e.clear();return true;
}
bool NewGameSetup::open(const NewGameSetupData&d,const StartupSettingsData&settings,std::string&e){if(!d.valid()||!settings.valid()||d.fields.size()!=settings.confirmation_fields.size())return fail(e,"Naming requires checked data/settings");const auto*locale=locale_;*this=NewGameSetup{};locale_=locale;data_=&d;settings_data_=&settings;settings_=settings.defaults();phase_=NamingPhase::Editing;for(const auto&f:d.fields)values_.push_back(f.initial);enter(0);e.clear();return true;}
void NewGameSetup::enter(uint32_t index){field_=index;value_=values_[index];selected_=0;error_prompt_=0;error_remaining_=0;const auto&r=data_->panels[panel_][0].rect;cursor_from_=cursor_to_={{r.x,r.y}};cursor_time_=0;}
void NewGameSetup::previous(){values_[field_]=value_;if(field_)enter(field_-1);else if(elapsed_>=data_->cancel_delay)phase_=NamingPhase::Cancelled;}

std::string NewGameSetup::normalized(std::string_view s)const{size_t first=0,last=s.size();while(first<last&&static_cast<unsigned char>(s[first])<=32)++first;while(last>first&&static_cast<unsigned char>(s[last-1])<=32)--last;std::string v(s.substr(first,last-first));for(char&c:v)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return v;}
void NewGameSetup::sound(uint32_t index){if(index<data_->presentation.sounds.size())sounds_.push_back(data_->sounds[data_->presentation.sounds[index]]);}
std::array<float,2>NewGameSetup::cursor()const{if(!data_)return{};const double t=std::clamp(cursor_time_/data_->arrow_move,0.,1.),ease=1-std::pow(1-t,4);return {{float(cursor_from_[0]+(cursor_to_[0]-cursor_from_[0])*ease),float(cursor_from_[1]+(cursor_to_[1]-cursor_from_[1])*ease)}};}
void NewGameSetup::select(uint32_t index){if(index>=data_->panels[panel_].size()||data_->panels[panel_][index].value.empty())return;cursor_from_=cursor();selected_=index;const auto&r=data_->panels[panel_][index].rect;cursor_to_={{r.x,r.y}};cursor_time_=0;sound(0);}
void NewGameSetup::erase(){if(!value_.empty()){value_.pop_back();sound(2);}else previous();}
void NewGameSetup::accept(){if(value_.empty())return;const auto key=normalized(value_);error_prompt_=0;for(const auto&blocked:data_->blacklist)if(key==blocked)error_prompt_=1;for(uint32_t i=0;i<values_.size();++i)if(i!=field_&&key==normalized(values_[i]))error_prompt_=key.empty()?1:2;if(error_prompt_){if(error_remaining_<=0)error_remaining_=data_->error_duration;return;}if(!data_->supported_field(field_,value_))return;values_[field_]=value_;if(field_+1<data_->fields.size())enter(field_+1);else enter_settings();}

bool NewGameSetup::step(double dt,const NamingInput&input,std::string&e){
 if(!locale_error_.empty()){e=locale_error_;return false;}
 if(!data_||!data_->valid()||!std::isfinite(dt)||dt<0||dt>10||input.x<-1||input.x>1||input.y<-1||input.y>1)return fail(e,"Naming step rejected");
 if(!active()){e.clear();return true;}elapsed_+=dt;cursor_time_+=dt;if(error_remaining_>0){error_remaining_-=dt;if(error_remaining_<=0)error_prompt_=0;}
 if(phase_!=NamingPhase::Editing){step_settings(dt,input);e.clear();return true;}
 if(input.cancel){erase();e.clear();return true;}
 if(input.previous&&field_){previous();e.clear();return true;}
 if(input.toggle_panel){panel_=(panel_+1)%uint32_t(data_->panels.size());if(data_->panels[panel_][selected_].value.empty())select(0);}
 if(input.command){const auto&p=data_->panels[panel_];auto found=std::find_if(p.begin(),p.end(),[](const NamingKey&k){return k.kind==1;});if(found!=p.end())select(uint32_t(found-p.begin()));}
 if(input.x||input.y){const uint32_t direction=input.y?(input.y<0?2:3):(input.x<0?0:1);const auto next=data_->panels[panel_][selected_].neighbors[direction];if(next!=selected_)select(next);}
 if(input.next){accept();e.clear();return true;}
 if(input.accept){const auto&key=data_->panels[panel_][selected_];switch(key.kind){case 0:if(!key.value.empty()&&value_.size()<field().maximum){value_+=key.value;sound(1);}break;case 1:{size_t next=0;for(size_t i=0;i<field().defaults.size();++i)if(value_==field().defaults[i]){next=(i+1)%field().defaults.size();break;}value_=field().defaults[next];sound(1);break;}case 2:erase();break;case 3:accept();break;}}
 e.clear();return true;
}
void NewGameSetup::enter_settings(){phase_=NamingPhase::Settings;settings_row_=0;option_=uint32_t(settings_data_->speed_index(settings_.text_speed));reset_preview();}
void NewGameSetup::restart(){phase_=NamingPhase::Editing;enter(0);}
void NewGameSetup::reset_preview(){preview_time_=0;preview_visible_=0;}
std::string NewGameSetup::preview_flavor()const{return phase_==NamingPhase::SettingOption&&settings_row_==1?settings_data_->flavors[option_]:settings_.menu_flavor;}
void NewGameSetup::step_settings(double dt,const NamingInput&in){
 const auto&d=*settings_data_;
 if(phase_==NamingPhase::Confirmation){
  if(in.cancel){restart();sound(2);return;}
  if(in.y){confirmation_=(confirmation_+uint32_t(in.y+2))%2;sound(0);}
  if(in.accept){if(confirmation_==0)phase_=NamingPhase::Accepted;else restart();}
  return;
 }
 if(phase_==NamingPhase::SettingOption){
  if(in.cancel){phase_=NamingPhase::Settings;reset_preview();sound(2);return;}
  const uint32_t count=uint32_t(d.panels[settings_row_].labels.size());
  if(in.y){option_=(option_+count+in.y)%count;reset_preview();sound(0);}
  if(in.accept){if(settings_row_==0)settings_.text_speed=d.speeds[option_];else if(settings_row_==1)settings_.menu_flavor=d.flavors[option_];else settings_.button_prompts=d.prompts[option_];phase_=NamingPhase::Settings;sound(1);return;}
  if(settings_row_==0){const auto&labels=d.panels[0].labels;bool worthwhile=true;size_t length=0;for(size_t i=0;i<labels.size();++i){size_t count=0;if(!encore::utf8_count(localized("settings.panel/0/"+std::to_string(i),labels[i].text),count))return;worthwhile&=count>d.preview_minimum_characters;if(i==option_)length=count;}if(!worthwhile)preview_visible_=uint32_t(length);else{preview_time_+=dt;if(preview_time_>d.speeds[option_]){preview_time_=0;preview_visible_=std::min(preview_visible_+1,uint32_t(length));}}}
  return;
 }
 if(in.cancel){phase_=NamingPhase::Editing;enter(uint32_t(data_->fields.size()-1));sound(2);return;}
 if(in.y){settings_row_=(settings_row_+uint32_t(d.rows.size())+in.y)%uint32_t(d.rows.size());sound(0);}
 if(in.accept){
  if(settings_row_+1==d.rows.size()){phase_=NamingPhase::Confirmation;confirmation_=0;return;}
  option_=uint32_t(settings_row_==0?d.speed_index(settings_.text_speed):settings_row_==1?d.flavor_index(settings_.menu_flavor):d.prompt_index(settings_.button_prompts));phase_=NamingPhase::SettingOption;reset_preview();sound(1);
 }
}
std::string NewGameSetup::localized(std::string_view id,std::string_view expected)const{if(!locale_)return std::string(expected);std::string out,error;if(!locale_->catalog()->bound(id,expected,locale_->code(),out,error)){locale_error_=error+": "+std::string(id);return {};}return out;}
std::string NewGameSetup::prompt()const{if(!data_)return {};return error_prompt_?localized("naming.text/"+std::to_string(data_->presentation.texts[error_prompt_]),data_->texts[data_->presentation.texts[error_prompt_]]):localized("naming.prompt/"+std::to_string(field_),field().prompt);}
std::string NewGameSetup::dotted_name()const{if(!data_)return{};auto s=value_;if(s.size()<field().maximum){s+=data_->texts[data_->presentation.texts[3]];while(s.size()<field().maximum)s+=data_->texts[data_->presentation.texts[4]];}return s;}
bool NewGameSetup::apply(SessionSnapshot&s,const NativeSessionData&session,std::string&e)const{
 if(phase_!=NamingPhase::Accepted||!data_||!session.valid()||s.characters.size()!=session.startup_characters().size()||s.party!=session.defaults().party||s.characters.empty()||s.characters[0].character_id!=data_->target||values_.size()!=data_->fields.size())return fail(e,"Naming commit outside fresh source startup target");
 auto next=s;std::set<std::string>named;
 for(uint32_t i=0;i<data_->fields.size();++i){const auto&f=data_->fields[i];if(!data_->supported_field(i,values_[i]))return fail(e,"Naming commit contains invalid field");if(f.kind==1){next.favorite_food=values_[i];continue;}auto it=std::find_if(next.characters.begin(),next.characters.end(),[&](const auto&c){return c.character_id==f.target;});if(it==next.characters.end()||!named.insert(f.target).second)return fail(e,"Naming startup identity unavailable");it->nickname=values_[i];}
 next.settings=settings_;
 if(!settings_data_||!settings_data_->supports(next.settings)||!session.supports_settings(next.settings))return fail(e,"Startup settings outside source-supported choices");
 if(named.size()!=next.characters.size()||!validate_session_snapshot(next,e))return fail(e,"Naming startup roster is incomplete");
 s=std::move(next);e.clear();return true;
}
}
