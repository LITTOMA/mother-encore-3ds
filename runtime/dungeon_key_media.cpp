#include "encore/dungeon_key_media.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
namespace encore::upstream {namespace {
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float f(const uint8_t*p){uint32_t raw=u(p);float v;std::memcpy(&v,&raw,4);return v;}
int32_t integer(const uint8_t*p){uint32_t raw=u(p);int32_t v;std::memcpy(&v,&raw,4);return v;}
bool bad(std::string&e,const char*why){e=why;return false;}
bool finite(float v){return std::isfinite(v)&&std::abs(v)<=1000000;}
bool path(std::string_view s){if(s.empty()||s.size()>512)return false;size_t from=0;for(size_t i=0;i<=s.size();++i){if(i==s.size()||s[i]=='/'){const auto p=s.substr(from,i-from);if(p.empty()||p=="."||p=="..")return false;from=i+1;}else if(s[i]<32||s[i]>126||s[i]==':'||s[i]=='\\')return false;}return true;}
}
struct DungeonKeyMediaData::State {uint32_t scene=0;DungeonKeyClocksData clocks;AudioBank bank;std::vector<KeySprite>sprites;std::vector<KeyPlacement>owners;std::vector<KeyShape>shapes;std::vector<KeySound>sounds;};
uint32_t DungeonKeyMediaData::scene()const{return state_?state_->scene:0;}
const DungeonKeyClocksData&DungeonKeyMediaData::clocks()const{static const DungeonKeyClocksData empty;return state_?state_->clocks:empty;}
const AudioBank&DungeonKeyMediaData::audio_bank()const{static const AudioBank empty;return state_?state_->bank:empty;}
const std::vector<KeySprite>&DungeonKeyMediaData::sprites()const{static const std::vector<KeySprite>empty;return state_?state_->sprites:empty;}
const std::vector<KeyPlacement>&DungeonKeyMediaData::owners()const{static const std::vector<KeyPlacement>empty;return state_?state_->owners:empty;}
const std::vector<KeyShape>&DungeonKeyMediaData::shapes()const{static const std::vector<KeyShape>empty;return state_?state_->shapes:empty;}
const std::vector<KeySound>&DungeonKeyMediaData::sounds()const{static const std::vector<KeySound>empty;return state_?state_->sounds:empty;}
const KeySprite*DungeonKeyMediaData::sprite(uint32_t id)const{for(const auto&s:sprites())if(s.id==id)return &s;return nullptr;}
const KeyPlacement*DungeonKeyMediaData::owner(uint32_t id)const{for(const auto&o:owners())if(o.owner==id)return &o;return nullptr;}
bool DungeonKeyMediaData::load_file(const char*name,std::string&e){std::vector<uint8_t>b;return encore::read_file(name,b,512*1024,e)&&load(b.data(),b.size(),e);}
bool DungeonKeyMediaData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<176||n>512*1024)return bad(e,"Key media size rejected");
 if(std::memcmp(p,"ENCKMP01",8)||u(p+8)!=1||u(p+12)!=n||u(p+20)!=1||!u(p+24)||u(p+24)>=65536||u(p+28)||u(p+52)!=7||u(p+56)||u(p+60))return bad(e,"Key media schema/capability/scene rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";for(unsigned i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[2*i])*16+hex(pin[2*i+1])))return bad(e,"Key media source pin rejected");}
 if(encore::crc32(p+32,n-32)!=u(p+16))return bad(e,"Key media checksum rejected");
 const uint32_t strides[]={1,1,112,64,64,48,1},limits[]={65536,128*1024,16,128,384,128,65536};uint32_t offsets[7],counts[7];size_t cursor=176;
 for(unsigned i=0;i<7;++i){const auto*d=p+64+i*16;offsets[i]=u(d+4);counts[i]=u(d+8);const size_t aligned=(cursor+3)&~size_t(3);if(u(d)!=i+1||u(d+12)!=strides[i]||!counts[i]||counts[i]>limits[i]||offsets[i]!=aligned||offsets[i]>n||counts[i]>(n-offsets[i])/strides[i])return bad(e,"Key media directory/span rejected");for(size_t at=cursor;at<aligned;++at)if(p[at])return bad(e,"Key media padding rejected");cursor=offsets[i]+size_t(counts[i])*strides[i];}
 if(cursor!=n)return bad(e,"Key media trailing data rejected");auto next=std::make_shared<State>();next->scene=u(p+24);
 if(!next->clocks.load(p+offsets[1],counts[1],e)||next->clocks.scene()!=next->scene||!next->bank.load(p+offsets[6],counts[6],e))return bad(e,"Key media nested clock/bank rejected");
 std::map<std::string,std::pair<uint32_t,uint32_t>>strings;uint32_t owned=0;
 auto text=[&](uint32_t at,uint32_t length,std::string&out){if(!length||length>512||at>counts[0]||length>counts[0]-at)return false;std::string v(reinterpret_cast<const char*>(p+offsets[0]+at),length);if(!path(v))return false;auto found=strings.find(v);if(found==strings.end()){if(at!=owned)return false;strings[v]={at,length};owned+=length;}else if(found->second!=std::make_pair(at,length))return false;out=std::move(v);return true;};
 std::set<uint32_t>profiles;std::set<std::string>textures;
 for(uint32_t i=0;i<counts[2];++i){const auto*r=p+offsets[2]+i*112;KeySprite s;s.id=u(r);s.texture_bytes=u(r+20);s.texture_crc=u(r+24);std::copy(r+28,r+60,s.sha.begin());s.width=u(r+60);s.height=u(r+64);s.cell_width=u(r+68);s.cell_height=u(r+72);s.columns=u(r+76);s.rows=u(r+80);s.frames=u(r+84);s.offset={f(r+88),f(r+92)};s.bits=u(r+96);s.z_index=integer(r+100);const auto*c=next->clocks.profile(s.id);
  if(!c||!profiles.insert(s.id).second||!s.texture_bytes||s.texture_bytes>8*1024*1024||std::none_of(s.sha.begin(),s.sha.end(),[](uint8_t v){return v!=0;})||!s.width||s.width>1024||!s.height||s.height>1024||!s.cell_width||s.cell_width>s.width||!s.cell_height||s.cell_height>s.height||!s.columns||s.columns>16||!s.rows||s.rows>1024||s.width!=s.cell_width*s.columns||s.height!=s.cell_height*s.rows||s.frames!=c->frames||(s.rows-1)*s.columns>=s.frames||s.frames>s.columns*s.rows||s.bits>31||bool(s.bits&2)!=bool(c->mask&1)||!finite(s.offset.x)||!finite(s.offset.y)||s.z_index< -4096||s.z_index>4096||u(r+104)||u(r+108)||!text(u(r+4),u(r+8),s.texture)||!text(u(r+12),u(r+16),s.source)||s.texture.compare(0,9,"graphics/")||s.texture.size()<4||s.texture.substr(s.texture.size()-4)!=".t3x"||!textures.insert(s.texture).second||s.source.compare(0,9,"Graphics/")||s.source.size()<4||s.source.substr(s.source.size()-4)!=".png")return bad(e,"Key media atlas/profile identity rejected");next->sprites.push_back(std::move(s));
 }
 if(profiles.size()!=next->clocks.profiles().size())return bad(e,"Key media sprite profile coverage rejected");
 const auto metadata=next->clocks.metadata();if(counts[3]!=metadata.node_count())return bad(e,"Key media owner count rejected");std::set<uint32_t>ids,orders;
 for(uint32_t i=0;i<counts[3];++i){const auto*r=p+offsets[3]+i*64;KeyPlacement o;o.owner=u(r);o.profile=u(r+4);o.position={f(r+24),f(r+28)};o.child_position={f(r+32),f(r+36)};o.bits=u(r+40);o.order=u(r+44);o.z_base=integer(r+48);o.parent_order=u(r+52);const auto n=metadata.node(i);const auto*c=next->clocks.profile(o.profile);
  if(!ids.insert(o.owner).second||o.owner!=n.id||!c||uint32_t(c->kind)!=uint32_t(n.kind)||o.bits>31||!finite(o.position.x)||!finite(o.position.y)||!finite(o.child_position.x)||!finite(o.child_position.y)||(!c->child&&(o.child_position.x||o.child_position.y))||o.order>65535||o.parent_order>=o.order||!orders.insert(o.order).second||o.z_base< -4096||o.z_base>4096||u(r+56)||u(r+60)||!text(u(r+8),u(r+12),o.path)||!text(u(r+16),u(r+20),o.parent)||o.path!=metadata.text(n.path)||o.path.find_last_of('/')==std::string::npos||o.parent!=o.path.substr(0,o.path.find_last_of('/')))return bad(e,"Key media placement/owner rejected");next->owners.push_back(std::move(o));
 }
 auto owner=[&](uint32_t id)->const KeyPlacement*{for(const auto&o:next->owners)if(o.owner==id)return &o;return nullptr;};std::map<uint32_t,std::set<uint32_t>>roles;std::set<std::pair<uint32_t,std::string>>shape_paths;
 for(uint32_t i=0;i<counts[4];++i){const auto*r=p+offsets[4]+i*64;KeyShape s;s.owner=u(r);const uint32_t role=u(r+4);s.role=KeyShapeRole(role);s.center={f(r+16),f(r+20)};s.extents={f(r+24),f(r+28)};s.layer=u(r+32);s.mask=u(r+36);s.bits=u(r+40);s.one_way_margin=f(r+44);const auto*o=owner(s.owner);const auto*c=o?next->clocks.profile(o->profile):nullptr;
  if(!c||role<1||role>3||(role==1)!=(c->kind==KeyClockKind::Key)||!roles[s.owner].insert(role).second||!finite(s.center.x)||!finite(s.center.y)||!finite(s.extents.x)||!finite(s.extents.y)||s.extents.x<=0||s.extents.y<=0||s.layer>0xfffff||s.mask>0xfffff||s.bits>7||(role==2&&(s.bits&6))||!std::isfinite(s.one_way_margin)||s.one_way_margin<0||s.one_way_margin>1000||u(r+48)||u(r+52)||u(r+56)||u(r+60)||!text(u(r+8),u(r+12),s.path)||!shape_paths.insert({s.owner,s.path}).second)return bad(e,"Key media shape/filter rejected");next->shapes.push_back(std::move(s));
 }
 for(const auto&o:next->owners){const auto kind=next->clocks.profile(o.profile)->kind;const auto expected=kind==KeyClockKind::Key?std::set<uint32_t>{1}:std::set<uint32_t>{2,3};if(roles[o.owner]!=expected)return bad(e,"Key media shape coverage rejected");}
 std::set<uint32_t>sound_owners,assets;
 for(uint32_t i=0;i<counts[5];++i){const auto*r=p+offsets[5]+i*48;KeySound s;s.owner=u(r);s.profile=u(r+4);s.asset=u(r+8);s.gain=f(r+20);s.pitch=f(r+24);s.bus=u(r+28);const auto finish=u(r+32);s.finished_callback=finish!=0;const auto*o=owner(s.owner);const auto*c=o?next->clocks.profile(o->profile):nullptr;const KeyClockAudio*a=nullptr;for(const auto&v:next->clocks.audios())if(v.profile==s.profile)a=&v;AudioAsset asset;
  if(!c||!a||!sound_owners.insert(s.owner).second||s.profile!=(c->child?c->child:c->id)||s.gain!=a->gain||s.pitch!=a->pitch||s.bus!=a->bus||finish>1||(s.finished_callback&&c->kind!=KeyClockKind::Key)||u(r+36)||u(r+40)||u(r+44)||!next->bank.find(s.asset,asset)||asset.source_path!=a->source||asset.source_sha256!=a->sha||asset.loops()||!text(u(r+12),u(r+16),s.path)||s.path!=o->path+(c->child?"/Lock":"")+"/AudioStreamPlayer")return bad(e,"Key media sound node/clock/bank binding rejected");assets.insert(s.asset);next->sounds.push_back(std::move(s));
 }
 if(sound_owners!=ids||assets.size()!=next->bank.count()||owned!=counts[0])return bad(e,"Key media unowned audio/text rejected");state_=std::move(next);e.clear();return true;
}
bool DungeonKeyMediaData::matches(const DungeonKeyClocks&world,std::string&e)const{if(!valid()||!world.valid()||clocks().metadata().bytes_identity()!=world.data().metadata().bytes_identity())return bad(e,"Key media clock resource owner differs");e.clear();return true;}
bool DungeonKeyMediaData::texture_matches(uint32_t id,const uint8_t*p,size_t n,std::string&e)const{const auto*s=sprite(id);if(!s||!p||n!=s->texture_bytes||encore::crc32(p,n)!=s->texture_crc)return bad(e,"Key media prepared texture bytes/CRC differ");e.clear();return true;}
bool DungeonKeyMediaData::draw(const DungeonKeyClocks&world,std::vector<KeyDraw>&out,std::string&e)const{
 if(!matches(world,e))return false;std::vector<KeyDraw>next;
 for(const auto&o:owners()){DungeonKeyClockView v;if(!world.state(world.epoch(),o.owner,v,e))return false;if(!v.object.alive||!v.object.visible||(o.bits&6)!=6)continue;
  for(unsigned child=0;child<unsigned(v.has_child)+1;++child){const auto&pose=child?v.child:v.root;if(!pose.visible||(child&&!v.root.child_visible))continue;const auto*profile=clocks().profile(o.profile);const auto*s=sprite(child?profile->child:profile->id);if(!s||pose.frame>=s->frames)return bad(e,"Key media draw frame is invalid");KeyDraw d;d.owner=o.owner;d.profile=s->id;d.x=pose.frame%s->columns*s->cell_width;d.y=pose.frame/s->columns*s->cell_height;d.width=s->cell_width;d.height=s->cell_height;d.bits=s->bits;d.order=o.order*2+child;d.parent_order=o.parent_order;d.parent_y_sort=o.bits&1;d.sort_y=o.position.y;d.z_index=(s->bits&16)?o.z_base+s->z_index:s->z_index;
   const float x=o.position.x+(child?o.child_position.x:0),y=o.position.y+(child?o.child_position.y:0);const float rx=s->offset.x-((s->bits&1)?float(s->cell_width)*.5f:0),ry=s->offset.y-((s->bits&1)?float(s->cell_height)*.5f:0);d.top_left={x+pose.position.x+rx,y+pose.position.y+ry};next.push_back(d);
  }
 }out=std::move(next);e.clear();return true;
}
bool DungeonKeyMediaData::rectangles(const DungeonKeyClocks&world,std::vector<KeyRectangle>&out,std::string&e)const{
 if(!matches(world,e))return false;std::vector<KeyRectangle>next;
 for(const auto&s:shapes()){DungeonKeyClockView v;if(!world.state(world.epoch(),s.owner,v,e))return false;KeyRectangle r;r.owner=s.owner;r.role=s.role;r.layer=s.layer;r.mask=s.mask;r.bits=s.bits;r.center=s.center;r.extents=s.extents;r.minimum={s.center.x-s.extents.x,s.center.y-s.extents.y};r.maximum={s.center.x+s.extents.x,s.center.y+s.extents.y};r.enabled=v.object.alive&&!(s.bits&1)&&(s.role==KeyShapeRole::Interaction||!v.object.collision_disabled);next.push_back(r);
 }out=std::move(next);e.clear();return true;
}
bool DungeonKeyAudioRouter::prepare(const DungeonKeyMediaData&d,const DungeonKeyClocks&w,std::string&e){if(epoch_||!d.matches(w,e)||w.epoch()<=last_epoch_)return bad(e,"Key audio binding epoch/resource rejected");data_=d;epoch_=last_epoch_=w.epoch();e.clear();return true;}
bool DungeonKeyAudioRouter::plan(const std::vector<DungeonKeyClockEvent>&events,std::vector<KeySoundCommand>&out,std::string&e)const{
 if(!epoch_||!data_.valid()||events.size()>4096)return bad(e,"Key audio producer epoch/count rejected");std::vector<KeySoundCommand>next;
 for(const auto&event:events){const auto*o=data_.owner(event.owner);if(event.epoch!=epoch_||!o)return bad(e,"Key audio producer scene/owner rejected");
  if(event.source==DungeonKeyClockEventSource::Method){const auto kind=uint32_t(event.method.kind);if(kind<1||kind>13||event.method.target.id!=event.owner||!event.method.target.generation)return bad(e,"Key audio unknown original method event");continue;}
  if(event.source!=DungeonKeyClockEventSource::Clock)return bad(e,"Key audio unknown producer source");const auto&c=event.clock;const auto kind=uint32_t(c.kind);const auto*profile=data_.clocks().profile(o->profile);const auto*clip=c.clip?data_.clocks().clip(c.clip):nullptr;
  if(kind<1||kind>8||(c.profile!=o->profile&&c.profile!=profile->child)||(c.clip&&(!clip||clip->profile!=c.profile))||(!c.clip&&c.kind!=KeyClockEventKind::Audio))return bad(e,"Key audio unknown clock owner/event");
  if(c.kind==KeyClockEventKind::Audio){if(c.value>1)return bad(e,"Key audio nonboolean write");bool found=false;for(uint32_t i=0;i<data_.sounds().size();++i){const auto&s=data_.sounds()[i];if(s.owner==event.owner&&s.profile==c.profile){next.push_back({epoch_,i,s.owner,s.profile,c.value!=0});found=true;break;}}if(!found)return bad(e,"Key audio sound node is absent");}
  else if(c.kind==KeyClockEventKind::RootParent){if(c.value!=1)return bad(e,"Key audio invalid root binding event");}
  else if(c.kind==KeyClockEventKind::InvalidSignal){if(c.value<1||c.value>4)return bad(e,"Key audio invalid source diagnostic role");}
  else if(c.value)return bad(e,"Key audio stray clock payload");
 }out=std::move(next);e.clear();return true;
}
bool DungeonKeyAudioRouter::retire(std::vector<KeySoundCommand>&out,std::string&e){if(!epoch_||!data_.valid())return bad(e,"Key audio retirement has no owner");std::vector<KeySoundCommand>next;for(uint32_t i=0;i<data_.sounds().size();++i){const auto&s=data_.sounds()[i];next.push_back({epoch_,i,s.owner,s.profile,false});}epoch_=0;out=std::move(next);e.clear();return true;}
bool DungeonKeyAudioRouter::finished(uint64_t epoch,uint32_t slot,DungeonKeyClocks&world,std::string&e)const{
 if(!epoch_||epoch!=epoch_||world.epoch()!=epoch_||!data_.matches(world,e)||slot>=data_.sounds().size())return bad(e,"Key audio completion epoch/slot/resource rejected");const auto&sound=data_.sounds()[slot];if(!sound.finished_callback)return world.node_audio_finished(epoch_,sound.owner,sound.profile,e);return world.audio_finished(epoch_,sound.owner,e);
}
}
