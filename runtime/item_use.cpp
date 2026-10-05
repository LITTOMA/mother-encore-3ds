#include "encore/item_use.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>

namespace encore::upstream {namespace {
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint16_t u16(const uint8_t*p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
float real(const uint8_t*p){float v;auto n=u32(p);std::memcpy(&v,&n,4);return v;}
bool reject(std::string&e,const char*m){e=m;return false;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~uint32_t(0);for(size_t i=0;i<n;++i){c^=i>=16&&i<20?0:p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
bool plain(std::string_view text,bool target=false){size_t i=0;uint32_t cp;while(i<text.size())if(!encore::utf8_next(text,i,cp)||cp<32||cp==127||cp=='['||cp==']'||(!target&&(cp=='{'||cp=='}')))return false;return true;}
bool identifier(std::string_view s){return !s.empty()&&s.size()<=128&&std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';});}
bool safe_path(std::string_view s){if(s.empty()||s.size()>512||s.front()=='/'||s.back()=='/'||s.find_first_of("\\:")!=s.npos||!plain(s))return false;size_t i=0;while(i<s.size()){auto j=s.find('/',i);if(j==s.npos)j=s.size();auto part=s.substr(i,j-i);if(part.empty()||part=="."||part=="..")return false;i=j+1;}return true;}
bool pattern(std::string_view s){auto at=s.find("{target}");if(at==s.npos||s.find("{target}",at+8)!=s.npos||!plain(s,true))return false;std::string rest(s);rest.erase(at,8);return rest.find_first_of("{}%") == rest.npos;}
std::string expand(std::string s,std::string_view nickname){auto at=s.find("{target}");s.replace(at,8,nickname);return s;}
}

const ItemUseRule*ItemUseData::rule(uint32_t id)const{for(const auto&r:rules_)if(r.definition==id)return &r;return nullptr;}
const ItemUseLocale*ItemUseData::locale(std::string_view code)const{for(const auto&l:locales_)if(l.code==code)return &l;return nullptr;}
BattleValue ItemUseData::layout(ItemUseLayoutRole role)const{for(const auto&l:layouts_)if(l.role==role)return l.rect;return {};}
BattleValue ItemUseData::layout_color(ItemUseLayoutRole role)const{for(const auto&l:layouts_)if(l.role==role)return l.color;return {};}
BattleValue ItemUseData::parameter(ItemUseParameter id)const{auto i=uint32_t(id);return i&&i<=parameters_.size()?parameters_[i-1]:BattleValue{};}
std::string_view ItemUseData::sound(FieldItemUseSound id)const{auto i=uint32_t(id);return i&&i<=sounds_.size()?std::string_view(sounds_[i-1]):std::string_view{};}
bool ItemUseData::bind_items(ItemView items,std::string&e)const{
 if(!valid()||!items.valid()||items.reviewed_commit()!=commit_)return reject(e,"Item consumption content/pin binding rejected");
 for(const auto&r:rules_){if(r.definition>=items.count(ItemSection::Definitions))return reject(e,"Item consumption definition binding rejected");const auto d=items.definition(r.definition);
  if(items.string(d.source)!=r.source||d.flags&uint32_t(ItemDefinitionFlag::Equipment)||d.heal_hp||d.heal_pp||d.max_hp_boost||d.max_pp_boost)return reject(e,"Item consumption source/effect binding rejected");
 }
 e.clear();return true;
}
bool ItemUseData::load(const uint8_t*p,size_t n,std::string&e){
 constexpr uint32_t header=192,sections=8;const uint32_t strides[]={1,40,20,4,20,36,20,8};
 if(!p||n<header||n>1024*1024||std::memcmp(p,"ENCIUSE1",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=sections||u32(p+24)!=1||u32(p+28)!=1||crc(p,n)!=u32(p+16))return reject(e,"Item consumption header/version/capability/CRC rejected");
 if(std::all_of(p+32,p+52,[](uint8_t v){return v==0;})||std::any_of(p+52,p+64,[](uint8_t v){return v!=0;}))return reject(e,"Item consumption pin/reserved rejected");
 uint32_t offsets[sections],counts[sections];size_t end=header;
 for(uint32_t i=0;i<sections;++i){auto d=p+64+i*16;offsets[i]=u32(d+4);counts[i]=u32(d+8);const uint64_t bytes=uint64_t(counts[i])*strides[i];
  if(u16(d)!=i+1||u16(d+2)!=strides[i]||!counts[i]||bytes!=u32(d+12)||offsets[i]!=(end+3)/4*4||offsets[i]>n||bytes>n-offsets[i]||std::any_of(p+end,p+offsets[i],[](uint8_t v){return v!=0;}))return reject(e,"Item consumption section directory rejected");end=offsets[i]+size_t(bytes);
 }
 if(end!=n||counts[0]>65536||counts[1]!=1||counts[2]>16||counts[3]>4||counts[4]>16||counts[5]!=7||counts[6]!=14||counts[7]!=7)return reject(e,"Item consumption section count/trailing data rejected");
 auto pool=p+offsets[0];if(pool[0]||pool[counts[0]-1])return reject(e,"Item consumption string pool rejected");
 std::set<uint32_t>starts;for(uint32_t i=0;i<counts[0];){starts.insert(i);auto j=i;while(j<counts[0]&&pool[j])++j;if(j==counts[0]||!plain({reinterpret_cast<const char*>(pool+i),size_t(j-i)},true))return reject(e,"Item consumption UTF-8/control rejected");i=j+1;}
 bool strings_ok=true;auto str=[&](uint32_t i){if(!starts.count(i)){strings_ok=false;return std::string{};}return std::string(reinterpret_cast<const char*>(pool+i));};
 ItemUseData candidate;const char*hex="0123456789abcdef";for(size_t i=32;i<52;++i){candidate.commit_+=hex[p[i]>>4];candidate.commit_+=hex[p[i]&15];}
 std::set<std::string>status_ids,target_ids,locale_ids,sources;std::set<uint32_t>definitions;
 for(uint32_t i=0;i<counts[2];++i){auto r=p+offsets[2]+i*strides[2];ItemUseStatusPolicy s{str(u32(r)),bool(u32(r+4)),bool(u32(r+8)),u32(r+12),u32(r+16)};
  if(!identifier(s.id)||!status_ids.insert(s.id).second||u32(r+4)>1||u32(r+8)>1||s.passive_healing||s.default_saved_turns||!s.refresh_hp_value||s.refresh_hp_value>INT32_MAX)return reject(e,"Item consumption unsupported status policy rejected");candidate.statuses_.push_back(std::move(s));
 }
 for(uint32_t i=0;i<counts[1];++i){auto r=p+offsets[1]+i*strides[1];ItemUseRule s;s.definition=u32(r);s.source=str(u32(r+4));s.status=str(u32(r+8));s.max_doses=u32(r+12);s.reusable=bool(u32(r+16));s.heal_message=str(u32(r+20));s.fail_message=str(u32(r+24));s.success_sound=str(u32(r+28));
  if(!definitions.insert(s.definition).second||!identifier(s.source)||!sources.insert(s.source).second||!status_ids.count(s.status)||!s.max_doses||s.max_doses>65535||u32(r+16)>1||!identifier(s.heal_message)||!identifier(s.fail_message)||s.heal_message==s.fail_message||!safe_path(s.success_sound)||u32(r+32)||u32(r+36))return reject(e,"Item consumption unsupported effect/rule rejected");candidate.rules_.push_back(std::move(s));
 }
 for(uint32_t i=0;i<counts[3];++i){auto id=str(u32(p+offsets[3]+i*4));if(!identifier(id)||!target_ids.insert(id).second)return reject(e,"Item consumption duplicate/invalid target rejected");candidate.targets_.push_back(std::move(id));}
 for(uint32_t i=0;i<counts[4];++i){auto r=p+offsets[4]+i*20;ItemUseLocale l{str(u32(r)),str(u32(r+4)),str(u32(r+8)),str(u32(r+12)),str(u32(r+16))};
  if(!identifier(l.code)||!locale_ids.insert(l.code).second||l.action.empty()||l.title.empty()||!plain(l.action)||!plain(l.title)||!pattern(l.heal)||!pattern(l.fail))return reject(e,"Item consumption locale/template rejected");candidate.locales_.push_back(std::move(l));
 }
 for(uint32_t i=0;i<counts[5];++i){auto r=p+offsets[5]+i*36;ItemUseLayout l{ItemUseLayoutRole(u32(r)),{real(r+4),real(r+8),real(r+12),real(r+16)},{real(r+20),real(r+24),real(r+28),real(r+32)}};
  if(u32(r)!=i+1||!std::isfinite(l.rect.x)||!std::isfinite(l.rect.y)||!std::isfinite(l.rect.z)||!std::isfinite(l.rect.w)||l.rect.x<0||l.rect.y<0||l.rect.z<=0||l.rect.w<=0||l.rect.x+l.rect.z>8192||l.rect.y+l.rect.w>8192)return reject(e,"Item consumption layout rejected");candidate.layouts_.push_back(l);
  for(auto c:{l.color.x,l.color.y,l.color.z,l.color.w})if(!std::isfinite(c)||c<0||c>1)return reject(e,"Item consumption layout color rejected");
 }
 for(uint32_t i=0;i<counts[6];++i){auto r=p+offsets[6]+i*20;BattleValue v{real(r+4),real(r+8),real(r+12),real(r+16)};
  if(u32(r)!=i+1||!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.w)||v.x<-8192||v.y<-8192||v.z<-8192||v.w<-8192||v.x>8192||v.y>8192||v.z>8192||v.w>8192)return reject(e,"Item consumption layout parameter rejected");candidate.parameters_.push_back(v);
 }
 const auto grid=candidate.parameter(ItemUseParameter::Grid);if(grid.x<1||grid.y<1||grid.x!=std::floor(grid.x)||grid.y!=std::floor(grid.y)||grid.x*grid.y>256||grid.z<1||grid.w<1||candidate.parameter(ItemUseParameter::TargetOrigin).z<1||!strings_ok)return reject(e,"Item consumption grid/string reference rejected");
 for(const auto id:{ItemUseParameter::GridOrigin,ItemUseParameter::TargetOrigin,ItemUseParameter::TextInset,ItemUseParameter::ActionOrigin}){const auto v=candidate.parameter(id);if(v.x<0||v.y<0||v.z<0||v.w<0)return reject(e,"Item consumption negative text placement rejected");}
 for(const auto id:{ItemUseParameter::LabelSize,ItemUseParameter::SourceViewport,ItemUseParameter::PlatformViewport}){const auto v=candidate.parameter(id);if(v.x<=0||v.y<=0||v.z||v.w)return reject(e,"Item consumption label/viewport parameter rejected");}
 const auto align=candidate.parameter(ItemUseParameter::MessageAlignment),patch=candidate.parameter(ItemUseParameter::MessagePatch),placement=candidate.parameter(ItemUseParameter::SubmenuPlacement);
 if(align.x<0||align.x>2||align.x!=std::floor(align.x)||align.y<0||align.y>2||align.y!=std::floor(align.y)||align.z!=1||align.w!=0||placement.x<=0||placement.y<=0||placement.z>=0||placement.w<=0)return reject(e,"Item consumption text mode/submenu placement rejected");
 for(auto v:{patch.x,patch.y,patch.z,patch.w})if(v<0||v!=std::floor(v))return reject(e,"Item consumption message patch rejected");
 for(uint32_t i=0;i<counts[7];++i){auto r=p+offsets[7]+i*8;auto source=str(u32(r+4));if(u32(r)!=i+1||(!source.empty()&&!safe_path(source)))return reject(e,"Item consumption sound binding rejected");candidate.sounds_.push_back(std::move(source));}
 if(!strings_ok||candidate.sound(FieldItemUseSound::Heal)!=candidate.rules_[0].success_sound)return reject(e,"Item consumption source heal sound mismatch");
 candidate.valid_=true;*this=std::move(candidate);e.clear();return true;
}
bool ItemUseData::load_file(const char*path,std::string&e){
 if(!path||!*path)return reject(e,"Item consumption path missing");FILE*f=std::fopen(path,"rb");if(!f)return reject(e,"Item consumption file unavailable");std::vector<uint8_t>b;uint8_t block[4096];bool ok=true;
 for(;;){const size_t n=std::fread(block,1,sizeof block,f);if(b.size()+n>1024*1024){ok=false;break;}b.insert(b.end(),block,block+n);if(n<sizeof block){ok=!std::ferror(f);break;}}if(std::fclose(f))ok=false;if(!ok)return reject(e,"Item consumption file read rejected");return load(b.data(),b.size(),e);
}

bool validate_item_use_snapshot(ItemUseView data,const FieldItemUseSnapshot&s,std::string&e){
 if(!data||!data->valid()||!data->bind_items(s.inventory.content(),e))return reject(e,"Item consumption missing checked snapshot/resources");
 if(s.owner.empty()||s.party.empty()||s.party.size()>data->targets().size()||s.maximum_hp.size()!=s.party.size())return reject(e,"Item consumption owner/party/maxhp rejected");
 std::set<std::string>ids;bool owner=false;
 for(size_t i=0;i<s.party.size();++i){const auto&c=s.party[i];if(std::find(data->targets().begin(),data->targets().end(),c.character_id)==data->targets().end()||!ids.insert(c.character_id).second||c.nickname.empty()||c.nickname.size()>256||!plain(c.nickname)||s.maximum_hp[i]<=0||s.maximum_hp[i]>INT32_MAX||c.hp<0||c.hp>s.maximum_hp[i])return reject(e,"Item consumption unknown party/nickname/HP rejected");owner|=c.character_id==s.owner;
  std::set<std::string>statuses;for(const auto&status:c.status){auto p=std::find_if(data->status_policies().begin(),data->status_policies().end(),[&](const ItemUseStatusPolicy&p){return p.id==status.status_id;});if(p==data->status_policies().end()||!statuses.insert(status.status_id).second||status.passive_healing_turns!=p->default_saved_turns)return reject(e,"Item consumption unsupported status/counter rejected");}
 }
 if(!owner)return reject(e,"Item consumption inventory owner outside party");
 InventoryState checked;if(!checked.restore(s.inventory.content(),s.inventory.instances(),e))return false;
 for(const auto&i:s.inventory.instances())if(const auto*r=data->rule(i.definition))if(i.equipped||i.doses>r->max_doses)return reject(e,"Item consumption instance doses/equipment rejected");
 e.clear();return true;
}
bool prepare_item_use(ItemUseView data,const FieldItemUseSnapshot&s,uint32_t uid,std::string_view target,FieldItemUseCandidate&out,std::string&e){
 if(!validate_item_use_snapshot(data,s,e))return false;
 const auto found=std::find_if(s.inventory.instances().begin(),s.inventory.instances().end(),[&](const ItemInstance&i){return i.id==uid;});if(found==s.inventory.instances().end())return reject(e,"Item consumption UID unavailable");
 const auto*r=data->rule(found->definition);if(!r)return reject(e,"Item consumption action is unsupported");
 const auto recipient=std::find_if(s.party.begin(),s.party.end(),[&](const SessionCharacter&c){return c.character_id==target;});if(recipient==s.party.end())return reject(e,"Item consumption target is unavailable");
 FieldItemUseCandidate next;next.inventory=s.inventory;next.party=s.party;const size_t recipient_index=size_t(recipient-s.party.begin());auto&c=next.party[recipient_index];const auto status=std::find_if(c.status.begin(),c.status.end(),[&](const SessionStatus&v){return v.status_id==r->status;});next.result.healed=status!=c.status.end();if(next.result.healed){c.status.erase(status);
  // PartyMember.remove_status also refreshes HP. No admitted status here marks
  // the target unconscious; the source recovery value is clamped by MAXHP.
  if(c.hp==0){const auto policy=std::find_if(data->status_policies().begin(),data->status_policies().end(),[&](const ItemUseStatusPolicy&p){return p.id==r->status;});c.hp=std::min<int64_t>(policy->refresh_hp_value,s.maximum_hp[recipient_index]);}
 }
 // Source HEAL_FAIL still performs consumption. transform_item is a different
 // action and is not implicitly called by reduce_or_drop_item.
 if(!r->reusable&&!next.inventory.consume_dose_uid(uid,found->doses,e))return false;
 next.result.removed=!r->reusable&&found->doses==1;next.result.doses_remaining=r->reusable?found->doses:found->doses-1;next.result.target=c.character_id;next.result.nickname=c.nickname;next.result.message_key=next.result.healed?r->heal_message:r->fail_message;if(next.result.healed)next.result.sound_source=r->success_sound;
 for(auto&owner:next.party)if(owner.character_id==s.owner){owner.inventory.clear();const auto items=next.inventory.content();for(const auto&i:next.inventory.instances())owner.inventory.push_back({std::string(items.string(items.definition(i.definition).source)),i.equipped!=0,int64_t(i.doses),i.id});}
 out=std::move(next);e.clear();return true;
}

bool FieldItemUseMenu::fail(const char*m){error_=m;return false;}
bool FieldItemUseMenu::initialize(ItemUseView data,FieldItemUseHost host,std::string_view language){
 if(!data||!data->valid()||!host.read||!host.commit||!data->locale(language))return fail("Field Items initialization rejected");
 *this=FieldItemUseMenu{};data_=data;host_=std::move(host);locale_=language;return true;
}
bool FieldItemUseMenu::set_locale(std::string_view language){if(!data_||!data_->locale(language))return fail("Field Items locale rejected");locale_=language;if(phase_==FieldItemUsePhase::Message)message_=expand(last_result_.healed?locale().heal:locale().fail,last_result_.nickname);return true;}
bool FieldItemUseMenu::read_snapshot(){FieldItemUseSnapshot next;std::string e;if(!host_.read(next,e)||!validate_item_use_snapshot(data_,next,e)){error_=e;return false;}snapshot_=std::move(next);if(selection_>=snapshot_.inventory.size())selection_=snapshot_.inventory.size()?snapshot_.inventory.size()-1:0;return true;}
bool FieldItemUseMenu::open(){if(!data_||!host_.read)return fail("Field Items missing content/host");if(!read_snapshot())return false;phase_=FieldItemUsePhase::Items;selection_=target_selection_=0;cursor_time_=0;message_.clear();last_result_={};sounds_.push_back(FieldItemUseSound::Open);return true;}
void FieldItemUseMenu::close(){phase_=FieldItemUsePhase::Closed;message_.clear();}
const ItemInstance*FieldItemUseMenu::selected_item()const{return selection_<snapshot_.inventory.size()?&snapshot_.inventory.instance(selection_):nullptr;}
bool FieldItemUseMenu::idle_frame(double dt){if(!std::isfinite(dt)||dt<0||dt>1)return fail("Field Items delta rejected");if(active())cursor_time_+=dt;return data_!=nullptr;}
bool FieldItemUseMenu::input(int x,int y,bool confirm,bool cancel,bool scope){
 if(x<-1||x>1||y<-1||y>1)return fail("Field Items direction rejected");if(!active())return true;
 if(scope)info_visible_=!info_visible_;
 if(phase_==FieldItemUsePhase::Message){if(confirm||cancel){if(!read_snapshot())return false;phase_=FieldItemUsePhase::Items;message_.clear();}return true;}
 if(cancel){if(phase_==FieldItemUsePhase::Items){close();sounds_.push_back(FieldItemUseSound::Close);}else{if(phase_==FieldItemUsePhase::Action)phase_=FieldItemUsePhase::Items;else phase_=FieldItemUsePhase::Action;sounds_.push_back(FieldItemUseSound::Back);}return true;}
 if(phase_==FieldItemUsePhase::Items){
  const auto size=snapshot_.inventory.size();if(size&&(x||y)){const uint32_t columns=uint32_t(data_->parameter(ItemUseParameter::Grid).x),rows=(size+columns-1)/columns;int row=int(selection_/columns),col=int(selection_%columns);if(y){row=(row+y+int(rows))%int(rows);col=std::min(col,int(std::min(columns,size-uint32_t(row)*columns))-1);}else{col=(col+x+int(columns))%int(columns);col=std::min(col,int(std::min(columns,size-uint32_t(row)*columns))-1);}auto next=uint32_t(row)*columns+uint32_t(col);if(next!=selection_){selection_=next;sounds_.push_back(FieldItemUseSound::Move);}}
  if(confirm&&size){const auto*i=selected_item();if(i&&data_->rule(i->definition)){uid_=i->id;phase_=FieldItemUsePhase::Action;sounds_.push_back(FieldItemUseSound::Confirm);}else sounds_.push_back(FieldItemUseSound::Restricted);}return true;
 }
 if(phase_==FieldItemUsePhase::Action){if(confirm){target_selection_=0;for(uint32_t i=0;i<snapshot_.party.size();++i)if(snapshot_.party[i].character_id==snapshot_.owner)target_selection_=i;phase_=FieldItemUsePhase::Targets;sounds_.push_back(FieldItemUseSound::Confirm);}return true;}
 if(phase_==FieldItemUsePhase::Targets){if(y){auto size=uint32_t(snapshot_.party.size());target_selection_=uint32_t((int(target_selection_)+y+int(size))%int(size));sounds_.push_back(FieldItemUseSound::Move);}if(confirm){
   ItemUseResult result;std::string e;if(!host_.commit(uid_,snapshot_.party[target_selection_].character_id,result,e)){error_=e;return false;}
   const auto*i=selected_item();const auto*r=i?data_->rule(i->definition):nullptr;if(!r||result.target!=snapshot_.party[target_selection_].character_id||result.nickname.empty()||!plain(result.nickname)||result.message_key!=(result.healed?r->heal_message:r->fail_message)||result.sound_source!=(result.healed?r->success_sound:std::string{}))return fail("Field Items host result rejected");
   last_result_=std::move(result);message_=expand(last_result_.healed?locale().heal:locale().fail,last_result_.nickname);phase_=FieldItemUsePhase::Message;sounds_.push_back(FieldItemUseSound::Confirm);if(last_result_.healed)sounds_.push_back(FieldItemUseSound::Heal);
  }return true;
 }
 return fail("Field Items unknown phase");
}
}
