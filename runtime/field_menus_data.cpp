#include "encore/field_menus.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <set>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace encore::upstream {namespace {
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void put(std::vector<uint8_t>&v,uint32_t n){for(unsigned i=0;i<4;++i)v.push_back(uint8_t(n>>(i*8)));}
bool fail(std::string&e,const char*s){e=s;return false;}
bool identity(std::string_view s){if(s.empty()||s.size()>128)return false;for(unsigned char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;return true;}
struct Reader {
 const uint8_t*p;size_t n;bool ok=true;
 uint32_t integer(){if(n<4){ok=false;return 0;}auto v=u32(p);p+=4;n-=4;return v;}
 uint32_t count(uint32_t max){auto v=integer();if(v>max){ok=false;return 0;}return v;}
 float real(){auto v=integer();float f;std::memcpy(&f,&v,4);if(!std::isfinite(f)||std::abs(f)>1000000)ok=false;return f;}
 double number(){if(n<8){ok=false;return 0;}uint64_t v=uint64_t(u32(p))|uint64_t(u32(p+4))<<32;double d;std::memcpy(&d,&v,8);p+=8;n-=8;if(!std::isfinite(d))ok=false;return d;}
 std::string text(){auto len=count(4096);if(len>n){ok=false;return{};}std::string s(reinterpret_cast<const char*>(p),len);p+=len;n-=len;size_t at=0;uint32_t cp=0;while(at<s.size())if(!encore::utf8_next(s,at,cp)||cp==0||(cp<32&&cp!='\n')||cp==127){ok=false;break;}return s;}
 FieldMenuLabel label(){return{text(),text()};}
};
bool path_ok(const char*p){if(!p||!*p)return false;std::string s(p);return s.size()<1000&&s.find("..") == s.npos&&s.back()!='/';}
bool read_bytes(const char*path,std::vector<uint8_t>&out,bool&missing,std::string&e){missing=false;errno=0;FILE*f=std::fopen(path,"rb");if(!f){missing=errno==ENOENT;return missing||fail(e,"Cannot open field preferences");}uint8_t bytes[1025];auto n=std::fread(bytes,1,sizeof bytes,f);bool ok=!std::ferror(f)&&n<=1024;if(std::fclose(f))ok=false;if(!ok)return fail(e,"Field preferences read/size rejected");out.assign(bytes,bytes+n);return true;}
bool write_new(const std::string&p,const std::vector<uint8_t>&bytes,std::string&e){int fd=::open(p.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return fail(e,"Cannot exclusively create field preferences temporary file");FILE*f=::fdopen(fd,"wb");if(!f){::close(fd);std::remove(p.c_str());return fail(e,"Cannot open field preferences stream");}bool ok=std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size();if(std::fflush(f))ok=false;if(std::fclose(f))ok=false;if(!ok){std::remove(p.c_str());return fail(e,"Field preferences write/flush failed");}return true;}
}
const FieldMenuItem*FieldMenusData::item(std::string_view id)const{for(const auto&i:items)if(i.id==id)return &i;return nullptr;}
bool expand_field_menu_text(std::string_view pattern,std::string_view owner,const std::vector<std::string>&buttons,std::string&out,std::string&e){
 if(buttons.size()!=8||pattern.find_first_of("[]")!=pattern.npos)return fail(e,"Field menu text context rejected");std::string next;
 while(!pattern.empty()){auto at=pattern.find_first_of("{}");if(at==pattern.npos){next.append(pattern);break;}next.append(pattern.substr(0,at));pattern.remove_prefix(at);auto end=pattern.find('}');if(pattern.front()!='{'||end==pattern.npos)return fail(e,"Unterminated field menu text token");auto token=pattern.substr(1,end-1);if(token=="owner"||token=="target")next.append(owner);else if(token.size()==8&&token.substr(0,7)=="button:"&&token[7]>='0'&&token[7]<='7')next+=buttons[size_t(token[7]-'0')];else return fail(e,"Unknown field menu text token");pattern.remove_prefix(end+1);if(next.size()>8192)return fail(e,"Expanded field menu text exceeds bounds");}
 if(next.size()>8192)return fail(e,"Expanded field menu text exceeds bounds");out=std::move(next);e.clear();return true;
}
bool FieldMenusData::bind_items(ItemView view,ItemDetailsView details,std::string&e)const{
 if(!valid_||!details.bind_items(view,e))return false;std::set<uint32_t>definitions;
 for(const auto&i:items)if(!i.key){if(i.definition>=view.count(ItemSection::Definitions)||!definitions.insert(i.definition).second||view.string(view.definition(i.definition).source)!=i.id)return fail(e,"Field item description identity mismatch");bool found=false;for(uint32_t j=0;j<details.count(ItemDetailsSection::Definitions);++j){const auto d=details.definition(j);if(d.definition==i.definition&&d.max_doses==i.doses)found=true;}if(!found)return fail(e,"Field item description dose binding mismatch");}
 if(definitions.size()!=view.count(ItemSection::Definitions))return fail(e,"Field item description coverage mismatch");e.clear();return true;
}
bool FieldMenusData::supports(const FieldMenuPreferences&p)const{
 if(!valid_||p.display>1||p.buttons.size()!=logical_buttons.size())return false;
 for(auto v:p.volume)if(v>=volume_db.size())return false;
 std::set<uint32_t>unique;for(auto v:p.buttons)if(std::find(logical_buttons.begin(),logical_buttons.end(),v)==logical_buttons.end()||!unique.insert(v).second)return false;return true;
}
uint32_t FieldMenusData::remap(uint32_t raw,const FieldMenuPreferences&p)const{if(!supports(p))return 0;uint32_t result=raw;for(auto mask:logical_buttons)result&=~mask;for(size_t i=0;i<p.buttons.size();++i)if(raw&p.buttons[i])result|=logical_buttons[i];return result;}
bool FieldMenusData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<52||n>4*1024*1024||std::memcmp(p,"ENCFMN01",8)||u32(p+8)!=2||u32(p+12)!=n||u32(p+20)!=2||u32(p+24)!=1||u32(p+28)||encore::crc32(p+32,n-32)!=u32(p+16))return fail(e,"Field menus schema/size/capabilities/rules/checksum rejected");
 // This port admits one reviewed upstream. This is provenance, not game data.
 constexpr char pin[]="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail(e,"Field menus pin rejected");}
 FieldMenusData d;Reader r{p+52,n-52};auto count=r.count(256);for(uint32_t i=0;i<count;++i)d.labels.push_back(r.label());
 if(count!=uint32_t(FieldMenuText::Count))return fail(e,"Field menus label coverage rejected");
 std::set<std::string>ids;count=r.count(128);for(uint32_t i=0;i<count;++i){FieldMenuItem v;v.id=r.text();v.key=r.integer();v.drop=r.integer();v.action=r.integer();v.doses=r.integer();v.score=r.integer();v.equipped_score=r.integer();v.slot=r.integer();v.definition=r.integer();v.name=r.label();v.description=r.label();v.sorting=r.label();v.result=r.label();if(!identity(v.id)||!ids.insert(v.id).second||v.key>1||v.drop>1||v.action>3||!v.doses||v.doses>65535||(v.key?v.definition!=UINT32_MAX:v.definition>=64)||(v.action==1?v.slot>3:v.slot!=UINT32_MAX)||(v.key&&(v.drop||v.action==1||v.action==2))||v.name.en.empty()||v.name.zh.empty()||(v.action==2&&(v.result.en.empty()||v.result.zh.empty())))return fail(e,"Field menu item rule/action/identity rejected");std::string expanded;const std::vector<std::string>buttons(8);if(!expand_field_menu_text(v.result.en,"",buttons,expanded,e)||!expand_field_menu_text(v.result.zh,"",buttons,expanded,e))return false;d.items.push_back(std::move(v));}
 if(d.items.empty())return fail(e,"Field menu item rules missing");
 ids.clear();count=r.count(256);for(uint32_t i=0;i<count;++i){FieldMenuSkill v;v.id=r.text();v.flag=r.text();v.battle_skill=r.text();v.weapon=r.text();v.category=r.integer();v.cost=r.integer();v.name=r.label();v.description=r.label();auto weapons=r.count(64);std::set<std::string>weapon_ids;for(uint32_t w=0;w<weapons;++w){auto id=r.text();if(!identity(id)||!weapon_ids.insert(id).second)return fail(e,"Field skill weapon binding rejected");v.weapons.push_back(id);}if(v.weapon.empty()!=v.weapons.empty())return fail(e,"Field skill weapon coverage rejected");if(!identity(v.id)||(!v.flag.empty()&&!identity(v.flag))||(!v.battle_skill.empty()&&!identity(v.battle_skill))||(!v.weapon.empty()&&!identity(v.weapon))||v.category>2||v.cost>65535||v.name.en.empty()||v.name.zh.empty()||!ids.insert(std::to_string(v.category)+"/"+v.id).second)return fail(e,"Field menu skill condition/identity rejected");std::string expanded;const std::vector<std::string>buttons(8);if(!expand_field_menu_text(v.description.en,"",buttons,expanded,e)||!expand_field_menu_text(v.description.zh,"",buttons,expanded,e))return false;d.skills.push_back(std::move(v));}
 size_t image_bytes=0;count=r.count(32);for(uint32_t i=0;i<count;++i){FieldMenuImage v;v.width=r.integer();v.height=r.integer();auto bytes=r.integer();if(!v.width||v.width>1024||!v.height||v.height>1024||uint64_t(v.width)*v.height*4!=bytes||bytes>r.n||image_bytes+bytes>3*1024*1024)return fail(e,"Field map image bounds rejected");v.rgba.assign(r.p,r.p+bytes);r.p+=bytes;r.n-=bytes;image_bytes+=bytes;d.images.push_back(std::move(v));}
 if(d.images.size()<3)return fail(e,"Field map images missing");
 count=r.count(128);for(uint32_t i=0;i<count;++i){FieldMenuMarker v;v.image=r.integer();v.x=r.real();v.y=r.real();auto flip=r.integer();v.flip=flip!=0;v.label=r.label();if(flip>1||(v.image!=UINT32_MAX&&v.image>=d.images.size())||v.x<0||v.y<0||v.x>d.images[0].width||v.y>d.images[0].height||(v.image==UINT32_MAX&&v.label.en.empty()))return fail(e,"Field map marker reference rejected");d.markers.push_back(std::move(v));}
 for(auto&g:d.geometry){g={r.real(),r.real(),r.real(),r.real()};if(g.x<0||g.y<0||g.w<=0||g.h<=0||g.x+g.w>320||g.y+g.h>180)return fail(e,"Field menu geometry rejected");}
 d.rows=r.integer();d.columns=r.integer();d.row_pitch=r.real();d.column_pitch=r.real();d.line_height=r.real();if(!d.rows||d.rows>16||d.columns!=2||d.row_pitch<=0||d.row_pitch>32||d.column_pitch<=0||d.column_pitch>160||d.line_height<=0||d.line_height>32||d.rows*d.row_pitch>d.geometry[2].h)return fail(e,"Field menu grid rejected");
 count=r.count(32);double last=-121;for(uint32_t i=0;i<count;++i){auto v=r.number();if(v<-120||v>0||v<=last)return fail(e,"Field volume curve rejected");d.volume_db.push_back(v);last=v;}if(d.volume_db.size()<2||d.volume_db.back()!=0)return fail(e,"Field volume range rejected");
 std::set<uint32_t>masks;count=r.count(16);for(uint32_t i=0;i<count;++i){auto v=r.integer();if(!v||v>4095||(v&(v-1))||!masks.insert(v).second||(v&240))return fail(e,"Field control identity rejected");d.logical_buttons.push_back(v);}if(count!=8)return fail(e,"Field control coverage rejected");
 count=r.count(16);for(uint32_t i=0;i<count;++i)d.button_names.push_back(r.text());if(d.button_names.size()!=d.logical_buttons.size()||std::any_of(d.button_names.begin(),d.button_names.end(),[](const std::string&s){return s.empty()||s.size()>32||s.find_first_of("[]{}\n")!=s.npos;}))return fail(e,"Field button names rejected");
 for(auto&v:d.defaults.volume)v=r.integer();
 d.defaults.display=r.integer();d.defaults.buttons=d.logical_buttons;
 count=r.count(16);for(uint32_t i=0;i<count;++i){auto s=r.text();if(!identity(s)||std::find(d.locales.begin(),d.locales.end(),s)!=d.locales.end())return fail(e,"Field locale identity rejected");d.locales.push_back(s);}count=r.count(16);for(uint32_t i=0;i<count;++i)d.locale_names.push_back(r.label());if(d.locales.size()!=2||d.locales.size()!=d.locale_names.size())return fail(e,"Field locale coverage rejected");
 d.map_item=r.text();d.map_offset_x=r.real();d.map_offset_y=r.real();d.map_divisor=r.real();d.map_speed=r.real();d.map_fast=r.real();auto toggle=r.integer();d.description_toggle=toggle!=0;if(!d.item(d.map_item)||d.item(d.map_item)->action!=3||d.map_divisor<=0||d.map_speed<=0||d.map_speed>1000||d.map_fast<1||d.map_fast>10||toggle>1)return fail(e,"Field map policy rejected");
 for(auto&group:d.setting_labels){count=r.count(32);for(uint32_t i=0;i<count;++i)group.push_back(r.label());if(group.empty())return fail(e,"Field setting choices missing");}
 count=r.count(8);for(uint32_t i=0;i<count;++i)d.display_names.push_back(r.text());d.capture_cancel=r.label();if(d.display_names.size()!=2||d.capture_cancel.en.empty()||d.capture_cancel.zh.empty())return fail(e,"Field platform choices missing");
 d.map_home_x=r.real();d.map_home_y=r.real();for(auto&size:d.display_sizes){size={r.integer(),r.integer()};if(!size[0]||!size[1]||size[0]>400||size[1]>240)return fail(e,"Field display size rejected");}
 d.valid_=true;if(!r.ok||r.n||!d.supports(d.defaults))return fail(e,"Field menus truncated/trailing/defaults rejected");*this=std::move(d);e.clear();return true;
}
bool FieldMenusData::load_file(const char*path,std::string&e){std::vector<uint8_t>b;return encore::read_file(path,b,4*1024*1024,e)&&load(b.data(),b.size(),e);}
bool encode_field_preferences(const FieldMenusData&d,const FieldMenuPreferences&p,std::vector<uint8_t>&out,std::string&e){if(!d.supports(p))return fail(e,"Unsupported field preferences");std::vector<uint8_t>b{'E','N','C','M','P','R','0','1'};put(b,1);for(auto v:p.volume)put(b,v);put(b,p.display);put(b,uint32_t(p.buttons.size()));for(auto v:p.buttons)put(b,v);put(b,encore::crc32(b.data(),b.size()));out=std::move(b);e.clear();return true;}
bool decode_field_preferences(const FieldMenusData&d,const uint8_t*p,size_t n,FieldMenuPreferences&out,std::string&e){if(!p||n<36||n>1024||std::memcmp(p,"ENCMPR01",8)||u32(p+8)!=1||encore::crc32(p,n-4)!=u32(p+n-4))return fail(e,"Field preferences schema/checksum rejected");Reader r{p+12,n-16};FieldMenuPreferences next;for(auto&v:next.volume)v=r.integer();next.display=r.integer();auto count=r.count(16);for(uint32_t i=0;i<count;++i)next.buttons.push_back(r.integer());if(!r.ok||r.n||!d.supports(next))return fail(e,"Field preferences values/length rejected");out=std::move(next);e.clear();return true;}
FieldPreferenceRead read_field_preferences(const FieldMenusData&d,const char*path,FieldMenuPreferences&out,std::string&e){if(!path_ok(path)){e="Invalid field preference path";return FieldPreferenceRead::Error;}std::vector<uint8_t>b;bool missing=false;if(!read_bytes(path,b,missing,e))return FieldPreferenceRead::Error;if(missing){e.clear();return FieldPreferenceRead::Missing;}return decode_field_preferences(d,b.data(),b.size(),out,e)?FieldPreferenceRead::Loaded:FieldPreferenceRead::Error;}
bool write_field_preferences(const FieldMenusData&d,const char*path,const FieldMenuPreferences&v,std::string&e){
 if(!path_ok(path))return fail(e,"Invalid field preference path");std::vector<uint8_t>next,prior,check;bool missing=false,absent=false;FieldMenuPreferences old;
 if(!encode_field_preferences(d,v,next,e)||!read_bytes(path,prior,missing,e)||(!missing&&!decode_field_preferences(d,prior.data(),prior.size(),old,e)))return false;
 const std::string primary(path),temp=primary+".tmp",backup=primary+".bak";
 if(!write_new(temp,next,e))return false;
 if(!read_bytes(temp.c_str(),check,absent,e)||absent||check!=next){std::remove(temp.c_str());return fail(e,"Field preferences read-back failed");}
 if(!missing){if(std::remove(backup.c_str())&&errno!=ENOENT){std::remove(temp.c_str());return fail(e,"Cannot rotate field preferences backup");}if(std::rename(primary.c_str(),backup.c_str())){std::remove(temp.c_str());return fail(e,"Cannot preserve field preferences");}}
 if(std::rename(temp.c_str(),primary.c_str())){std::remove(temp.c_str());e="Cannot commit field preferences";if(!missing){std::string detail;bool gone=false;if(read_bytes(primary.c_str(),check,gone,detail)&&gone&&write_new(primary,prior,detail))e+="; previous preferences restored";else e+="; previous preferences retained in .bak";}return false;}
 e.clear();return true;
}
}
