#include "encore/startup_settings.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
uint32_t integer(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(int j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
bool fail(std::string&e,const char*s){e=s;return false;}
bool path(const std::string&s){return !s.empty()&&s.front()!='/'&&s.find("..") == std::string::npos&&s.find(':')==std::string::npos&&s.find('\\')==std::string::npos;}
struct Reader{
 const uint8_t*p;size_t n;bool ok=true;
 uint32_t u32(){if(n<4){ok=false;return 0;}auto v=integer(p);p+=4;n-=4;return v;}
 uint32_t count(uint32_t max){auto v=u32();if(v>max){ok=false;return 0;}return v;}
 float real(){auto u=u32();float f;std::memcpy(&f,&u,4);if(!std::isfinite(f))ok=false;return f;}
 double number(){if(n<8){ok=false;return 0;}uint64_t u=uint64_t(integer(p))|uint64_t(integer(p+4))<<32;double f;std::memcpy(&f,&u,8);p+=8;n-=8;if(!std::isfinite(f))ok=false;return f;}
 std::string text(){auto k=count(1024);if(k>n){ok=false;return{};}std::string s(reinterpret_cast<const char*>(p),k);p+=k;n-=k;for(unsigned char c:s)if(c<32||c>126)ok=false;return s;}
 std::vector<std::string>texts(){std::vector<std::string>v;auto k=count(128);for(uint32_t i=0;i<k;++i)v.push_back(text());return v;}
 SettingsRect rect(){SettingsRect q{real(),real(),real(),real()};if(q.x<0||q.y<0||q.w<0||q.h<0||q.x>400||q.y>240||q.w>400||q.h>240)ok=false;return q;}
 SettingsLabel label(){auto s=text();return {std::move(s),rect()};}
};
template<typename T>int index(const std::vector<T>&v,const T&x){auto it=std::find(v.begin(),v.end(),x);return it==v.end()?-1:int(it-v.begin());}
bool unique(const std::vector<std::string>&v){return !v.empty()&&std::set<std::string>(v.begin(),v.end()).size()==v.size()&&std::none_of(v.begin(),v.end(),[](const auto&s){return s.empty();});}
}
int StartupSettingsData::speed_index(double v)const{return index(speeds,v);}int StartupSettingsData::flavor_index(const std::string&v)const{return index(flavors,v);}int StartupSettingsData::prompt_index(const std::string&v)const{return index(prompts,v);}
SessionSettings StartupSettingsData::defaults()const{SessionSettings s;if(valid_){s.text_speed=speeds[default_indices[0]];s.menu_flavor=flavors[default_indices[1]];s.button_prompts=prompts[default_indices[2]];s.description=description;}return s;}
bool StartupSettingsData::supports(const SessionSettings&s)const{return valid_&&speed_index(s.text_speed)>=0&&flavor_index(s.menu_flavor)>=0&&prompt_index(s.button_prompts)>=0&&s.description==description;}
bool StartupSettingsData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<24||n>1024*1024||std::memcmp(p,"ENCSETUI",8)||integer(p+8)!=1||integer(p+12)!=n||integer(p+20)!=1||crc(p+24,n-24)!=integer(p+16))return fail(e,"Startup settings schema/size/CRC rejected");
 StartupSettingsData d;Reader r{p+24,n-24};auto count=r.count(16);for(uint32_t i=0;i<count;++i){double v=r.number();if(v<=0||v>1||index(d.speeds,v)>=0)return fail(e,"Startup text speed choice rejected");d.speeds.push_back(v);}
 d.flavors=r.texts();d.prompts=r.texts();d.speed_labels=r.texts();d.flavor_labels=r.texts();d.prompt_labels=r.texts();for(auto&v:d.default_indices)v=r.u32();auto description=r.u32();d.description=description!=0;d.text_color=r.u32();for(auto&v:d.patch)v=r.u32();d.settings_box=r.rect();d.confirmation_settings_box=r.rect();for(auto&v:d.confirmation_row_offset)v=r.real();count=r.count(16);for(uint32_t i=0;i<count;++i)d.rows.push_back({r.text(),r.rect(),r.rect()});
 count=r.count(16);for(uint32_t i=0;i<count;++i){SettingsPanel panel;panel.box=r.rect();auto labels=r.count(32);for(uint32_t j=0;j<labels;++j)panel.labels.push_back(r.label());d.panels.push_back(std::move(panel));}
 count=r.count(32);for(uint32_t i=0;i<count;++i){SettingsResource a{r.text(),r.u32(),r.u32(),r.u32(),r.u32()};if(!path(a.path)||!a.width||!a.height||a.width>1024||a.height>1024||!a.columns||!a.rows||a.width%a.columns||a.height%a.rows)return fail(e,"Startup settings texture rejected");d.resources.push_back(std::move(a));}
 d.box_resource=r.u32();d.card_resource=r.u32();d.inside_resource=r.u32();count=r.count(16);for(uint32_t i=0;i<count;++i)d.confirmation_fields.push_back({r.rect(),r.rect(),r.rect(),r.rect(),r.u32()});d.confirmation_box=r.rect();d.certainty=r.label();count=r.count(8);for(uint32_t i=0;i<count;++i)d.confirmation_choices.push_back(r.label());d.palette_threshold=r.number();for(auto&v:d.source_palette)v=r.u32();count=r.count(32);for(uint32_t i=0;i<count;++i){std::array<uint32_t,8>a;for(auto&v:a)v=r.u32();d.palettes.push_back(a);}d.skin_paths=r.texts();
 if(!r.ok||r.n||description>1||d.palette_threshold<=0||d.palette_threshold>1||d.speeds.size()!=d.speed_labels.size()||!unique(d.flavors)||!unique(d.prompts)||d.flavors.size()!=d.flavor_labels.size()||d.prompts.size()!=d.prompt_labels.size()||d.flavors.size()!=d.palettes.size()||d.default_indices[0]>=d.speeds.size()||d.default_indices[1]>=d.flavors.size()||d.default_indices[2]>=d.prompts.size()||d.rows.size()!=4||d.panels.size()!=3||d.confirmation_choices.size()!=2||d.confirmation_fields.size()!=6||d.box_resource>=d.resources.size()||d.card_resource>=d.resources.size()||d.inside_resource>=d.resources.size())return fail(e,"Startup settings malformed structure rejected");
 const size_t sizes[]={d.speeds.size(),d.flavors.size(),d.prompts.size()};for(size_t i=0;i<3;++i)if(d.panels[i].labels.size()!=sizes[i])return fail(e,"Startup panel option topology rejected");
 for(const auto&f:d.confirmation_fields)if(f.resource>=d.resources.size())return fail(e,"Startup confirmation resource rejected");for(auto p:d.patch)if(p>64)return fail(e,"Startup skin margin rejected");if(!unique(d.skin_paths))return fail(e,"Startup duplicate UI skin identity");for(const auto&s:d.skin_paths)if(!path(s))return fail(e,"Startup UI skin path rejected");
 d.valid_=true;*this=std::move(d);e.clear();return true;
}
bool StartupSettingsData::load_file(const char*path,std::string&e){
 if(!path)return fail(e,"Missing startup settings path");FILE*f=std::fopen(path,"rb");if(!f)return fail(e,"Cannot open startup settings pack");std::vector<uint8_t>b;uint8_t chunk[4096];bool ok=true;while(true){size_t n=std::fread(chunk,1,sizeof(chunk),f);if(b.size()+n>1024*1024){ok=false;break;}b.insert(b.end(),chunk,chunk+n);if(n<sizeof(chunk)){ok=!std::ferror(f);break;}}if(std::fclose(f))ok=false;return ok?load(b.data(),b.size(),e):fail(e,"Startup settings bounded read failed");
}
}
