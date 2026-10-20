#include "encore/dungeon_mechanism_presentation.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){const auto bits=u32(p);float v;std::memcpy(&v,&bits,4);return v;}
Vec2 vec(const uint8_t*p){return {real(p),real(p+4)};}
bool finite(Vec2 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::abs(v.x)<=1000000&&std::abs(v.y)<=1000000;}
bool path(const std::string&s){
 if(s.empty()||s.size()>512||s.front()=='/'||s.back()=='/')return false;
 size_t first=0;
 for(size_t i=0;i<=s.size();++i){
  if(i<s.size()&&(static_cast<unsigned char>(s[i])<32||static_cast<unsigned char>(s[i])>126||s[i]=='\\'||s[i]==':'))return false;
  if(i==s.size()||s[i]=='/'){if(i==first||s.substr(first,i-first)=="."||s.substr(first,i-first)=="..")return false;first=i+1;}
 }
 return true;
}
bool ends(const std::string&s,const char*t){const size_t n=std::strlen(t);return s.size()>=n&&s.compare(s.size()-n,n,t)==0;}
}
struct DungeonMechanismPresentationData::State {
 DungeonMechanismBindingsData bindings;std::vector<DungeonMechanismSprite>sprites;
 std::vector<DungeonMechanismPlacement>owners;std::vector<DungeonMechanismShape>shapes;
 uint32_t capability=0,bytes=0,crc=0;AudioBank audio;std::vector<DungeonMechanismSound>sounds;
};
uint32_t DungeonMechanismPresentationData::capability()const{return state_?state_->capability:0;}
uint32_t DungeonMechanismPresentationData::encoded_bytes()const{return state_?state_->bytes:0;}
uint32_t DungeonMechanismPresentationData::encoded_crc()const{return state_?state_->crc:0;}
const DungeonMechanismBindingsData&DungeonMechanismPresentationData::bindings()const{static const DungeonMechanismBindingsData empty;return state_?state_->bindings:empty;}
const std::vector<DungeonMechanismSprite>&DungeonMechanismPresentationData::sprites()const{static const std::vector<DungeonMechanismSprite>empty;return state_?state_->sprites:empty;}
const std::vector<DungeonMechanismPlacement>&DungeonMechanismPresentationData::owners()const{static const std::vector<DungeonMechanismPlacement>empty;return state_?state_->owners:empty;}
const std::vector<DungeonMechanismShape>&DungeonMechanismPresentationData::shapes()const{static const std::vector<DungeonMechanismShape>empty;return state_?state_->shapes:empty;}
const std::vector<DungeonMechanismSound>&DungeonMechanismPresentationData::sounds()const{static const std::vector<DungeonMechanismSound>empty;return state_?state_->sounds:empty;}
const AudioBank&DungeonMechanismPresentationData::audio_bank()const{static const AudioBank empty;return state_?state_->audio:empty;}
const DungeonMechanismSound*DungeonMechanismPresentationData::sound(uint32_t id)const{for(const auto&s:sounds())if(s.owner==id)return &s;return nullptr;}
const DungeonMechanismSprite*DungeonMechanismPresentationData::sprite(uint32_t id)const{for(const auto&s:sprites())if(s.id==id)return &s;return nullptr;}
bool DungeonMechanismPresentationData::load_file(const char*path,std::string&e){
 if(state_||!path)return bad(e,"Mechanism presentation requires a fresh owner and file path");
 std::vector<uint8_t>b;return encore::read_file(path,b,512*1024,e)&&load(b.data(),b.size(),e);
}
bool DungeonMechanismPresentationData::load(const uint8_t*p,size_t n,std::string&e){
 if(state_||!p||n<144||n>512*1024)return bad(e,"Mechanism presentation size/owner rejected");
 const uint32_t capability=u32(p+20),sections=capability==2?7u:5u;
 if(std::memcmp(p,"ENCDMP01",8)||u32(p+8)!=1||u32(p+12)!=n||(capability!=1&&capability!=2)||!u32(p+24)||u32(p+24)>65535||u32(p+28)||u32(p+52)!=sections||u32(p+56)||u32(p+60)||n<64+sections*16)return bad(e,"Mechanism presentation schema/capability/reserved rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return bad(e,"Mechanism presentation source pin rejected");}
 if(encore::crc32(p+32,n-32)!=u32(p+16))return bad(e,"Mechanism presentation checksum rejected");
 const uint32_t strides[]={1,112,40,64,1,88,1},limits[]={65536,64,256,1024,256*1024,257,65536};uint32_t off[7]{},count[7]{};size_t cursor=64+sections*16;
 for(unsigned i=0;i<sections;++i){const auto*d=p+64+i*16;off[i]=u32(d+4);count[i]=u32(d+8);
  if(u32(d)!=i+1||u32(d+12)!=strides[i]||!count[i]||count[i]>limits[i]||off[i]%4||off[i]<cursor||off[i]>n||count[i]>(n-off[i])/strides[i])return bad(e,"Mechanism presentation section span rejected");
  for(size_t j=cursor;j<off[i];++j)if(p[j])return bad(e,"Mechanism presentation noncanonical padding");cursor=off[i]+count[i]*strides[i];
 }
 if(cursor!=n)return bad(e,"Mechanism presentation trailing bytes rejected");
 auto v=std::make_shared<State>();v->capability=capability;v->bytes=uint32_t(n);v->crc=u32(p+16);if(!v->bindings.load(p+off[4],count[4],e))return false;
 const auto g=v->bindings.graph();if(g.scene_id()!=u32(p+24)||count[2]!=v->bindings.bindings().size())return bad(e,"Mechanism presentation nested scene/owner coverage rejected");
 std::vector<uint8_t>owned(count[0],0);
 auto text=[&](const uint8_t*q,std::string&out){const auto first=u32(q),size=u32(q+4);if(!size||first>count[0]||size>count[0]-first)return false;
  for(uint32_t i=first;i<first+size;++i)if(owned[i])return false;
  out.assign(reinterpret_cast<const char*>(p+off[0]+first),size);if(!path(out))return false;
  std::fill(owned.begin()+first,owned.begin()+first+size,1);return true;
 };
 std::set<uint32_t>ids;std::set<std::string>textures;
 for(uint32_t i=0;i<count[1];++i){const auto*q=p+off[1]+i*112;DungeonMechanismSprite s;s.id=u32(q);s.texture_bytes=u32(q+20);s.texture_crc=u32(q+24);std::copy(q+28,q+60,s.texture_sha256.begin());
  s.width=u32(q+60);s.height=u32(q+64);s.cell_width=u32(q+68);s.cell_height=u32(q+72);s.columns=u32(q+76);s.rows=u32(q+80);s.frames=u32(q+84);s.position=vec(q+88);s.offset=vec(q+96);s.bits=u32(q+104);const uint32_t z=u32(q+108);std::memcpy(&s.z_index,&z,4);
  if(!s.id||s.id>65535||!ids.insert(s.id).second||!text(q+4,s.texture)||!text(q+12,s.source)||s.texture.compare(0,9,"graphics/")||!ends(s.texture,".t3x")||s.source.compare(0,9,"Graphics/")||!ends(s.source,".png")||!textures.insert(s.texture).second||!s.texture_bytes||s.texture_bytes>8*1024*1024||std::none_of(s.texture_sha256.begin(),s.texture_sha256.end(),[](uint8_t c){return c!=0;}))return bad(e,"Mechanism presentation profile/texture identity rejected");
  if(!s.width||s.width>1024||!s.height||s.height>1024||!s.cell_width||s.cell_width>s.width||!s.cell_height||s.cell_height>s.height||!s.columns||s.columns>16||!s.rows||s.rows>65535||s.width!=s.cell_width*s.columns||s.height!=s.cell_height*s.rows||!s.frames||s.frames>s.columns*s.rows||(s.frames+s.columns-1)/s.columns!=s.rows||s.bits>31||s.z_index< -4096||s.z_index>4096||!finite(s.position)||!finite(s.offset))return bad(e,"Mechanism presentation sprite grid/flags/origin rejected");
  const auto*profile=v->bindings.profile(s.id);if(!profile)return bad(e,"Mechanism presentation clock profile missing");
  const auto&props=profile->family==DungeonMechanismFamily::Wall?profile->wall.properties():profile->properties;
  if(props.frame_count()!=s.frames)return bad(e,"Mechanism presentation sprite frames differ from clock");v->sprites.push_back(std::move(s));
 }
 for(const auto&binding:v->bindings.bindings())if(!ids.count(binding.profile))return bad(e,"Mechanism presentation owner sprite missing");
 std::set<uint32_t>seen;std::set<std::string>paths;std::set<uint32_t>used;
 for(uint32_t i=0;i<count[2];++i){const auto*q=p+off[2]+i*40;DungeonMechanismPlacement o;o.owner=u32(q);o.profile=u32(q+4);o.position=vec(q+24);o.bits=u32(q+32);o.order=u32(q+36);const auto&b=v->bindings.bindings()[i];
  if(o.owner!=b.owner||o.profile!=b.profile||!seen.insert(o.owner).second||!text(q+8,o.path)||!text(q+16,o.parent)||!paths.insert(o.path).second||o.path.find_last_of('/')==std::string::npos||o.parent!=o.path.substr(0,o.path.find_last_of('/'))||o.bits>1||o.order!=i||!finite(o.position))return bad(e,"Mechanism presentation owner path/order/profile rejected");
  std::string_view expected;Vec2 point{};bool found=false;
  for(uint32_t j=0;j<g.switch_count();++j){const auto n=g.switch_node(j);if(n.id==o.owner){expected=g.text(n.path);point={n.x,n.y};found=true;}}
  for(uint32_t j=0;j<g.object_count();++j){const auto n=g.object(j);if(n.id==o.owner){expected=g.text(n.path);point={n.x,n.y};found=true;}}
  if(!found||expected!=o.path||point.x!=o.position.x||point.y!=o.position.y)return bad(e,"Mechanism presentation source owner placement rejected");used.insert(o.profile);v->owners.push_back(std::move(o));
 }
 if(used!=ids)return bad(e,"Mechanism presentation unused sprite profile rejected");
 std::set<std::pair<uint32_t,std::string>>shape_paths;std::vector<std::vector<uint32_t>>coverage(v->owners.size());
 for(uint32_t i=0;i<count[3];++i){const auto*q=p+off[3]+i*64;DungeonMechanismShape s;s.owner=u32(q);const auto role=u32(q+4);s.role=DungeonMechanismShapeRole(role);s.body_position=vec(q+16);s.shape_position=vec(q+24);s.extents=vec(q+32);s.layer=u32(q+40);s.mask=u32(q+44);s.bits=u32(q+48);s.safe_margin=real(q+52);s.one_way_margin=real(q+56);
  size_t owner=v->owners.size();for(size_t j=0;j<v->owners.size();++j)if(v->owners[j].owner==s.owner)owner=j;
  if(owner==v->owners.size()||role<1||role>3||!text(q+8,s.path)||!shape_paths.insert({s.owner,s.path}).second||u32(q+60)||((s.layer|s.mask)&~0xfffffu)||s.bits>15||!finite(s.body_position)||!finite(s.shape_position)||!finite(s.extents)||s.extents.x<=0||s.extents.y<=0||!std::isfinite(s.safe_margin)||s.safe_margin<0||s.safe_margin>1000||!std::isfinite(s.one_way_margin)||s.one_way_margin<0||s.one_way_margin>1000||(role==1&&(s.bits&6))||(role!=1&&s.safe_margin!=0))return bad(e,"Mechanism presentation shape identity/geometry/flags rejected");
  coverage[owner].push_back(role);v->shapes.push_back(std::move(s));
 }
 for(size_t i=0;i<v->owners.size();++i){auto roles=coverage[i];std::sort(roles.begin(),roles.end());const auto family=v->bindings.profile(v->owners[i].profile)->family;const auto expected=family==DungeonMechanismFamily::Lever?std::vector<uint32_t>{1,2}:family==DungeonMechanismFamily::Plate?std::vector<uint32_t>{3}:std::vector<uint32_t>{1};if(roles!=expected)return bad(e,"Mechanism presentation shape family coverage rejected");}
 if(capability==2){
  if(!v->audio.load(p+off[6],count[6],e))return false;std::set<uint32_t>sound_owners,assets;unsigned denied=0;
  for(uint32_t i=0;i<count[5];++i){const auto*q=p+off[5]+i*88;DungeonMechanismSound s;s.owner=u32(q);const auto kind=u32(q+4);s.kind=DungeonMechanismSoundKind(kind);std::copy(q+24,q+56,s.source_sha256.begin());s.stable_id=u32(q+56);s.gain_db=real(q+60);s.pitch_scale=real(q+64);s.bus=u32(q+68);s.pause_mode=u32(q+72);const auto channel_first=u32(q+76),channel_size=u32(q+80);
   if(!sound_owners.insert(s.owner).second||kind<1||kind>2||!text(q+8,s.path)||!text(q+16,s.source)||s.source.compare(0,6,"Audio/")||std::none_of(s.source_sha256.begin(),s.source_sha256.end(),[](uint8_t c){return c!=0;})||!std::isfinite(s.gain_db)||s.gain_db< -120||s.gain_db>24||!std::isfinite(s.pitch_scale)||s.pitch_scale<.01f||s.pitch_scale>4||s.bus!=1||s.pause_mode!=0||u32(q+84)||(channel_size?!text(q+76,s.channel):channel_first!=0))return bad(e,"Mechanism sound owner/source/policy rejected");
   AudioAsset asset;const std::string uri="res://"+s.source;
   if(!v->audio.find(s.stable_id,asset)||asset.source_path!=uri||asset.source_sha256!=s.source_sha256||asset.loops())return bad(e,"Mechanism sound PCM bank/source binding rejected");assets.insert(s.stable_id);
   if(kind==2){const auto policy=g.policy();if(s.owner!=0||s.source!=g.text(policy.denied_audio)||s.channel!=g.text(policy.denied_channel))return bad(e,"Mechanism denied-operation source/channel rejected");++denied;}
   else {
    const DungeonMechanismPlacement*owner=nullptr;for(const auto&o:v->owners)if(o.owner==s.owner)owner=&o;
    if(!owner||s.path.find_last_of('/')==std::string::npos||s.path.substr(0,s.path.find_last_of('/'))!=owner->path||!s.channel.empty())return bad(e,"Mechanism source-node audio path rejected");const auto*profile=v->bindings.profile(owner->profile);
    if(profile->family==DungeonMechanismFamily::Wall){const auto&w=profile->wall.audio();if(s.stable_id!=w.id||uri!=w.source||s.source_sha256!=w.sha||s.gain_db!=w.gain_db||s.pitch_scale!=w.pitch||s.bus!=w.bus)return bad(e,"Mechanism wall clock/audio source binding rejected");}
    else {bool matched=false;for(uint32_t j=0;j<g.switch_count();++j){const auto node=g.switch_node(j);if(node.id==s.owner&&g.text(node.audio)==s.source)matched=true;}if(!matched)return bad(e,"Mechanism switch clock/audio source binding rejected");}
   }v->sounds.push_back(std::move(s));
  }
  std::set<uint32_t>expected{0};for(const auto&o:v->owners)if(v->bindings.profile(o.profile)->family!=DungeonMechanismFamily::Gate)expected.insert(o.owner);
  if(sound_owners!=expected||denied!=1||assets.size()!=v->audio.count())return bad(e,"Mechanism sound missing/unowned source/owner rejected");
 }
 if(std::find(owned.begin(),owned.end(),0)!=owned.end())return bad(e,"Mechanism presentation unowned text rejected");
 state_=std::move(v);e.clear();return true;
}
bool DungeonMechanismPresentationData::texture_matches(uint32_t id,const uint8_t*p,size_t n,std::string&e)const{
 const auto*s=sprite(id);if(!s||!p||n!=s->texture_bytes||encore::crc32(p,n)!=s->texture_crc)return bad(e,"Mechanism prepared texture identity rejected");e.clear();return true;
}
bool DungeonMechanismPresentationData::draw(uint32_t owner,uint32_t frame,DungeonMechanismDraw&out,std::string&e)const{
 for(const auto&o:owners())if(o.owner==owner){const auto*s=sprite(o.profile);if(!s||frame>=s->frames)return bad(e,"Mechanism draw frame/profile rejected");
  DungeonMechanismDraw d;d.owner=o.owner;d.profile=o.profile;d.x=frame%s->columns*s->cell_width;d.y=frame/s->columns*s->cell_height;d.width=s->cell_width;d.height=s->cell_height;d.bits=s->bits;d.order=o.order;d.z_index=s->z_index;d.sort_y=o.position.y;d.parent_y_sort=(o.bits&1)!=0;
  const float x=o.position.x+s->position.x,y=o.position.y+s->position.y;
  const float rx=s->offset.x-((s->bits&1)?s->cell_width/2.0f:0),ry=s->offset.y-((s->bits&1)?s->cell_height/2.0f:0);
  d.top_left={x+rx,y+ry};out=d;e.clear();return true;
 }return bad(e,"Mechanism draw owner missing");
}
bool DungeonMechanismPresentationData::rectangle(size_t index,DungeonMechanismRectangle&out,std::string&e)const{
 if(index>=shapes().size())return bad(e,"Mechanism rectangle index rejected");const auto&s=shapes()[index];
 for(const auto&o:owners())if(o.owner==s.owner){DungeonMechanismRectangle r;r.extents=s.extents;const float bx=o.position.x+s.body_position.x,by=o.position.y+s.body_position.y;r.center={bx+s.shape_position.x,by+s.shape_position.y};r.minimum={r.center.x-r.extents.x,r.center.y-r.extents.y};r.maximum={r.center.x+r.extents.x,r.center.y+r.extents.y};out=r;e.clear();return true;}
 return bad(e,"Mechanism rectangle owner missing");
}
}
