#include "encore/breakables.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){auto u=word(p);float v;std::memcpy(&v,&u,4);return v;}
double scalar(const uint8_t*p){uint64_t u=word(p)|uint64_t(word(p+4))<<32;double v;std::memcpy(&v,&u,8);return v;}
bool finite(double v){return std::isfinite(v)&&std::abs(v)<=1e6;}
bool path(const std::string&s,bool empty=false){if(s.empty())return empty;if(s.front()=='/'||s.back()=='/')return false;size_t begin=0;for(size_t n=0;n<=s.size();++n)if(n==s.size()||s[n]=='/'){const auto part=s.substr(begin,n-begin);if(part.empty()||part=="."||part=="..")return false;begin=n+1;}return true;}
}
const BreakableFamily*BreakablesData::family(uint32_t id)const{for(const auto&v:families_)if(v.id==id)return &v;return nullptr;}
const BreakableOwner*BreakablesData::owner(uint32_t id)const{for(const auto&v:owners_)if(v.id==id)return &v;return nullptr;}
bool BreakablesData::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,256*1024,e)&&load(b.data(),b.size(),e);}
bool BreakablesData::load(const uint8_t*p,size_t size,std::string&e){
 if(!p||size<176||size>256*1024)return fail(e,"Breakable resource size");
 if(std::memcmp(p,"ENCBRK01",8)||word(p+8)!=1||word(p+12)!=size||word(p+20)!=1||!word(p+24)||word(p+24)>65535||word(p+28)||word(p+52)!=7||word(p+56)||word(p+60)||encore::crc32(p+32,size-32)!=word(p+16))return fail(e,"Breakable header/version/capability/checksum");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(size_t i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[2*i])*16+hex(pin[2*i+1])))return fail(e,"Breakable source pin");}
 const uint32_t strides[]={104,136,88,24,20,1,1},limits[]={1,8,128,128,256,65536,65536};uint32_t offsets[7]{},counts[7]{};size_t cursor=176;
 for(unsigned i=0;i<7;++i){const auto*d=p+64+i*16;offsets[i]=word(d+4);counts[i]=word(d+8);if(word(d)!=i+1||word(d+12)!=strides[i]||counts[i]>limits[i])return fail(e,"Breakable section directory");if(!counts[i]){if(offsets[i]||i==0||i==1||i==2||i==5||i==6)return fail(e,"Breakable empty section");continue;}
  if(offsets[i]%4||offsets[i]<cursor||offsets[i]>size||counts[i]>(size-offsets[i])/strides[i])return fail(e,"Breakable section span");for(size_t n=cursor;n<offsets[i];++n)if(p[n])return fail(e,"Breakable padding");cursor=offsets[i]+counts[i]*strides[i];}
 if(cursor!=size||counts[0]!=1)return fail(e,"Breakable trailing bytes/policy");
 std::map<uint32_t,std::string>strings;std::set<std::string>distinct;const auto*pool=p+offsets[6];
 if(pool[0]||pool[counts[6]-1])return fail(e,"Breakable string pool boundary");
 for(uint32_t n=0;n<counts[6];){const auto start=n;std::string s;while(n<counts[6]&&pool[n]){const auto c=pool[n++];if(c<32||c>=127||c=='\\'||c==':'||s.size()>=512)return fail(e,"Breakable string characters/length");s+=char(c);}if(n==counts[6]||(!start&&!s.empty())||(start&&s.empty())||!distinct.insert(s).second)return fail(e,"Breakable string canonical identity");strings.emplace(start,std::move(s));++n;}
 auto string=[&](uint32_t offset,std::string&out,bool empty=false,bool is_path=false){auto found=strings.find(offset);if(found==strings.end()||(!empty&&found->second.empty())||(is_path&&!path(found->second,empty)))return false;out=found->second;return true;};
 BreakablesData v;v.scene_=word(p+24);const auto*pr=p+offsets[0];double values[9];for(unsigned n=0;n<9;++n){values[n]=scalar(pr+8*n);if(!finite(values[n]))return fail(e,"Breakable policy scalar");}
 auto&policy=v.policy_;policy.distance_delay=values[0];policy.maximum_delay=values[1];policy.weak=values[2];policy.strong=values[3];policy.rumble_seconds=values[4];policy.lift=values[5];policy.lift_seconds=values[6];policy.fall=values[7];policy.fall_seconds=values[8];policy.device=word(pr+76);policy.clear_hit_layer=word(pr+80);policy.clear_hit_mask=word(pr+84);policy.clear_body_layer=word(pr+88);policy.sound=word(pr+92);
 if(values[0]<=0||values[1]<0||values[2]<0||values[2]>1||values[3]<0||values[3]>1||values[4]<0||values[5]>0||values[6]<=0||values[7]<0||values[8]<=0||word(pr+72)!=v.scene_||policy.device>=16||!policy.sound||policy.sound==UINT32_MAX||!policy.clear_hit_layer||policy.clear_hit_layer>=1u<<20||!policy.clear_hit_mask||policy.clear_hit_mask>=1u<<20||!policy.clear_body_layer||policy.clear_body_layer>=1u<<20||!string(word(pr+96),policy.source,false,true)||!string(word(pr+100),policy.name))return fail(e,"Breakable policy domain");
 std::set<std::string>target_paths;
 for(uint32_t i=0;i<counts[3];++i){const auto*r=p+offsets[3]+i*24;BreakableContentBinding row;if(!string(word(r),row.path,false,true)||!string(word(r+4),row.type)||!string(word(r+8),row.script,true,true)||!string(word(r+12),row.source,false,true)||!string(word(r+16),row.parent,false,true)||word(r+20)||!target_paths.insert(row.path).second)return fail(e,"Breakable content binding");v.contents_.push_back(std::move(row));}
 std::set<uint32_t>family_ids;uint32_t clip_first=0,audio_first=0;
 for(uint32_t i=0;i<counts[1];++i){const auto*r=p+offsets[1]+i*136;BreakableFamily row;row.id=word(r);row.columns=word(r+12);row.rows=word(r+16);row.width=word(r+20);row.height=word(r+24);const auto first=word(r+28),length=word(r+32),af=word(r+48),ac=word(r+52);row.break_clip=word(r+36);row.ready_clip=word(r+40);row.reset_clip=word(r+44);row.spatial_audio=word(r+56);row.body_layer=word(r+60);row.body_mask=word(r+64);row.hit_layer=word(r+68);row.hit_mask=word(r+72);
  if(!row.id||row.id==UINT32_MAX||!family_ids.insert(row.id).second||!string(word(r+4),row.source,false,true)||!string(word(r+8),row.texture,false,true)||!row.columns||row.columns>65535||!row.rows||row.rows>65535||!row.width||row.width>65535||!row.height||row.height>65535||row.width%row.columns||row.height%row.rows||first!=clip_first||!length||length>counts[5]-clip_first||af!=audio_first||ac>counts[4]-audio_first||word(r+56)>1||row.body_layer>=1u<<20||row.body_mask>=1u<<20||row.hit_layer>=1u<<20||row.hit_mask>=1u<<20||word(r+76)||word(r+132))return fail(e,"Breakable family identity/span");
  float x[13];for(unsigned n=0;n<13;++n){x[n]=real(r+80+4*n);if(!finite(x[n]))return fail(e,"Breakable family scalar");}row.sprite_offset={x[0],x[1]};row.drop_offset={x[2],x[3]};row.drop_seconds=x[4];if(x[4]<=0||x[4]>60||x[7]<=0||x[8]<=0||x[11]<=0||x[12]<=0)return fail(e,"Breakable timer/shape domain");for(unsigned n=0;n<8;++n)row.shapes[n/4][n%4]=x[5+n];
  if(!row.clips.load(p+offsets[5]+first,length,e)||uint64_t(row.columns)*row.rows!=row.clips.frame_count()||row.clips.initial().hitbox||row.clips.initial().interaction||row.clips.clips().size()!=3||row.break_clip==row.ready_clip||row.break_clip==row.reset_clip||row.ready_clip==row.reset_clip||!row.clips.clip(row.break_clip)||!row.clips.clip(row.ready_clip)||!row.clips.clip(row.reset_clip))return fail(e,"Breakable nested clips/roles");
  for(const auto&key:row.clips.keys())if(uint32_t(key.track)>3)return fail(e,"Breakable foreign nested track");
  std::map<uint32_t,float>last;
  for(uint32_t n=0;n<ac;++n){const auto*a=p+offsets[4]+(af+n)*20;BreakableFamily::AudioKey key;key.clip=word(a);key.time=real(a+4);key.value=word(a+8);key.update=word(a+12);key.track=word(a+16);const auto*c=row.clips.clip(key.clip);if(!c||!std::isfinite(key.time)||key.time<0||key.time>c->duration||word(a+8)>1||key.update>1||key.track>=64||(last.count(key.clip)&&key.time<=last[key.clip])||(!key.update&&key.time!=0))return fail(e,"Breakable audio key domain/order");last[key.clip]=key.time;row.audio.push_back(key);}
  clip_first+=length;audio_first+=ac;v.families_.push_back(std::move(row));
 }
 if(clip_first!=counts[5]||audio_first!=counts[4])return fail(e,"Breakable unowned nested/audio data");
 std::set<uint32_t>owner_ids,used_targets;std::set<std::string>owner_paths;
 for(uint32_t i=0;i<counts[2];++i){const auto*r=p+offsets[2]+i*88;BreakableOwner row;row.id=word(r);row.family=word(r+4);row.content=word(r+24);row.coins=word(r+32);row.object_flag=word(r+40);const auto flags=word(r+44);row.emit_flag=flags&1;row.reset_area=flags&2;row.reset_region=flags&4;row.drop_hidden=flags&8;row.interact_content=flags&16;row.pause=flags&32;row.hide_drop=word(r+56);row.tween_drop=word(r+60);row.position={real(r+80),real(r+84)};
  if(!row.id||row.id==UINT32_MAX||!owner_ids.insert(row.id).second||!v.family(row.family)||(row.content!=UINT32_MAX&&row.content>=counts[3])||row.coins>1024||word(r+40)>1||flags>=64||word(r+56)>1||word(r+60)>1||word(r+64)||word(r+68)||word(r+72)||word(r+76)||!finite(row.position.x)||!finite(row.position.y)||!string(word(r+8),row.path,false,true)||!owner_paths.insert(row.path).second||!string(word(r+12),row.parent,false,true)||!string(word(r+16),row.new_parent,true,true)||!string(word(r+20),row.drop_parent,false,true)||!string(word(r+28),row.spawner,true,true)||!string(word(r+36),row.flag)||!string(word(r+48),row.appear,true)||!string(word(r+52),row.disappear,true))return fail(e,"Breakable owner identity/domain/binding");
  if(row.content!=UINT32_MAX)used_targets.insert(row.content);v.owners_.push_back(std::move(row));
 }
 if(used_targets.size()!=v.contents_.size())return fail(e,"Breakable unowned content");v.valid_=true;*this=std::move(v);e.clear();return true;
}
}
