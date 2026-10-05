#include "encore/field_sprite_bridge.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
namespace encore::upstream {namespace {
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=i>=16&&i<20?0:p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return~c;}
struct Reader {const uint8_t*p;size_t n,at=64;bool ok=true;
 uint32_t integer(){if(!ok||at>n||n-at<4){ok=false;return 0;}auto v=u32(p+at);at+=4;return v;}
 float scalar(){auto v=integer();float f;std::memcpy(&f,&v,4);if(!std::isfinite(f)||std::abs(f)>1000000)ok=false;return f;}
 Vec2 vec(){return{scalar(),scalar()};}
 std::string text(){auto len=integer();if(!ok||len>8192||at>n||len>n-at){ok=false;return{};}std::string v(reinterpret_cast<const char*>(p+at),len);at+=len;size_t i=0;uint32_t cp;while(i<v.size())if(!encore::utf8_next(v,i,cp)||cp<32){ok=false;break;}return v;}
 bool hash(){if(!ok||at>n||n-at<32){ok=false;return false;}bool any=false;for(unsigned i=0;i<32;++i)any|=p[at+i]!=0;at+=32;ok&=any;return any;}
};
bool path(std::string_view p){return!p.empty()&&p.front()!='/'&&p.back()!='/'&&p.find("..") ==p.npos&&p.find(':')==p.npos&&p.find('\\')==p.npos;}
bool relative(std::string_view p){return!p.empty()&&p.front()!='/'&&p.find(':')==p.npos&&p.find('\\')==p.npos;}
}
const FieldSpriteDescriptor*FieldSpriteData::record(uint32_t id)const{for(const auto&d:records_)if(d.id==id)return&d;return nullptr;}
const FieldSpriteTexture*FieldSpriteData::texture(uint32_t id)const{for(const auto&d:textures_)if(d.id==id)return&d;return nullptr;}
const FieldSpriteAnimation*FieldSpriteData::animation(uint32_t id)const{for(const auto&d:animations_)if(d.id==id)return&d;return nullptr;}
bool FieldSpriteData::load_file(const char*name,std::string&e){if(!name){e="Sprite pack path absent";return false;}std::ifstream f(name,std::ios::binary);if(!f){e="Sprite pack unavailable";return false;}f.seekg(0,std::ios::end);auto n=f.tellg();if(n<64||n>4*1024*1024){e="Sprite pack file size";return false;}f.seekg(0);std::vector<uint8_t>b(static_cast<size_t>(n));if(!f.read(reinterpret_cast<char*>(b.data()),n)){e="Sprite pack file read";return false;}return load(b.data(),b.size(),e);}
bool FieldSpriteData::load(const uint8_t*p,size_t n,std::string&e){
 auto reject=[&](const char*t){e=t;return false;};if(!p||n<64||n>4*1024*1024)return reject("Sprite pack size");if(std::memcmp(p,"ENCSPR01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||u32(p+24)!=1||!u32(p+28)||u32(p+28)>4096||!u32(p+52)||!u32(p+56)||u32(p+56)>512||u32(p+60)||u32(p+16)!=crc(p,n))return reject("Sprite pack header/version/capabilities/rules/CRC");
 FieldSpriteData next;std::copy(p+32,p+52,next.pin_.begin());if(std::all_of(next.pin_.begin(),next.pin_.end(),[](uint8_t b){return!b;}))return reject("Sprite source pin absent");next.scene_=u32(p+52);Reader r{p,n};next.fallback_=r.text();next.reflector_path_=r.text();auto reflect=r.integer();if(!r.ok||next.fallback_.empty()||!path(next.reflector_path_)||reflect>1)return reject("Sprite scene reflection fact");next.reflector_=reflect;auto count=r.integer();if(!count||count>4096)return reject("Sprite texture catalog size");std::set<uint32_t>ids;
 for(uint32_t i=0;i<count;++i){FieldSpriteTexture t;t.id=r.integer();t.width=r.integer();t.height=r.integer();t.source=r.text();if(!r.ok||!t.id||!ids.insert(t.id).second||!t.width||!t.height||t.width>16384||t.height>16384||!path(t.source))return reject("Sprite texture source/extent");next.textures_.push_back(std::move(t));}
 count=r.integer();if(!count||count>1024)return reject("Sprite animation catalog size");ids.clear();
 for(uint32_t i=0;i<count;++i){FieldSpriteAnimation a;a.id=r.integer();a.columns=r.integer();a.rows=r.integer();a.offset=r.vec();a.source=r.text();auto motions=r.integer();if(!r.ok||!a.id||!ids.insert(a.id).second||!a.columns||!a.rows||a.columns>1024||a.rows>1024||!path(a.source)||!motions||motions>128)return reject("Sprite animation/grid/source");std::set<std::string>names;
  for(uint32_t j=0;j<motions;++j){FieldSpriteMotion m;m.name=r.text();auto loop=r.integer(),dirs=r.integer();m.loop=loop;if(!r.ok||m.name.empty()||!names.insert(m.name).second||loop>1||(dirs!=1&&dirs!=2&&dirs!=4&&dirs!=8))return reject("Sprite motion/directions");uint32_t first_keys=0;
   for(uint32_t k=0;k<dirs;++k){FieldSpriteDirection d;d.vector=r.vec();d.duration=r.scalar();auto keys=r.integer();if(!r.ok||d.duration<=0||d.duration>86400||!keys||keys>4096||(k&&keys!=first_keys))return reject("Sprite discrete direction/first frameCount");first_keys=keys;float last=-1;for(uint32_t q=0;q<keys;++q){FieldSpriteKey key;key.time=r.scalar();key.frame=r.integer();if(!r.ok||key.time<0||key.time<last||key.time>=d.duration||key.frame>=a.columns*a.rows)return reject("Sprite discrete key/frame");last=key.time;d.keys.push_back(key);}m.directions.push_back(std::move(d));}a.motions.push_back(std::move(m));
  }next.animations_.push_back(std::move(a));
 }
 ids.clear();uint32_t previous=0;bool had=false;
 for(uint32_t i=0;i<u32(p+28);++i){FieldSpriteDescriptor d;d.id=r.integer();d.parent_id=r.integer();d.ready_ordinal=r.integer();auto kind=r.integer();d.kind=FieldSpriteKind(kind);d.flags=r.integer();d.target_id=r.integer();d.columns=r.integer();d.rows=r.integer();d.frame=r.integer();d.texture=r.integer();d.sprite=r.integer();d.initial_animation=r.integer();d.setup_texture=r.integer();d.setup_animation=r.integer();d.offset=r.vec();d.extra_offset=r.vec();d.reflect_offset=r.scalar();d.node=r.text();d.target_path=r.text();auto transitions=r.integer();
  if(!r.ok||!d.id||!d.parent_id||d.id==UINT32_MAX||!ids.insert(d.id).second||kind<1||kind>2||d.flags>3||!path(d.node)||d.ready_ordinal==UINT32_MAX||(had&&d.ready_ordinal<=previous)||transitions>128)return reject("Sprite descriptor/Ready/flags");previous=d.ready_ordinal;had=true;
  for(uint32_t j=0;j<transitions;++j){FieldSpriteConnection c;c.from=r.text();c.to=r.text();c.mode=r.integer();if(!r.ok||c.from.empty()||c.to.empty()||c.mode>2)return reject("Sprite transition mode/source");d.connections.push_back(std::move(c));}
  if(kind==1){if(!d.columns||!d.rows||d.columns>1024||d.rows>1024||d.frame>=d.columns*d.rows||!next.texture(d.texture)||(d.sprite&&!next.texture(d.sprite))||(d.initial_animation&&!next.animation(d.initial_animation))||!next.texture(d.setup_texture)||!next.animation(d.setup_animation)||d.target_id||!d.target_path.empty()||d.reflect_offset)return reject("CharacterSprite checked resource binding");
   for(auto pair:{std::pair<uint32_t,uint32_t>{d.setup_texture,d.setup_animation},{d.sprite,d.initial_animation}}){if(!pair.second||!pair.first)continue;auto*t=next.texture(pair.first);auto*a=next.animation(pair.second);if(t->width%a->columns||t->height%a->rows)return reject("CharacterSprite texture/grid geometry");}
  }else if(!d.target_id||!relative(d.target_path)||d.columns||d.rows||d.frame||d.texture||d.sprite||d.initial_animation||d.setup_texture||d.setup_animation||!d.connections.empty()||d.offset.x||d.offset.y||d.extra_offset.x||d.extra_offset.y)return reject("Fetcher checked nullable target binding");next.records_.push_back(std::move(d));
 }
 auto sources=r.integer();if(sources!=u32(p+56))return reject("Sprite source receipt count");std::set<std::string>names;for(uint32_t i=0;i<sources;++i){auto s=r.text();if(!r.ok||!path(s)||!names.insert(s).second||!r.hash())return reject("Sprite source closure/hash");}if(!r.ok||r.at!=n)return reject("Sprite trailing data");for(const auto&t:next.textures_)if(!names.count(t.source))return reject("Sprite texture outside source closure");for(const auto&a:next.animations_)if(!names.count(a.source))return reject("Sprite animation outside source closure");next.valid_=true;*this=std::move(next);e.clear();return true;
}
}
