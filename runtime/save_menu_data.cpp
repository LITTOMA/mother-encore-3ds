#include "encore/save_menu_data.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
constexpr size_t max_pack_bytes=1024*1024; // Engine allocation bound, not source tuning.
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(int b=0;b<8;++b)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
struct Reader {
 const uint8_t*p;size_t n;bool ok=true;
 uint32_t integer(){if(n<4){ok=false;return 0;}auto x=u32(p);p+=4;n-=4;return x;}
 float real(){uint32_t u=integer();float f;std::memcpy(&f,&u,4);if(!std::isfinite(f)||std::abs(f)>1000000)ok=false;return f;}
 double number(){if(n<8){ok=false;return 0;}uint64_t u=u32(p)|uint64_t(u32(p+4))<<32;p+=8;n-=8;double f;std::memcpy(&f,&u,8);if(!std::isfinite(f))ok=false;return f;}
 std::string text(){auto k=integer();if(k>4096||k>n){ok=false;return {}; }std::string s(reinterpret_cast<const char*>(p),k);p+=k;n-=k;for(unsigned char c:s)if(c<32||c>126)ok=false;return s;}
 uint32_t count(uint32_t max){auto x=integer();if(x>max){ok=false;return 0;}return x;}
 SaveMenuRect rect(){return {real(),real(),real(),real()};}
};
bool path(const std::string&s){if(s.empty()||s.front()=='/'||s.find('\\')!=std::string::npos||s.find(':')!=std::string::npos)return false;size_t p=0;while(p<s.size()){auto e=s.find('/',p);if(e==std::string::npos)e=s.size();auto part=s.substr(p,e-p);if(part.empty()||part=="."||part=="..")return false;p=e+1;}return s.back()!='/';}
}
int SaveMenuData::flavor_index(const std::string&id)const{for(size_t i=0;i<flavors_.size();++i)if(flavors_[i].id==id)return int(i);return -1;}
int SaveMenuData::icon_index(const std::string&id)const{for(size_t i=0;i<icons_.size();++i)if(icons_[i].id==id)return int(i);return -1;}
bool SaveMenuData::supports_text(const std::string&s,bool bottle)const{for(unsigned char c:s){if(bottle){auto it=std::lower_bound(glyphs_.begin(),glyphs_.end(),c,[](const SaveMenuGlyph&g,uint32_t cp){return g.codepoint<cp;});if(it==glyphs_.end()||it->codepoint!=c||it->advance<=0)return false;}else if(!std::binary_search(ebmain_codepoints_.begin(),ebmain_codepoints_.end(),uint32_t(c)))return false;}return true;}
bool SaveMenuData::load(const uint8_t*p,size_t n,std::string&e){
 auto fail=[&](const char*s){e=s;return false;};
 if(!p||n<24||n>max_pack_bytes)return fail("Save menu pack size rejected");
 if(std::memcmp(p,"ENCSMENU",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1)return fail("Save menu schema/size/capability rejected");
 if(crc(p+24,n-24)!=u32(p+16))return fail("Save menu CRC mismatch");
 SaveMenuData d;Reader r{p+24,n-24};d.slots_=r.integer();d.activation_=r.number();d.scroll_=r.number();d.cursor_loop_=r.number();d.arrow_loop_=r.number();d.arrow_move_=r.number();d.spacing_=r.real();d.bottle_height_=r.real();d.ebmain_height_=r.real();d.bottle_spacing_=r.real();
 if(d.slots_<2||d.slots_>256||d.spacing_<=0||d.spacing_>1024||d.bottle_height_<=0||d.bottle_height_>256||d.ebmain_height_<=0||d.ebmain_height_>256)return fail("Save menu count/font/spacing rejected");
 for(double t:{d.activation_,d.scroll_,d.cursor_loop_,d.arrow_loop_,d.arrow_move_})if(t<=0||t>120)return fail("Save menu timing rejected");
 uint32_t count=r.count(64);if(count!=uint32_t(SaveMenuLayout::Count))return fail("Save menu layout schema rejected");for(uint32_t i=0;i<count;++i)d.layouts_.push_back(r.rect());
 count=r.count(64);if(count!=uint32_t(SaveMenuText::Count))return fail("Save menu text schema rejected");for(uint32_t i=0;i<count;++i)d.texts_.push_back(r.text());
 count=r.count(64);if(count!=uint32_t(SaveMenuSound::Count))return fail("Save menu sound schema rejected");for(uint32_t i=0;i<count;++i){auto s=r.text();if(s.empty()||!path(s))return fail("Save menu sound rejected");d.sounds_.push_back(s);}
 count=r.count(64);std::set<std::string>paths;for(uint32_t i=0;i<count;++i){SaveMenuResource a;a.path=r.text();a.width=r.integer();a.height=r.integer();a.columns=r.integer();a.rows=r.integer();if(!path(a.path)||!paths.insert(a.path).second||!a.width||a.width>1024||!a.height||a.height>1024||!a.columns||a.columns>256||!a.rows||a.rows>256||a.width%a.columns||a.height%a.rows)return fail("Save menu texture rejected");d.resources_.push_back(a);}
 d.font_=r.integer();d.outline_=r.integer();d.cursor_=r.integer();d.arrow_=r.integer();d.text_color_=r.integer();d.time_color_=r.integer();d.outline_color_=r.integer();
 for(auto i:{d.font_,d.outline_,d.cursor_,d.arrow_})if(i>=d.resources_.size())return fail("Save menu resource binding rejected");
 const auto&font=d.resources_[d.font_];const auto&outline=d.resources_[d.outline_];if(font.width!=outline.width||font.height!=outline.height||font.columns!=1||font.rows!=1||outline.columns!=1||outline.rows!=1)return fail("Save menu font textures differ");
 count=r.count(128);for(uint32_t i=0;i<count;++i){SaveMenuGlyph g;g.codepoint=r.integer();g.u=r.integer();g.v=r.integer();g.width=r.integer();g.height=r.integer();g.advance=r.real();g.offset_x=r.real();g.offset_y=r.real();if(g.codepoint<32||g.codepoint>126||(i&&g.codepoint<=d.glyphs_.back().codepoint)||g.u>font.width||g.width>font.width-g.u||g.v>font.height||g.height>font.height-g.v||g.advance<0||g.advance>256)return fail("Save menu glyph rejected");d.glyphs_.push_back(g);}
 count=r.count(128);for(uint32_t i=0;i<count;++i){auto cp=r.integer();if(cp<32||cp>126||(i&&cp<=d.ebmain_codepoints_.back()))return fail("Save menu EBMain domain rejected");d.ebmain_codepoints_.push_back(cp);}
 count=r.count(32);std::set<std::string>flavors;for(uint32_t i=0;i<count;++i){SaveMenuFlavor f;f.id=r.text();f.resource=r.integer();f.confirm_resource=r.integer();f.divider_color=r.integer();f.background_color=r.integer();if(f.id.empty()||!flavors.insert(f.id).second||(f.resource>=d.resources_.size()||f.confirm_resource>=d.resources_.size()))return fail("Save menu flavor rejected");d.flavors_.push_back(f);}if(d.flavors_.empty())return fail("Save menu needs default flavor");
 count=r.count(32);std::set<std::string>icons;for(uint32_t i=0;i<count;++i){SaveMenuIcon a;a.id=r.text();a.resource=r.integer();if(a.id.empty()||!icons.insert(a.id).second||a.resource>=d.resources_.size())return fail("Save menu party icon rejected");d.icons_.push_back(a);}if(d.icons_.empty())return fail("Save menu needs party icons");
 count=r.count(256);for(uint32_t i=0;i<count;++i){SaveMenuCursorKey k;k.time=r.number();k.margins=r.rect();if(k.time<0||k.time>=d.cursor_loop_||(i?k.time<=d.cursor_keys_.back().time:k.time!=0))return fail("Save menu cursor animation rejected");d.cursor_keys_.push_back(k);}if(d.cursor_keys_.empty())return fail("Save menu needs cursor animation");
 count=r.count(256);const auto&arrow=d.resources_[d.arrow_];for(uint32_t i=0;i<count;++i){SaveMenuArrowKey k;k.time=r.number();k.frame=r.integer();if(k.time<0||k.time>=d.arrow_loop_||(i?k.time<=d.arrow_keys_.back().time:k.time!=0)||k.frame>=uint64_t(arrow.columns)*arrow.rows)return fail("Save menu arrow animation rejected");d.arrow_keys_.push_back(k);}if(d.arrow_keys_.empty())return fail("Save menu needs arrow animation");
 if(!r.ok||r.n)return fail("Save menu truncated/nonfinite/trailing payload");
 for(auto k:{SaveMenuLayout::Reference,SaveMenuLayout::Card,SaveMenuLayout::Name,SaveMenuLayout::Level,SaveMenuLayout::Title,SaveMenuLayout::Time,SaveMenuLayout::NoData,SaveMenuLayout::NoDataLabel,SaveMenuLayout::FileNumber,SaveMenuLayout::DividerVertical,SaveMenuLayout::DividerHorizontal,SaveMenuLayout::Confirm,SaveMenuLayout::ConfirmText,SaveMenuLayout::Choices}){const auto&a=d.layout(k);if(a.w<=0||a.h<=0||a.w>4096||a.h>4096)return fail("Save menu layout dimensions rejected");}
 for(auto k:{SaveMenuLayout::CardPatch,SaveMenuLayout::CursorPatch,SaveMenuLayout::ConfirmPatch}){const auto&a=d.layout(k);for(float v:{a.x,a.y,a.w,a.h})if(v<0||v>1024||v!=std::floor(v))return fail("Save menu patch margins rejected");}
 auto fits=[&](uint32_t resource,SaveMenuLayout key){auto m=d.layout(key);const auto&a=d.resources_[resource];return m.x+m.w<=a.width&&m.y+m.h<=a.height;};
 if(!fits(d.cursor_,SaveMenuLayout::CursorPatch))return fail("Save menu cursor patch exceeds source");
 for(const auto&f:d.flavors_)if(!fits(f.resource,SaveMenuLayout::CardPatch)||!fits(f.confirm_resource,SaveMenuLayout::ConfirmPatch))return fail("Save menu flavor patch exceeds source");
 for(const auto&s:d.texts_)if(s.empty()||!d.supports_text(s,true))return fail("Save menu unsupported source text");
 if(!d.supports_text("0123456789",true))return fail("Save menu missing numeric glyphs");
 auto tf=d.layout(SaveMenuLayout::TimeFormat);for(float v:{tf.x,tf.y,tf.w,tf.h})if(v<=0||v!=std::floor(v))return fail("Save menu time format rejected");if(tf.w>32||tf.h>32)return fail("Save menu time digit bound rejected");
 d.valid_=true;*this=std::move(d);e.clear();return true;
}
bool SaveMenuData::load_file(const char*path,std::string&e){if(!path){e="Missing save menu path";return false;}FILE*f=std::fopen(path,"rb");if(!f){e="Could not open save menu pack";return false;}std::vector<uint8_t>b;uint8_t block[4096];while(true){size_t n=std::fread(block,1,sizeof(block),f);if(b.size()+n>max_pack_bytes){std::fclose(f);e="Save menu file exceeds bound";return false;}b.insert(b.end(),block,block+n);if(n<sizeof(block)){bool error=std::ferror(f);std::fclose(f);if(error){e="Save menu read error";return false;}break;}}return load(b.data(),b.size(),e);}
}
