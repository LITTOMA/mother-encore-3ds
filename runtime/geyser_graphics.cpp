#include "encore/geyser_graphics.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
bool finite(float v){return std::isfinite(v)&&std::abs(v)<=1e6;}
bool safe(const std::string&s){
 if(s.empty()||s.size()>512||s.front()=='/'||s.back()=='/')return false;size_t first=0;
 for(size_t n=0;n<=s.size();++n){if(n<s.size()&&(static_cast<unsigned char>(s[n])<32||static_cast<unsigned char>(s[n])>=127||s[n]==':'||s[n]=='\\'))return false;if(n==s.size()||s[n]=='/'){const auto part=s.substr(first,n-first);if(part.empty()||part=="."||part=="..")return false;first=n+1;}}
 return s.rfind("graphics/",0)==0&&s.size()>4&&s.compare(s.size()-4,4,".t3x")==0;
}
}
struct GeyserGraphicsData::State {GeyserGraphicsConfig config;GeyserTimelineData timeline;std::vector<GeyserSpriteFrame>frames;};
const GeyserGraphicsConfig&GeyserGraphicsData::config()const{static const GeyserGraphicsConfig empty;return state_?state_->config:empty;}
const GeyserTimelineData&GeyserGraphicsData::timeline()const{static const GeyserTimelineData empty;return state_?state_->timeline:empty;}
const std::vector<GeyserSpriteFrame>&GeyserGraphicsData::frames()const{static const std::vector<GeyserSpriteFrame>empty;return state_?state_->frames:empty;}
bool GeyserGraphicsData::matches(const GeyserTimelineData&source)const{return valid()&&state_->timeline.same_content(source);}
bool GeyserGraphicsData::load_file(const char*path,std::string&e){if(valid()||!path||!*path)return bad(e,"Geyser graphics fresh owner/path required");std::vector<uint8_t>bytes;return encore::read_file(path,bytes,262144,e)&&load(bytes.data(),bytes.size(),e);}
bool GeyserGraphicsData::load(const uint8_t*p,size_t size,std::string&e){
 if(valid()||!p||size<128||size>262144)return bad(e,"Geyser graphics fresh owner/size rejected");
 if(std::memcmp(p,"ENCGFX01",8)||u(p+8)!=1||u(p+12)!=size||u(p+20)!=1||u(p+24)||u(p+28)||u(p+52)!=4||u(p+56)||u(p+60))return bad(e,"Geyser graphics format/capability/scene rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned n=0;n<20;++n){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+n]!=uint8_t(hex(pin[2*n])*16+hex(pin[2*n+1])))return bad(e,"Geyser graphics source pin rejected");}
 if(encore::crc32(p+32,size-32)!=u(p+16))return bad(e,"Geyser graphics checksum rejected");const uint32_t strides[]={1,32,1,32},limits[]={512,1,65536,256};uint32_t off[4]{},count[4]{};size_t cursor=128;
 for(unsigned n=0;n<4;++n){const auto q=p+64+16*n;off[n]=u(q+4);count[n]=u(q+8);if(u(q)!=n+1||u(q+12)!=strides[n]||!count[n]||count[n]>limits[n]||off[n]<cursor||off[n]%4||off[n]>size||count[n]>(size-off[n])/strides[n])return bad(e,"Geyser graphics directory bounds/stride rejected");for(size_t at=cursor;at<off[n];++at)if(p[at])return bad(e,"Geyser graphics padding rejected");cursor=off[n]+size_t(count[n])*strides[n];}
 if(cursor!=size||count[2]<160)return bad(e,"Geyser graphics exact coverage rejected");auto next=std::make_shared<State>();auto&c=next->config;const auto q=p+off[1];if(u(q)||u(q+4)!=count[0]||u(q+28))return bad(e,"Geyser graphics text/reserved coverage rejected");c.texture.assign(reinterpret_cast<const char*>(p+off[0]),count[0]);c.width=u(q+8);c.height=u(q+12);c.texture_bytes=u(q+16);c.texture_crc=u(q+20);c.flags=u(q+24);
 if(!safe(c.texture)||!c.width||c.width>1024||!c.height||c.height>1024||!c.texture_bytes||c.texture_bytes>8*1024*1024||c.flags>7)return bad(e,"Geyser graphics texture dimensions/flags rejected");if(!next->timeline.load(p+off[2],count[2],e))return false;const auto&source=next->timeline.policy();if(count[3]!=source.frames)return bad(e,"Geyser graphics timeline frame coverage rejected");
 for(uint32_t n=0;n<count[3];++n){const auto f=p+off[3]+32*n;GeyserSpriteFrame row{u(f),u(f+4),u(f+8),u(f+12),u(f+16)};if(row.index!=n||row.x>=c.width||row.y>=c.height||!row.width||!row.height||row.width>c.width-row.x||row.height>c.height-row.y||row.width!=source.size.x||row.height!=source.size.y||u(f+20)||u(f+24)||u(f+28))return bad(e,"Geyser graphics source frame identity/bounds rejected");next->frames.push_back(row);}
 state_=std::move(next);e.clear();return true;
}
bool GeyserGraphicsData::texture_matches(const uint8_t*p,size_t size,std::string&e)const{if(!valid()||!p||size!=config().texture_bytes||encore::crc32(p,size)!=config().texture_crc)return bad(e,"Geyser graphics texture size/checksum rejected");e.clear();return true;}
bool GeyserGraphicsData::compose(const GeyserTimeline&actor,uint64_t epoch,uint64_t instance,Vec2 root,Vec2 camera,bool visible,GeyserSpriteDraw&out,std::string&e)const{
 GeyserSnapshot pose;if(!valid()||!matches(actor.content())||!actor.inspect(epoch,instance,pose,e)||pose.frame>=frames().size()||!finite(root.x)||!finite(root.y)||!finite(camera.x)||!finite(camera.y))return bad(e,"Geyser graphics source/actor/position owner rejected");const auto&policy=timeline().policy();const auto&frame=frames()[pose.frame];Vec2 offset=policy.offset;if(policy.centered){offset.x-=frame.width*.5f;offset.y-=frame.height*.5f;}if(config().flags&1){offset.x=std::floor(offset.x);offset.y=std::floor(offset.y);}GeyserSpriteDraw next{frame,{root.x+policy.sprite.x+offset.x-camera.x,root.y+policy.sprite.y+offset.y-camera.y},visible,bool(config().flags&2),bool(config().flags&4)};if(!finite(next.position.x)||!finite(next.position.y))return bad(e,"Geyser graphics output geometry rejected");out=next;e.clear();return true;
}
}
