#include "encore/shadow_graphics.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
namespace encore::upstream {namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){const auto n=word(p);float value;std::memcpy(&value,&n,4);return value;}
bool fail(std::string&e,const char*s){e=s;return false;}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=1000000&&std::abs(p.y)<=1000000;}
bool path(const std::string&s){if(s.empty()||s.size()>512||s.back()=='/'||s.substr(s.size()>4?s.size()-4:0)!=".t3x")return false;size_t start=0;for(size_t i=0;i<=s.size();++i){if(i==s.size()||s[i]=='/'){const auto part=s.substr(start,i-start);if(part.empty()||part=="."||part=="..")return false;start=i+1;}else if(uint8_t(s[i])<32||uint8_t(s[i])>126||s[i]==':'||s[i]=='\\')return false;}return true;}
bool identifier(const std::string&s){if(s.empty()||s.size()>32||s[0]<'a'||s[0]>'z')return false;for(auto c:s)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;return true;}
}
struct ShadowGraphicsData::State {ShadowGraphicsConfig config;std::vector<ShadowGraphicsMode>modes;std::vector<ShadowGraphicsFrame>frames;};
const ShadowGraphicsConfig&ShadowGraphicsData::config()const{static const ShadowGraphicsConfig empty;return state_?state_->config:empty;}
const std::vector<ShadowGraphicsMode>&ShadowGraphicsData::modes()const{static const std::vector<ShadowGraphicsMode>empty;return state_?state_->modes:empty;}
const std::vector<ShadowGraphicsFrame>&ShadowGraphicsData::frames()const{static const std::vector<ShadowGraphicsFrame>empty;return state_?state_->frames:empty;}
const ShadowGraphicsFrame*ShadowGraphicsData::frame(uint32_t mode,uint32_t index)const{if(!state_||mode>=modes().size()||index>=modes()[mode].count)return nullptr;return &frames()[modes()[mode].first+index];}
bool ShadowGraphicsData::load_file(const char*path,std::string&e){std::vector<uint8_t>b;return encore::read_file(path,b,1048576,e)&&load(b.data(),b.size(),e);}
bool ShadowGraphicsData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<128||n>1048576||std::memcmp(p,"ENCSHDW1",8)||word(p+8)!=1||word(p+12)!=n||word(p+20)!=1||!word(p+24)||word(p+24)==UINT32_MAX||word(p+28)||word(p+52)!=4||word(p+56)||word(p+60))return fail(e,"Shadow graphics header/version/capability rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};for(size_t i=0;i<20;++i)if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail(e,"Shadow graphics source pin rejected");
 if(encore::crc32(p+32,n-32)!=word(p+16))return fail(e,"Shadow graphics CRC rejected");
 const uint32_t strides[]={1,64,32,32},limits[]={32768,1,16,1024};std::array<uint32_t,4>offsets{},counts{};size_t cursor=128;
 for(uint32_t i=0;i<4;++i){const auto*d=p+64+i*16;const auto off=word(d+4),count=word(d+8);if(word(d)!=i+1||word(d+12)!=strides[i]||!count||count>limits[i]||off%4||off<cursor||off>n||count>(n-off)/strides[i])return fail(e,"Shadow graphics directory/range rejected");for(size_t at=cursor;at<off;++at)if(p[at])return fail(e,"Shadow graphics padding rejected");cursor=size_t(off)+size_t(count)*strides[i];offsets[i]=off;counts[i]=count;}
 if(cursor!=n)return fail(e,"Shadow graphics trailing bytes rejected");auto next=std::make_shared<State>();auto&c=next->config;std::vector<uint8_t>owned(counts[0],0);
 auto text=[&](const uint8_t*ref,std::string&out){const auto first=word(ref),count=word(ref+4);if(!count||first>counts[0]||count>counts[0]-first)return false;out.assign(reinterpret_cast<const char*>(p+offsets[0]+first),count);std::fill(owned.begin()+first,owned.begin()+first+count,1);return true;};
 const auto*config=p+offsets[1];c.owner=word(p+24);c.width=word(config+8);c.height=word(config+12);c.texture_bytes=word(config+16);c.texture_crc=word(config+20);c.position={real(config+24),real(config+28)};c.offset={real(config+32),real(config+36)};c.flags=word(config+40);c.initial=word(config+44);c.playing=word(config+48)!=0;
 if(!text(config,c.texture)||!path(c.texture)||!c.width||c.width>1024||!c.height||c.height>1024||!c.texture_bytes||c.texture_bytes>8*1024*1024||!finite(c.position)||!finite(c.offset)||(c.flags&~31u)||c.initial>=counts[2]||word(config+48)>1||word(config+52)||word(config+56)||word(config+60))return fail(e,"Shadow graphics texture/geometry/policy rejected");
 uint32_t frame_cursor=0;std::set<std::string>names;
 for(uint32_t i=0;i<counts[2];++i){const auto*r=p+offsets[2]+i*32;ShadowGraphicsMode m;m.fps=real(r+8);m.first=word(r+12);m.count=word(r+16);const auto flags=word(r+20);m.front=flags&1;m.loop=flags&2;
  if(!text(r,m.name)||!identifier(m.name)||!names.insert(m.name).second||!std::isfinite(m.fps)||m.fps<=0||m.fps>60||1.0/double(m.fps)>std::numeric_limits<float>::max()||m.first!=frame_cursor||!m.count||m.count>64||m.count>counts[3]-frame_cursor||(flags&~3u)||word(r+24)||word(r+28))return fail(e,"Shadow graphics mode/clock/span rejected");
  for(uint32_t j=0;j<m.count;++j){const auto*f=p+offsets[3]+(frame_cursor+j)*32;ShadowGraphicsFrame frame{word(f),word(f+4),word(f+8),word(f+12),word(f+16),word(f+20)};
   if(frame.mode!=i||frame.index!=j||frame.x>=c.width||frame.y>=c.height||!frame.width||!frame.height||frame.width>c.width-frame.x||frame.height>c.height-frame.y||word(f+24)||word(f+28))return fail(e,"Shadow graphics frame owner/rectangle rejected");next->frames.push_back(frame);
  }frame_cursor+=m.count;next->modes.push_back(std::move(m));
 }
 if(frame_cursor!=counts[3]||std::find(owned.begin(),owned.end(),0)!=owned.end())return fail(e,"Shadow graphics orphaned frames/text rejected");state_=std::move(next);e.clear();return true;
}
bool ShadowGraphicsData::matches(const FieldStepShadowState&shadow)const{
 if(!state_||!shadow.prepared())return false;const auto data=shadow.content();if(data.capability()!=2||data.shadow_count()!=modes().size()||data.shadow_initial_playing()!=config().playing||data.text(data.policy().shadow)!=modes()[config().initial].name)return false;
 for(uint32_t i=0;i<modes().size();++i){const auto&mode=modes()[i];const auto source=data.shadow(i);if(data.text(source.name)!=mode.name||source.fps!=mode.fps||source.frames!=mode.count||source.front!=mode.front||source.loop!=mode.loop)return false;}return frame(shadow.animation(),shadow.frame())!=nullptr;
}
bool ShadowGraphicsData::texture_matches(const uint8_t*p,size_t n,std::string&e)const{if(!state_||!p||n!=config().texture_bytes||encore::crc32(p,n)!=config().texture_crc)return fail(e,"Shadow texture byte identity rejected");e.clear();return true;}
bool ShadowGraphicsData::compose(const FieldStepShadowState&shadow,uint32_t profile,Vec2 body,Vec2 camera,bool visible,ShadowDraw&out,std::string&e)const{
 if(!matches(shadow)||profile!=config().owner||!finite(body)||!finite(camera))return fail(e,"Shadow composition source/profile/geometry rejected");const auto&c=config();const auto*source=frame(shadow.animation(),shadow.frame());ShadowDraw next;next.source=*source;next.visible=visible&&(c.flags&8);next.behind=shadow.behind();next.flip_h=c.flags&2;next.flip_v=c.flags&4;auto local=c.offset;
 if(c.flags&1){local.x-=source->width*.5f;local.y-=source->height*.5f;}if(c.flags&16){local.x=std::floor(local.x);local.y=std::floor(local.y);}next.position={body.x+c.position.x+local.x-camera.x,body.y+c.position.y+local.y-camera.y};if(!std::isfinite(next.position.x)||!std::isfinite(next.position.y))return fail(e,"Shadow composition coordinate overflow");out=next;e.clear();return true;
}
}
