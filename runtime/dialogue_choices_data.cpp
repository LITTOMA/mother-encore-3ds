#include "encore/dialogue_choices_data.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <utility>

namespace encore::upstream {namespace {
constexpr size_t max_pack_bytes=1024*1024;
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(int j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
struct Reader {
 const uint8_t*p;size_t n;bool ok=true;
 uint32_t integer(){if(n<4){ok=false;return 0;}const auto v=u32(p);p+=4;n-=4;return v;}
 float real(){const uint32_t v=integer();float f;std::memcpy(&f,&v,4);if(!std::isfinite(f)||std::abs(f)>1000000)ok=false;return f;}
 double number(){if(n<8){ok=false;return 0;}const uint64_t v=u32(p)|uint64_t(u32(p+4))<<32;p+=8;n-=8;double d;std::memcpy(&d,&v,8);if(!std::isfinite(d))ok=false;return d;}
 uint32_t count(uint32_t maximum){auto v=integer();if(v>maximum){ok=false;return 0;}return v;}
 std::string text(){const auto size=integer();if(size>4096||size>n){ok=false;return {};}std::string out(reinterpret_cast<const char*>(p),size);p+=size;n-=size;for(unsigned char c:out)if(c<32||c>126)ok=false;return out;}
 DialogueChoiceRect rect(){return {real(),real(),real(),real()};}
};
bool safe_path(const std::string&s){
 if(s.empty()||s.front()=='/'||s.back()=='/'||s.find('\\')!=std::string::npos||s.find(':')!=std::string::npos)return false;
 size_t pos=0;while(pos<s.size()){auto end=s.find('/',pos);if(end==std::string::npos)end=s.size();auto part=s.substr(pos,end-pos);if(part.empty()||part=="."||part=="..")return false;pos=end+1;}return true;
}
}
bool DialogueChoicesData::validate_program(uint32_t index,std::string_view identity,uint32_t extent,std::string&e)const{
 if(!valid_||index>=groups_.size()){e="Unknown/unloaded dialogue choice group";return false;}
 const auto&g=groups_[index];if(identity!=g.program_identity||extent!=g.program_command_count){e="Dialogue choice program identity/extent differs";return false;}
 if(g.cancel_target_pc>=extent){e="Dialogue cancel target outside program";return false;}
 for(const auto&o:g.options)if(o.target_pc>=extent){e="Dialogue option target outside program";return false;}
 e.clear();return true;
}
bool DialogueChoicesData::load(const uint8_t*p,size_t n,std::string&e){
 auto fail=[&](const char*s){e=s;return false;};
 if(!p||n<24||n>max_pack_bytes)return fail("Dialogue choice pack size rejected");
 if(std::memcmp(p,"ENCCHOIC",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1)return fail("Dialogue choice schema/size/capability rejected");
 if(crc(p+24,n-24)!=u32(p+16))return fail("Dialogue choice CRC mismatch");
 DialogueChoicesData d;Reader r{p+24,n-24};d.columns_=r.integer();d.children_=r.integer();d.grid_=r.rect();
 d.minimum_.w=r.real();d.minimum_.h=r.real();d.arrow_geometry_=r.rect();
 d.move_=r.number();d.loop_=r.number();d.font_height_=r.real();d.color_=r.integer();d.trailing_blank_lines_=r.integer();
 if(!d.trailing_blank_lines_||d.trailing_blank_lines_>16)return fail("Dialogue choice trailing lines rejected");
 if(d.columns_<1||d.columns_>16||d.children_<d.columns_||d.children_>64)return fail("Dialogue choice grid domain rejected");
 for(float v:{d.grid_.w,d.grid_.h,d.minimum_.w,d.minimum_.h,d.arrow_geometry_.w,d.arrow_geometry_.h,d.font_height_})if(v<=0||v>4096)return fail("Dialogue choice geometry rejected");
 if(d.move_<=0||d.move_>120||d.loop_<=0||d.loop_>120)return fail("Dialogue choice timing rejected");
 for(unsigned i=0;i<3;++i){auto s=r.text();if(!safe_path(s))return fail("Dialogue choice sound rejected");d.sounds_.push_back(std::move(s));}
 d.arrow_.path=r.text();d.arrow_.width=r.integer();d.arrow_.height=r.integer();d.arrow_.columns=r.integer();d.arrow_.rows=r.integer();d.font_path_=r.text();
 const auto&a=d.arrow_;if(!safe_path(a.path)||!safe_path(d.font_path_)||!a.width||a.width>1024||!a.height||a.height>1024||!a.columns||a.columns>256||!a.rows||a.rows>256||a.width%a.columns||a.height%a.rows||float(a.width/a.columns)!=d.arrow_geometry_.w||float(a.height/a.rows)!=d.arrow_geometry_.h)return fail("Dialogue choice resource rejected");
 auto count=r.count(256);for(uint32_t i=0;i<count;++i){DialogueChoiceKey key;key.time=r.number();key.frame=r.integer();if(key.time<0||key.time>=d.loop_||(i?key.time<=d.keys_.back().time:key.time!=0)||key.frame>=uint64_t(a.columns)*a.rows)return fail("Dialogue choice animation rejected");d.keys_.push_back(key);}if(d.keys_.empty())return fail("Dialogue choice animation missing");
 count=r.count(32);std::set<std::string>groups;
 for(uint32_t i=0;i<count;++i){
  DialogueChoiceGroup g;g.id=r.text();g.program_identity=r.text();g.source_label=r.text();g.program_command_count=r.integer();g.initial_selection=r.integer();g.cancel_target_pc=r.integer();
  if(g.id.empty()||!groups.insert(g.id).second||!safe_path(g.program_identity)||g.source_label.empty()||!g.program_command_count||g.program_command_count>1000000||g.cancel_target_pc>=g.program_command_count)return fail("Dialogue choice group/branch rejected");
  const auto options=r.count(16);if(!options||options>d.columns_||options>d.children_||g.initial_selection>=options)return fail("Dialogue choice visible option domain rejected");
  std::set<std::string>keys;
  for(uint32_t j=0;j<options;++j){DialogueChoiceOption o;o.translation_key=r.text();o.text=r.text();o.target_pc=r.integer();o.rect=r.rect();
   if(o.translation_key.empty()||!keys.insert(o.translation_key).second||o.text.empty()||o.target_pc>=g.program_command_count||o.rect.x<0||o.rect.y!=0||o.rect.w<d.minimum_.w||o.rect.h<d.minimum_.h||o.rect.x+o.rect.w>d.grid_.w||o.rect.y+o.rect.h>d.grid_.h||(j&&o.rect.x<g.options.back().rect.x+g.options.back().rect.w))return fail("Dialogue choice option/text/target rejected");
   g.options.push_back(std::move(o));
  }
  d.groups_.push_back(std::move(g));
 }
 if(d.groups_.empty()||!r.ok||r.n)return fail("Dialogue choice empty/truncated/nonfinite/trailing payload");
 d.valid_=true;*this=std::move(d);e.clear();return true;
}
bool DialogueChoicesData::load_file(const char*path,std::string&e){
 if(!path){e="Dialogue choice path missing";return false;}FILE*f=std::fopen(path,"rb");if(!f){e="Could not open dialogue choice pack";return false;}
 std::vector<uint8_t>bytes;uint8_t block[4096];
 for(;;){const size_t n=std::fread(block,1,sizeof(block),f);if(bytes.size()+n>max_pack_bytes){std::fclose(f);e="Dialogue choice file exceeds bound";return false;}bytes.insert(bytes.end(),block,block+n);if(n<sizeof(block)){const bool failed=std::ferror(f);std::fclose(f);if(failed){e="Dialogue choice read failed";return false;}break;}}
 return load(bytes.data(),bytes.size(),e);
}
}
