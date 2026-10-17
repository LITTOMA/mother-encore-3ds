#include "encore/field_story.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include "encore/utf8.hpp"
#include "encore/dialogue.hpp"
#include "encore/new_game_setup.hpp"
#include <cstring>
#include <cmath>
#include <set>
namespace encore::upstream {namespace {
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
FieldText ref(const uint8_t*p){return {u32(p),u32(p+4)};}
constexpr uint32_t strides[]={1,24,36,40,12};
float f32(const uint8_t*p){const auto bits=u32(p);float value;std::memcpy(&value,&bits,4);return value;}
bool path(std::string_view value){if(value.empty()||value.size()>512||value.front()=='/'||value.find(':')!=value.npos||value.find('\\')!=value.npos)return false;
 size_t at=0;while(at<=value.size()){auto end=value.find('/',at);if(end==value.npos)end=value.size();const auto part=value.substr(at,end-at);if(part.empty()||part=="."||part=="..")return false;if(end==value.size())break;at=end+1;}return true;}
}
bool format_field_story_text(std::string_view input,const NewGameSetupData&names,const SessionSnapshot&snapshot,const LocaleSelection&locale,std::string&out,std::string&error){
 auto reject=[&]{error="Unmapped field character text binding";return false;};
 if(!names.valid()||!locale.catalog()||input.size()>16384)return reject();std::string result;size_t at=0;
 while(at<input.size()){const auto open=input.find('[',at);if(open==input.npos){result.append(input.substr(at));break;}result.append(input.substr(at,open-at));const auto close=input.find(']',open);if(close==input.npos)return reject();
  std::string id(input.substr(open+1,close-open-1));for(char&c:id)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
  bool known=false;for(const auto&field:names.fields)known|=field.kind==0&&field.target==id;if(!known)return reject();
  std::string value;bool found=false;for(const auto&character:snapshot.characters)if(character.character_id==id){if(found)return reject();found=true;value=character.nickname;}
  if(value.empty()){const auto*binding=locale.catalog()->binding("field.character/"+id);if(!binding||!locale.catalog()->bound(binding->identity,binding->expected,locale.code(),value,error))return reject();}
  if(value.empty()||value.size()>128)return reject();result+=value;if(result.size()>65536)return reject();at=close+1;
 }
 out=std::move(result);error.clear();return true;
}
uint32_t FieldStoryView::count(uint32_t s)const{return bytes_&&s>=1&&s<=5?u32(bytes_+64+(s-1)*16+8):0;}
const uint8_t*FieldStoryView::record(uint32_t s,uint32_t i)const{return i<count(s)?bytes_+u32(bytes_+64+(s-1)*16+4)+i*strides[s-1]:nullptr;}
uint32_t FieldStoryView::scene_id()const{return bytes_?u32(bytes_+24):0;}
uint32_t FieldStoryView::programme_count()const{return count(2);}uint32_t FieldStoryView::text_count()const{return count(3);}uint32_t FieldStoryView::encounter_count()const{return count(4);}uint32_t FieldStoryView::override_count()const{return count(5);}
std::string_view FieldStoryView::text(FieldText t)const{return bytes_&&t.length&&t.offset<=count(1)&&t.length<=count(1)-t.offset?std::string_view(reinterpret_cast<const char*>(record(1,0)+t.offset),t.length):std::string_view{};}
FieldStoryProgramme FieldStoryView::programme(uint32_t i)const{const auto*p=record(2,i);return p?FieldStoryProgramme{u32(p),ref(p+4),ref(p+12),u32(p+20)}:FieldStoryProgramme{};}
FieldStoryText FieldStoryView::text_record(uint32_t i)const{const auto*p=record(3,i);return p?FieldStoryText{u32(p),ref(p+4),ref(p+12),ref(p+20),ref(p+28)}:FieldStoryText{};}
FieldStoryText FieldStoryView::dialogue(uint32_t id,std::string_view locale)const{for(uint32_t i=0;i<count(3);++i){const auto*p=record(3,i);if(u32(p)==id&&text(ref(p+4))==locale)return {id,ref(p+4),ref(p+12),ref(p+20),ref(p+28)};}return {};}
FieldStoryEncounter FieldStoryView::encounter(uint32_t id)const{for(uint32_t i=0;i<count(4);++i){const auto*p=record(4,i);if(u32(p)==id)return {id,u32(p+4),ref(p+8),ref(p+16),u32(p+24),u32(p+28),f32(p+32),u32(p+36)};}return {};}
FieldStoryEncounter FieldStoryView::encounter_for_programme(uint32_t programme)const{for(uint32_t i=0;i<count(4);++i){const auto row=encounter(u32(record(4,i)));if(row.programme==programme)return row;}return {};}
FieldBasicOverride FieldStoryView::basic_override(uint32_t id)const{for(uint32_t i=0;i<count(5);++i){const auto*p=record(5,i);if(u32(p)==id)return {id,ref(p+4)};}return {};}
FieldStoryView FieldStoryData::view()const{FieldStoryView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
bool FieldStoryData::load_file(const char*file,std::string&error){std::vector<uint8_t>b;return encore::read_file(file,b,128*1024,error)&&load(b.data(),b.size(),error);}
bool FieldStoryData::load(const uint8_t*p,size_t n,std::string&error){
 auto fail=[&](const char*why){error=why;return false;};
 if(!p||n<144||n>128*1024)return fail("Field story size rejected");
 if(std::memcmp(p,"ENCFST01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||!u32(p+24)||u32(p+28)||u32(p+52)!=5||u32(p+56)||u32(p+60))return fail("Field story schema/capability/reserved rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail("Field story source pin rejected");}
 if(encore::crc32(p+32,n-32)!=u32(p+16))return fail("Field story checksum rejected");
 size_t cursor=144;
 for(uint32_t s=1;s<=5;++s){const auto*d=p+64+(s-1)*16;const auto offset=u32(d+4),count=u32(d+8),stride=u32(d+12);
  if(u32(d)!=s||stride!=strides[s-1])return fail("Field story directory rejected");
  if(!count){if(offset)return fail("Field story empty section rejected");continue;}
  if(offset%4||offset<cursor||offset>n||count>(n-offset)/stride)return fail("Field story span rejected");
  for(size_t i=cursor;i<offset;++i)if(p[i])return fail("Field story padding rejected");cursor=offset+count*stride;
 }
 if(cursor!=n)return fail("Field story trailing bytes rejected");
 FieldStoryView v;v.bytes_=p;v.size_=n;
 if(!v.count(1)||v.count(1)>64*1024||!v.count(2)||v.count(2)>64||!v.count(3)||v.count(3)>256||!v.count(4)||v.count(4)>32||v.count(5)>64)return fail("Field story capacity rejected");
 auto valid_text=[&](FieldText t,bool empty,size_t maximum){if(!t.length)return empty&&!t.offset;if(t.offset>v.count(1)||t.length>v.count(1)-t.offset||t.length>maximum)return false;const auto value=v.text(t);size_t count=0;return value.find('\0')==value.npos&&encore::utf8_count(value,count);};
 std::set<uint32_t>programmes,battles,overrides;std::set<std::string_view>boundaries,sources;std::set<std::pair<uint32_t,std::string_view>>texts;
 std::set<std::string_view>locales;std::set<uint32_t>text_ids;
 for(uint32_t i=0;i<v.count(2);++i){const auto row=v.programme(i);if(row.programme>=1024||row.flags||!programmes.insert(row.programme).second||!valid_text(row.boundary,false,512)||!valid_text(row.source,false,512)||!path(v.text(row.boundary))||!path(v.text(row.source))||!boundaries.insert(v.text(row.boundary)).second||!sources.insert(v.text(row.source)).second)return fail("Field story programme rejected");}
 for(uint32_t i=0;i<v.count(3);++i){const auto*r=v.record(3,i);const auto id=u32(r);const auto locale=ref(r+4),speaker=ref(r+12),body=ref(r+20),voice=ref(r+28);
  if(!id||!valid_text(locale,false,32)||!valid_text(speaker,true,256)||!valid_text(body,false,8192)||!valid_text(voice,true,512)||(!v.text(voice).empty()&&!path(v.text(voice)))||!texts.insert({id,v.text(locale)}).second)return fail("Field story dialogue rejected");
  for(char c:v.text(locale))if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return fail("Field story locale rejected");
  locales.insert(v.text(locale));text_ids.insert(id);
 }
 for(auto id:text_ids)for(auto locale:locales)if(!texts.count({id,locale}))return fail("Field story locale coverage rejected");
 for(uint32_t i=0;i<v.count(4);++i){const auto row=v.encounter(u32(v.record(4,i)));if(!row.battle||!battles.insert(row.battle).second||row.actor==0||row.actor>=64||row.basic_slot>=16||!programmes.count(row.programme)||!std::isfinite(row.return_music_seconds)||row.return_music_seconds<=0||row.return_music_seconds>60||row.return_music_curve<1||row.return_music_curve>3||!valid_text(row.basic,false,512)||!valid_text(row.music,false,512)||!path(v.text(row.basic))||!path(v.text(row.music)))return fail("Field story encounter rejected");}
 for(uint32_t i=0;i<v.count(5);++i){const auto*r=v.record(5,i);const auto id=u32(r);const auto value=ref(r+4);if(!id||!overrides.insert(id).second||!valid_text(value,false,512)||!path(v.text(value)))return fail("Field basic action override rejected");}
 std::vector<uint8_t>owned(p,p+n);bytes_=std::move(owned);error.clear();return true;
}
bool FieldStoryView::bind(RoomView room,FieldMapView map,ItemView items,std::string&error)const{
 auto fail=[&](const char*why){error=why;return false;};
 if(!valid()||!room.valid()||!map.valid()||!items.valid()||scene_id()!=room.scene().stable_id||scene_id()!=map.scene_id())return fail("Field story scene/item binding rejected");
 for(uint32_t i=0;i<programme_count();++i){const auto row=programme(i);if(row.programme>=room.program_count()||room.string(room.program(row.programme).source_path_string)!=text(row.source))return fail("Field story room programme differs");
  unsigned found=0;for(uint32_t b=0;b<map.count(FieldMapSection::Boundaries);++b){const auto boundary=map.boundary(b);if(map.text(boundary.path)==text(row.boundary)){if(boundary.kind!=2)return fail("Field story boundary is not a source cutscene");++found;}}
  if(found!=1)return fail("Field story source boundary is missing/ambiguous");
  const auto program=room.program(row.programme);
  for(uint32_t k=0;k<program.command_count;++k){const auto command=room.command(program.first_command+k);if(command.opcode==uint16_t(DialogueActionKind::ShowDialogue)){
    bool present=false;for(uint32_t t=0;t<text_count();++t)present|=u32(record(3,t))==command.target_index;if(!present)return fail("Field story programme has an unbound dialogue");}}
 }
 for(uint32_t i=0;i<encounter_count();++i){const auto row=encounter(u32(record(4,i)));unsigned found=0;
  for(uint32_t j=0;j<room.battle_count();++j){const auto battle=room.battle(j);if(battle.stable_id==row.battle){if(battle.actor_instance_index!=row.actor)return fail("Field story encounter actor differs");++found;}}
  if(found!=1)return fail("Field story room encounter missing/ambiguous");
  bool queued=false,requested=false;const auto program=room.program(row.programme);
  for(uint32_t k=0;k<program.command_count;++k){const auto command=room.command(program.first_command+k);if(command.opcode!=uint16_t(DialogueActionKind::QueueBattle)&&command.opcode!=uint16_t(DialogueActionKind::RequestBattle))continue;
   if(command.target_index>=room.battle_count()||room.battle(command.target_index).stable_id!=row.battle||command.actor_index!=row.actor)return fail("Field story battle command differs");
   if(command.opcode==uint16_t(DialogueActionKind::QueueBattle))queued=true;else requested=true;
  }
  if(!queued||!requested)return fail("Field encounter programme lacks its queue/request lifecycle");
 }
 for(uint32_t i=0;i<override_count();++i){const auto row=basic_override(u32(record(5,i)));bool found=false;for(uint32_t j=0;j<items.count(ItemSection::Definitions);++j){const auto definition=items.definition(j);if(definition.id!=row.definition)continue;if(!(definition.flags&uint32_t(ItemDefinitionFlag::Equipment))||!definition.can_use)return fail("Field basic override is not admitted equipment");for(uint32_t b=0;b<encounter_count();++b)if(definition.equipment_slot!=encounter(u32(record(4,b))).basic_slot)return fail("Field basic slot differs from item binding");found=true;}if(!found)return fail("Field basic override definition is absent");}
 error.clear();return true;
}
}
