#include "encore/title_locale_data.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
constexpr size_t limit=64*1024;
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
struct Reader {
 const uint8_t*p;size_t n;bool ok=true;
 uint32_t integer(){if(n<4){ok=false;return 0;}auto v=u32(p);p+=4;n-=4;return v;}
 std::string text(){auto k=integer();if(!k||k>512||k>n){ok=false;return{};}std::string s(reinterpret_cast<const char*>(p),k);p+=k;n-=k;for(unsigned char c:s)if(c<32||c>126)ok=false;return s;}
};
bool locale_code(const std::string&s){if(s.empty()||s.size()>32)return false;for(unsigned char c:s)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;return true;}
bool safe_path(const std::string&s){if(s.empty()||s.front()=='/'||s.back()=='/'||s.find('\\')!=s.npos||s.find(':')!=s.npos)return false;size_t at=0;while(at<s.size()){auto end=s.find('/',at);if(end==s.npos)end=s.size();const auto part=s.substr(at,end-at);if(part.empty()||part=="."||part=="..")return false;at=end+1;}return s.size()>4&&s.substr(s.size()-4)==".t3x";}
}
bool TitleLocaleData::load(const uint8_t*p,size_t n,std::string&e){
 auto fail=[&](const char*s){e=s;return false;};
 if(!p||n<24||n>limit)return fail("Title locale pack size rejected");
 if(std::memcmp(p,"ENCTLCL1",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1)return fail("Title locale schema/size/capability rejected");
 if(crc(p+24,n-24)!=u32(p+16))return fail("Title locale CRC rejected");
 TitleLocaleData d;Reader r{p+24,n-24};d.fallback_=r.text();const auto count=r.integer();
 if(!locale_code(d.fallback_)||!count||count>128)return fail("Title locale fallback/count rejected");
 std::set<std::pair<std::string,std::string>>bindings;std::set<std::string>targets;
 for(uint32_t i=0;i<count;++i){TitleTextureRemap a{r.text(),r.text(),r.text(),r.integer(),r.integer()};
  if(!locale_code(a.locale)||a.locale==d.fallback_||!safe_path(a.base_path)||!safe_path(a.path)||a.path==a.base_path||!a.width||!a.height||a.width>1024||a.height>1024||!bindings.insert({a.locale,a.base_path}).second||!targets.insert(a.path).second)return fail("Title locale resource/duplicate binding rejected");
  d.remaps_.push_back(std::move(a));
 }
 if(!r.ok||r.n)return fail("Title locale truncated/trailing payload rejected");
 *this=std::move(d);e.clear();return true;
}
bool TitleLocaleData::load_file(const char*path,std::string&e){
 if(!path){e="Missing title locale path";return false;}FILE*f=std::fopen(path,"rb");if(!f){e="Could not open title locale pack";return false;}
 std::vector<uint8_t>b;uint8_t block[4096];while(true){auto n=std::fread(block,1,sizeof(block),f);if(b.size()+n>limit){std::fclose(f);e="Title locale file exceeds bound";return false;}b.insert(b.end(),block,block+n);if(n<sizeof(block)){bool bad=std::ferror(f);std::fclose(f);if(bad){e="Title locale read error";return false;}break;}}
 return load(b.data(),b.size(),e);
}
bool TitleLocaleData::resolve(const ContinueMenuData&d,const LocaleSelection&selection,std::vector<SaveMenuResource>&out,std::vector<SaveMenuRect>&rects,std::string&e)const{
 auto fail=[&](const char*s){e=s;return false;};const auto*c=selection.catalog();
 if(!valid()||!d.valid()||!c||!c->valid()||selection.code().empty()||fallback_!=c->fallback())return fail("Title locale requires checked matching data and selection");
 auto resources=d.resources();std::vector<SaveMenuRect>positions;std::set<std::string>options;
 for(const auto&o:d.title_options())for(auto index:{o.resource,o.selected_resource})if(!options.insert(resources[index].path).second)return fail("Title locale option aliases rejected");
 // Validate every binding, including inactive locales, against option-only
 // resources. An external pack cannot replace primary title scene layers.
 for(const auto&r:remaps_)if(!options.count(r.base_path)||c->locale_index(r.locale)<0)return fail("Title locale unknown option/locale binding rejected");
 if(selection.code()!=fallback_){size_t found=0;for(const auto&r:remaps_)if(r.locale==selection.code()){
   auto i=std::find_if(resources.begin(),resources.end(),[&](const SaveMenuResource&x){return x.path==r.base_path;});
   if(i==resources.end())return fail("Title locale stale resource binding");
   *i={r.path,r.width,r.height,1,1};++found;
  }if(found!=options.size())return fail("Title locale incomplete option coverage");
 }
 const auto ref=d.layout(ContinueLayout::Reference);
 for(const auto&o:d.title_options()){
  const auto&a=resources[o.resource];const auto&b=resources[o.selected_resource];
  if(a.width!=b.width||a.height!=b.height)return fail("Title locale normal/selected dimensions disagree");
  SaveMenuRect rect{o.rect.x+(o.rect.w-a.width)/2,o.rect.y+(o.rect.h-a.height)/2,float(a.width),float(a.height)};
  for(size_t v=0;v<o.viewport_offsets.size();++v){const auto&s=o.viewport_offsets[v];if(rect.x+s.x<0||rect.y+s.y<0||rect.x+s.x+rect.w>(v?ref.w:ref.x)||rect.y+s.y+rect.h>(v?ref.h:ref.y))return fail("Title locale rectangle outside checked viewport");}
  positions.push_back(rect);
 }
 out=std::move(resources);rects=std::move(positions);e.clear();return true;
}
}
