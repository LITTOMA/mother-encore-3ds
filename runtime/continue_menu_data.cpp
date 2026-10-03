#include "encore/continue_menu_data.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
constexpr size_t limit=1024*1024;
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
struct Reader {
 const uint8_t*p;size_t n;bool ok=true;
 uint32_t integer(){if(n<4){ok=false;return 0;}auto x=u32(p);p+=4;n-=4;return x;}
 float real(){auto u=integer();float x;std::memcpy(&x,&u,4);if(!std::isfinite(x)||std::abs(x)>1000000)ok=false;return x;}
 double number(){if(n<8){ok=false;return 0;}uint64_t u=u32(p)|uint64_t(u32(p+4))<<32;p+=8;n-=8;double x;std::memcpy(&x,&u,8);if(!std::isfinite(x))ok=false;return x;}
 std::string text(){auto k=integer();if(k>4096||k>n){ok=false;return{};}std::string s(reinterpret_cast<const char*>(p),k);p+=k;n-=k;for(unsigned char c:s)if(c<32||c>126)ok=false;return s;}
 uint32_t count(uint32_t max){auto x=integer();if(x>max){ok=false;return 0;}return x;}
 SaveMenuRect rect(){return{real(),real(),real(),real()};}
};
bool path(const std::string&s){if(s.empty()||s.front()=='/'||s.back()=='/'||s.find('\\')!=s.npos||s.find(':')!=s.npos)return false;size_t at=0;while(at<s.size()){auto end=s.find('/',at);if(end==s.npos)end=s.size();const auto part=s.substr(at,end-at);if(part.empty()||part=="."||part=="..")return false;at=end+1;}return true;}
bool dimensions(const SaveMenuRect&r){return r.w>0&&r.h>0&&r.w<=4096&&r.h<=4096;}
bool title_offsets(const SaveMenuRect&rect,const std::array<ContinueViewportOffset,2>&offsets,const SaveMenuRect&reference){
 if(offsets[0].x!=0||offsets[0].y!=0)return false;
 for(size_t i=0;i<offsets.size();++i){const auto&o=offsets[i];const float width=i?reference.w:reference.x,height=i?reference.h:reference.y;if(o.x!=std::floor(o.x)||o.y!=std::floor(o.y)||rect.x+o.x<0||rect.y+o.y<0||rect.x+o.x+rect.w>width||rect.y+o.y+rect.h>height)return false;}
 return true;
}
}
double ContinueFadeAnimation::value(double seconds)const{double x=std::clamp(seconds/key_end,0.0,1.0);x=ease<1?1-std::pow(1-x,1/ease):std::pow(x,ease);return from+(to-from)*x;}
int ContinueMenuData::viewport_index(float width)const{for(size_t i=0;i<viewports_.size();++i)if(viewports_[i].width==width)return int(i);return -1;}
bool ContinueMenuData::load(const uint8_t*p,size_t n,std::string&e){
 auto fail=[&](const char*s){e=s;return false;};
 if(!p||n<24||n>limit)return fail("Continue pack size rejected");
 if(std::memcmp(p,"ENCCONT1",8)||u32(p+8)!=2||u32(p+12)!=n||u32(p+20)!=1)return fail("Continue schema/size/capability rejected");
 if(crc(p+24,n-24)!=u32(p+16))return fail("Continue CRC rejected");
 ContinueMenuData d;Reader r{p+24,n-24};d.background_=r.integer();d.title_background_=r.integer();d.door_in_=r.number();d.door_out_=r.number();d.music_fade_=r.number();d.door_music_fade_=r.number();d.action_repeat_=r.number();d.title_music_=r.text();
 if((d.title_background_>>24)!=255)return fail("Continue title exterior opacity rejected");
 if(!path(d.title_music_))return fail("Continue music path rejected");
 for(double t:{d.door_in_,d.door_out_,d.music_fade_,d.door_music_fade_,d.action_repeat_})if(t<=0||t>120)return fail("Continue timing rejected");
 auto count=r.count(64);if(count!=4)return fail("Continue action text count rejected");for(uint32_t i=0;i<count;++i){auto s=r.text();if(s.empty())return fail("Continue text empty");d.texts_.push_back(s);}
 count=r.count(64);if(count!=uint32_t(ContinueSound::Count))return fail("Continue sound count rejected");for(uint32_t i=0;i<count;++i){auto s=r.text();if(!path(s))return fail("Continue sound path rejected");d.sounds_.push_back(s);}
 count=r.count(64);if(count!=uint32_t(ContinueLayout::Count))return fail("Continue layout count rejected");for(uint32_t i=0;i<count;++i)d.layouts_.push_back(r.rect());
 count=r.count(64);if(count!=uint32_t(ContinueAnimation::Count))return fail("Continue animation count rejected");for(uint32_t i=0;i<count;++i){ContinueFadeAnimation a{r.number(),r.number(),r.number(),r.number(),r.number()};if(a.length<=0||a.length>120||a.key_end<=0||a.key_end>a.length||a.from<0||a.from>1||a.to<0||a.to>1||a.ease<=0||a.ease>100)return fail("Continue fade curve rejected");d.animations_.push_back(a);}
 count=r.count(64);std::set<std::string>paths;for(uint32_t i=0;i<count;++i){SaveMenuResource a;a.path=r.text();a.width=r.integer();a.height=r.integer();a.columns=r.integer();a.rows=r.integer();if(!path(a.path)||!paths.insert(a.path).second||!a.width||!a.height||a.width>1024||a.height>1024||a.columns!=1||a.rows!=1)return fail("Continue texture rejected");d.resources_.push_back(a);}if(d.resources_.empty())return fail("Continue texture set empty");
 count=r.count(64);if(!count)return fail("Continue title layers empty");for(uint32_t i=0;i<count;++i){ContinueDraw a{r.integer(),r.rect(),{}};for(auto&o:a.viewport_offsets)o={r.real(),r.real()};if(a.resource>=d.resources_.size()||!dimensions(a.rect))return fail("Continue title draw rejected");const auto&res=d.resources_[a.resource];if(a.rect.w!=res.width||a.rect.h!=res.height)return fail("Continue title sprite scaling rejected");d.layers_.push_back(a);}
 count=r.count(64);if(count!=4)return fail("Continue title options rejected");for(uint32_t i=0;i<count;++i){ContinueTitleOption a{r.integer(),r.integer(),r.rect(),{}};for(auto&o:a.viewport_offsets)o={r.real(),r.real()};if(a.resource>=d.resources_.size()||a.selected_resource>=d.resources_.size()||!dimensions(a.rect))return fail("Continue title option rejected");for(auto res:{a.resource,a.selected_resource})if(d.resources_[res].width!=a.rect.w||d.resources_[res].height!=a.rect.h)return fail("Continue title option scaling rejected");d.options_.push_back(a);}
 count=r.count(8);if(count!=2)return fail("Continue viewport count rejected");for(uint32_t i=0;i<count;++i){ContinueViewport a;a.width=r.real();auto k=r.count(8);if(k!=a.actions.size()||a.width<=0||a.width>4096||(i&&a.width<=d.viewports_.back().width))return fail("Continue viewport rejected");for(auto&b:a.actions){b=r.rect();if(!dimensions(b))return fail("Continue action rectangle rejected");}d.viewports_.push_back(a);}
 if(!r.ok||r.n)return fail("Continue truncated/nonfinite/trailing payload");
 const auto ref=d.layout(ContinueLayout::Reference),body=d.layout(ContinueLayout::Body),f=d.layout(ContinueLayout::FadeGeometry);if(!dimensions(ref)||ref.x<=0||ref.y<=0||ref.x>=ref.w||ref.y>=ref.h||d.viewports_[0].width!=ref.x||d.viewports_[1].width!=ref.w||body.x<0||body.w>0||ref.x-body.x+body.w<=0||f.x<=0||f.y<=0||f.w<=0||f.h<=0)return fail("Continue layout geometry rejected");
 for(const auto&a:d.layers_)if(!title_offsets(a.rect,a.viewport_offsets,ref))return fail("Continue title layer viewport offsets rejected");
 for(const auto&a:d.options_)if(!title_offsets(a.rect,a.viewport_offsets,ref))return fail("Continue title option viewport offsets rejected");
 const auto title=d.layout(ContinueLayout::TitleViewportOffsets);const std::array<ContinueViewportOffset,2>title_shift{{{title.x,title.y},{title.w,title.h}}};if(!title_offsets({ref.x/2,ref.y/2,1,1},title_shift,ref))return fail("Continue title center viewport offsets rejected");
 for(const auto&a:d.options_)for(size_t i=0;i<title_shift.size();++i)if(a.viewport_offsets[i].x!=title_shift[i].x||a.viewport_offsets[i].y!=title_shift[i].y)return fail("Continue title option/center offsets disagree");
 d.valid_=true;*this=std::move(d);e.clear();return true;
}
bool ContinueMenuData::load_file(const char*path,std::string&e){if(!path){e="Missing Continue path";return false;}FILE*f=std::fopen(path,"rb");if(!f){e="Could not open Continue pack";return false;}std::vector<uint8_t>b;uint8_t block[4096];while(true){auto n=std::fread(block,1,sizeof(block),f);if(b.size()+n>limit){std::fclose(f);e="Continue file exceeds bound";return false;}b.insert(b.end(),block,block+n);if(n<sizeof(block)){bool bad=std::ferror(f);std::fclose(f);if(bad){e="Continue read error";return false;}break;}}return load(b.data(),b.size(),e);}
}
