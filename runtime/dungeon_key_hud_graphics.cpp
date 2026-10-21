#include "encore/dungeon_key_hud_graphics.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace encore::upstream {namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){auto u=word(p);float v;std::memcpy(&v,&u,4);return v;}
bool bad(std::string&e,const char*s){e=s;return false;}
bool path(const std::string&s){if(s.empty()||s.size()>512||s.front()=='/'||s.back()=='/')return false;size_t start=0;for(size_t i=0;i<=s.size();++i){if(i==s.size()||s[i]=='/'){const auto part=s.substr(start,i-start);if(part.empty()||part=="."||part=="..")return false;start=i+1;}else if(uint8_t(s[i])<32||uint8_t(s[i])>=127||s[i]=='\\'||s[i]==':')return false;}return true;}
}
struct DungeonKeyHudGraphicsData::State {DungeonKeyIndicatorData indicator;KeyHudGraphicsConfig config;};
const KeyHudGraphicsConfig&DungeonKeyHudGraphicsData::config()const{static const KeyHudGraphicsConfig empty;return state_?state_->config:empty;}
const DungeonKeyIndicatorData&DungeonKeyHudGraphicsData::indicator()const{static const DungeonKeyIndicatorData empty;return state_?state_->indicator:empty;}
bool DungeonKeyHudGraphicsData::matches(const DungeonKeyIndicator&hud)const{return state_&&hud.epoch()&&state_->indicator.same_contract(hud.data());}
bool DungeonKeyHudGraphicsData::load_file(const char*name,std::string&e){std::vector<uint8_t>b;return encore::read_file(name,b,1024*1024,e)&&load(b.data(),b.size(),e);}
bool DungeonKeyHudGraphicsData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<128||n>1024*1024||std::memcmp(p,"ENCKGFX1",8)||word(p+8)!=1||word(p+12)!=n||word(p+16)!=encore::crc32(p+32,n-32)||word(p+20)!=1||word(p+24)!=128||word(p+28))return bad(e,"Key HUD graphics header/version/capability/CRC rejected");
 const auto at=word(p+32),amount=word(p+36),text=word(p+40),length=word(p+44);
 if(at!=128||amount<176||amount>n-128||text!=128+amount||length>512||uint64_t(text)+length!=n||word(p+108)||std::any_of(p+112,p+128,[](uint8_t v){return v!=0;}))return bad(e,"Key HUD graphics canonical sections/reserved fields rejected");
 auto next=std::make_shared<State>();if(!next->indicator.load(p+at,amount,e))return false;auto&c=next->config;
 c.texture.assign(reinterpret_cast<const char*>(p+text),length);if(!path(c.texture))return bad(e,"Key HUD graphics unsafe texture path rejected");
 c.width=word(p+48);c.height=word(p+52);c.texture_bytes=word(p+56);c.texture_crc=word(p+60);c.font_bytes=word(p+64);c.font_crc=word(p+68);c.color=word(p+104);
 if(!c.width||c.width>1024||!c.height||c.height>1024||!c.texture_bytes||c.texture_bytes>8*1024*1024||c.font_bytes<24||c.font_bytes>1024*1024)return bad(e,"Key HUD graphics image/font bounds rejected");
 c.reference={real(p+72),real(p+76)};c.expanded={real(p+80),real(p+84)};c.icon_offset={real(p+88),real(p+92)};c.text_offset={real(p+96),real(p+100)};
 for(float v:{c.reference.x,c.reference.y,c.expanded.x,c.expanded.y,c.icon_offset.x,c.icon_offset.y,c.text_offset.x,c.text_offset.y})if(!std::isfinite(v)||std::abs(v)>65535)return bad(e,"Key HUD graphics nonfinite/unbounded geometry rejected");
 if(c.reference.x<=0||c.reference.y<=0||c.expanded.x<c.reference.x||c.expanded.y<c.reference.y||c.expanded.x>1024||c.expanded.y>1024||(c.reference.x==c.expanded.x&&c.reference.y==c.expanded.y))return bad(e,"Key HUD graphics viewport pair rejected");
 state_=std::move(next);e.clear();return true;
}
bool DungeonKeyHudGraphicsData::texture_matches(const uint8_t*p,size_t n,std::string&e)const{if(!state_||!p||n!=config().texture_bytes||encore::crc32(p,n)!=config().texture_crc)return bad(e,"Key HUD graphics icon byte identity rejected");e.clear();return true;}
bool DungeonKeyHudGraphicsData::compose(const DungeonKeyIndicator&hud,float w,float h,KeyHudDraw&out,std::string&e)const{
 if(!matches(hud))return bad(e,"Key HUD graphics state contract/epoch rejected");const auto&c=config();
 if(!((w==c.reference.x&&h==c.reference.y)||(w==c.expanded.x&&h==c.expanded.y)))return bad(e,"Key HUD graphics unreviewed viewport rejected");
 const auto&v=hud.view();KeyHudDraw next;next.visible=v.visible;next.label=v.text;next.icon={v.position.x+w-c.reference.x+c.icon_offset.x,v.position.y+h-c.reference.y+c.icon_offset.y};next.text={v.position.x+w-c.reference.x+c.text_offset.x,v.position.y+h-c.reference.y+c.text_offset.y};
 for(float q:{next.icon.x,next.icon.y,next.text.x,next.text.y})if(!std::isfinite(q))return bad(e,"Key HUD graphics composed coordinate overflow");out=std::move(next);e.clear();return true;
}
}
