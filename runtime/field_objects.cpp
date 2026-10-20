#include "encore/field_objects.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){uint32_t bits=word(p);float v;std::memcpy(&v,&bits,4);return v;}
bool fail(std::string&e,const char*why){e=why;return false;}
bool path(const std::string&s){return !s.empty()&&s.size()<=512&&s.front()!='/'&&s.find("..") ==std::string::npos&&std::all_of(s.begin(),s.end(),[](unsigned char c){return c>=32&&c<127&&c!='\\'&&c!=':';});}
bool flag(const std::string&s){return !s.empty()&&s.size()<=128&&std::all_of(s.begin(),s.end(),[](unsigned char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';});}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=1000000&&std::abs(p.y)<=1000000;}
}
bool FieldObjectsData::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,1024*1024,e)&&load(b.data(),b.size(),e);}
bool FieldObjectsData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<324||n>1024*1024)return fail(e,"Field object size rejected");
 if(std::memcmp(p,"ENCFOB01",8)||word(p+8)!=1||word(p+12)!=n||word(p+20)<1||word(p+20)>3||!word(p+24)||word(p+24)==UINT32_MAX||word(p+28)||word(p+52)!=5||(word(p+20)<3&&(word(p+56)||word(p+60))))return fail(e,"Field object schema/capability/header rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};for(unsigned i=0;i<20;++i)if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail(e,"Field object source pin rejected");
 if(encore::crc32(p+32,n-32)!=word(p+16))return fail(e,"Field object checksum rejected");
 const auto capability=word(p+20);const uint32_t strides[]={1,64,capability>=2?136u:96u,16,4},limits[]={65536,1,128,4096,8192};uint32_t off[5],count[5];size_t cursor=144;
 for(unsigned i=0;i<5;++i){const auto*d=p+64+i*16;off[i]=word(d+4);count[i]=word(d+8);
  if(word(d)!=i+1||word(d+12)!=strides[i]||!count[i]||count[i]>limits[i]||off[i]%4||off[i]<cursor||off[i]>n||count[i]>(n-off[i])/strides[i])return fail(e,"Field object directory/span rejected");
  for(size_t at=cursor;at<off[i];++at)if(p[at])return fail(e,"Field object padding rejected");cursor=off[i]+count[i]*strides[i];
 }
 if(cursor!=n)return fail(e,"Field object trailing bytes rejected");
 FieldObjectsData v;v.scene_=word(p+24);v.capability_=capability;std::vector<uint8_t>text_owned(count[0],0);
 if(capability==3){v.texture_bytes_=word(p+56);v.texture_crc_=word(p+60);if(!v.texture_bytes_||v.texture_bytes_>1024*1024)return fail(e,"Field object drawing texture size rejected");}
 auto text=[&](const uint8_t*ref,std::string&out,bool empty){const auto first=word(ref),size=word(ref+4);if(first>count[0]||size>count[0]-first||(!empty&&!size))return false;if(!size){if(first)return false;out.clear();return true;}out.assign(reinterpret_cast<const char*>(p+off[0]+first),size);std::fill(text_owned.begin()+first,text_owned.begin()+first+size,1);return true;};
 const auto*policy=p+off[1];v.width_=word(policy);v.height_=word(policy+4);
 auto dimension=[](uint32_t x){return x>=8&&x<=1024&&(x&(x-1))==0;};
 if(!dimension(v.width_)||!dimension(v.height_)||(capability==1&&(word(policy+8)||word(policy+12)||word(policy+56)||word(policy+60)))||!text(policy+16,v.texture_,false)||!path(v.texture_)||v.texture_.size()<5||v.texture_.substr(v.texture_.size()-4)!=".t3x")return fail(e,"Field object texture policy rejected");
 v.ready_state_=uint64_t(word(policy+8))|(uint64_t(word(policy+12))<<32);
 v.player_layer_=word(policy+56);v.player_mask_=word(policy+60);
 if(capability>=2&&(!v.player_layer_||(v.player_layer_|v.player_mask_)&~0xfffffu))return fail(e,"Field object player collision policy rejected");
 std::copy(policy+24,policy+56,v.texture_sha256_.begin());if(std::all_of(v.texture_sha256_.begin(),v.texture_sha256_.end(),[](uint8_t x){return x==0;}))return fail(e,"Field object texture identity missing");
 std::vector<uint8_t>frame_owned(count[3],0),sequence_owned(count[4],0);
 for(uint32_t i=0;i<count[3];++i){const auto*b=p+off[3]+i*16;FieldObjectFrame f{word(b),word(b+4),word(b+8),word(b+12)};
  if(!f.width||!f.height||f.x>=v.width_||f.y>=v.height_||f.width>v.width_-f.x||f.height>v.height_-f.y)return fail(e,"Field object frame rectangle rejected");v.frames_.push_back(f);
 }
 for(uint32_t i=0;i<count[4];++i){auto frame=word(p+off[4]+i*4);if(frame>=count[3])return fail(e,"Field object animation frame rejected");v.animation_frames_.push_back(frame);}
 std::set<uint32_t>ids,bodies,draws,notices;std::set<std::string>paths;
 for(uint32_t i=0;i<count[2];++i){const auto*b=p+off[2]+i*strides[2];FieldObjectDefinition d;d.id=word(b);d.kind=word(b+4);d.flags=word(b+8);d.main_frame=word(b+12);d.first=word(b+16);d.count=word(b+20);d.initial=word(b+24);
  if(!d.id||d.id==UINT32_MAX||!ids.insert(d.id).second||d.kind!=1||(d.flags&~(capability>=2?511u:255u))||d.main_frame>=count[3]||d.first>count[4]||!d.count||d.count>count[4]-d.first||d.initial>=d.count||(capability==1&&word(b+28))||word(b+88)||word(b+92))return fail(e,"Field object identity/kind/animation span rejected");
  if(capability>=2){
   if(!(d.flags&uint32_t(FieldObjectFlag::PrecomputedReady))||!(d.flags&uint32_t(FieldObjectFlag::RandomFrame))||!word(b+28)||word(b+28)>d.count)return fail(e,"Field object precomputed ready frame rejected");
   d.resolved=word(b+28)-1;d.body=word(b+96);d.draw=word(b+100);d.notice=word(b+104);d.body_center={real(b+112),real(b+116)};d.body_extents={real(b+120),real(b+124)};
   d.body_layer=word(b+128);d.body_mask=word(b+132);const bool collides=(d.body_layer&v.player_mask_)||(d.body_mask&v.player_layer_);
   if(((d.body_layer|d.body_mask)&~0xfffffu)||collides!=(d.body!=kFieldNone)||(collides&&!bodies.insert(d.body).second)||d.draw==kFieldNone||d.notice==kFieldNone||!draws.insert(d.draw).second||!notices.insert(d.notice).second||word(b+108)||!finite(d.body_center)||!finite(d.body_extents)||d.body_extents.x<=0||d.body_extents.y<=0)return fail(e,"Field object map owner/geometry rejected");
  }
  if(!text(b+32,d.path,false)||!path(d.path)||!paths.insert(d.path).second||!text(b+40,d.ready_flag,true)||((d.flags&uint32_t(FieldObjectFlag::SuppressAtReady))?!flag(d.ready_flag):!d.ready_flag.empty()))return fail(e,"Field object path/ready flag rejected");
  d.position={real(b+48),real(b+52)};d.main_offset={real(b+56),real(b+60)};d.effect_offset={real(b+64),real(b+68)};d.fps=real(b+72);d.random_min=real(b+76);d.random_max=real(b+80);d.ready_delay=word(b+84);
  if(!finite(d.position)||!finite(d.main_offset)||!finite(d.effect_offset)||!std::isfinite(d.fps)||d.fps<=0||d.fps>240||!std::isfinite(1.f/d.fps)||!std::isfinite(d.random_min)||!std::isfinite(d.random_max)||d.ready_delay!=((d.flags&uint32_t(FieldObjectFlag::ShowAfterIdle))?1u:0u))return fail(e,"Field object finite clock/ready policy rejected");
  if(d.flags&uint32_t(FieldObjectFlag::RandomFrame)){if(d.random_min<0||d.random_max<=d.random_min||d.random_max>65536)return fail(e,"Field object random range rejected");}
  else if(d.random_min!=0||d.random_max!=0)return fail(e,"Field object unexpected random policy rejected");
  frame_owned[d.main_frame]=1;for(uint32_t k=0;k<d.count;++k){sequence_owned[d.first+k]=1;frame_owned[v.animation_frames_[d.first+k]]=1;}v.objects_.push_back(std::move(d));
 }
 if(std::find(text_owned.begin(),text_owned.end(),0)!=text_owned.end()||std::find(frame_owned.begin(),frame_owned.end(),0)!=frame_owned.end()||std::find(sequence_owned.begin(),sequence_owned.end(),0)!=sequence_owned.end())return fail(e,"Field object unconsumed content rejected");
 v.valid_=true;*this=std::move(v);e.clear();return true;
}
bool FieldObjectsData::bind(RoomView room,std::string&e)const{
 if(!valid_||!room.valid()||scene_!=room.scene().stable_id)return fail(e,"Field object room owner rejected");
 for(const auto&d:objects_)if(!d.ready_flag.empty()){bool found=false;for(uint32_t i=0;i<room.flag_count();++i)found|=room.string(room.flag(i).name_string)==d.ready_flag;if(!found)return fail(e,"Field object ready flag domain rejected");}
 e.clear();return true;
}
bool FieldObjectsData::texture_matches(const uint8_t*p,size_t n,std::string&e)const{
 if(!drawing_bound()||!p||n!=texture_bytes_||encore::crc32(p,n)!=texture_crc_)return fail(e,"Field object drawing texture receipt differs");e.clear();return true;
}
bool FieldObjectsData::bind(RoomView room,FieldMapView map,std::string&e)const{
 if(!bind(room,e))return false;
 if(!rendering_bound()||!map.valid()||map.scene_id()!=scene_||map.capabilities()!=6)return fail(e,"Field object map capability/scene rejected");
 auto same=[](Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;};
 auto conditions=[&](FieldSpan a,FieldSpan b){if(a.count!=b.count)return false;for(uint32_t i=0;i<a.count;++i){const auto x=map.condition(a.first+i),y=map.condition(b.first+i);if(map.text(x.flag)!=map.text(y.flag)||x.expected!=y.expected)return false;}return true;};
 uint32_t owned=0;for(uint32_t i=0;i<map.count(FieldMapSection::Items);++i)owned+=map.item(i).kind==uint16_t(FieldItemKind::OwnedObject);
 if(owned!=objects_.size())return fail(e,"Field object map coverage rejected");
 for(const auto&d:objects_){
  if((d.body!=kFieldNone&&d.body>=map.count(FieldMapSection::Bodies))||d.draw>=map.count(FieldMapSection::Items)||d.notice>=map.count(FieldMapSection::Notices))return fail(e,"Field object map index rejected");
  const auto item=map.item(d.draw);const auto notice=map.notice(d.notice);
  if(item.kind!=uint16_t(FieldItemKind::OwnedObject)||item.first!=d.id||item.count!=1||notice.kind!=uint16_t(FieldNoticeKind::Interaction)||map.text(notice.path)!=d.path||!same(notice.position,d.position)||!conditions(item.conditions,notice.conditions))return fail(e,"Field object source map join rejected");
  if(d.body!=kFieldNone){const auto body=map.body(d.body);const auto shape=map.shape(body.shape_first);
   if(body.shape_count!=1||!conditions(item.conditions,body.conditions)||shape.kind!=uint16_t(FieldShapeKind::Convex)||shape.count!=4||!same(shape.minimum,{d.body_center.x-d.body_extents.x,d.body_center.y-d.body_extents.y})||!same(shape.maximum,{d.body_center.x+d.body_extents.x,d.body_center.y+d.body_extents.y}))return fail(e,"Field object body geometry differs");
   for(uint32_t i=0;i<shape.count;++i){const auto p=map.point(shape.first+i);if((p.x!=shape.minimum.x&&p.x!=shape.maximum.x)||(p.y!=shape.minimum.y&&p.y!=shape.maximum.y))return fail(e,"Field object body is not an axis-aligned rectangle");}
  }
  for(uint32_t i=0;i<map.count(FieldMapSection::Groups);++i){const auto g=map.group(i);if(g.kind==uint32_t(FieldGroupKind::YSort)&&d.draw>=g.first&&d.draw-g.first<g.count&&item.sort_y!=d.position.y)return fail(e,"Field object draw source position differs");}
 }
 e.clear();return true;
}
bool FieldObjects::prepare(const FieldObjectsData&d,RoomView room,std::string&e){
 if(!d.bind(room,e))return false;FieldObjects v;v.data_=d;v.states_.resize(d.objects().size());
 *this=std::move(v);e.clear();return true;
}
size_t FieldObjects::index(uint32_t id)const{
 for(size_t i=0;i<data_.objects().size();++i)if(data_.objects()[i].id==id)return i;return states_.size();
}
const FieldObjectState*FieldObjects::state(uint32_t id)const{const auto i=index(id);return i<states_.size()?&states_[i]:nullptr;}
bool FieldObjects::enter_ready(uint32_t id,bool ready_flag,SourceRandom&random,std::string&e){
 const auto i=index(id);if(!prepared()||i==states_.size()||states_[i].ready||!states_[i].alive)return fail(e,"Field object ready owner/order rejected");
 const auto&d=data_.objects()[i];if(ready_flag&&!(d.flags&uint32_t(FieldObjectFlag::SuppressAtReady)))return fail(e,"Field object unexpected ready flag rejected");
 if(d.flags&uint32_t(FieldObjectFlag::PrecomputedReady))return fail(e,"Field object ready draw was already replayed offline");
 auto&s=states_[i];s.frame=d.initial;
 // Children enter ready before the parent tests its flag. A suppressed parent
 // still consumes its Sparkles child's global random draw and clamp.
 if(d.flags&uint32_t(FieldObjectFlag::RandomFrame))s.frame=std::min(uint32_t(random.rand_range(d.random_min,d.random_max)),d.count-1);
 s.timeout=float(1.0/d.fps);s.ready=true;s.queued=ready_flag;
 s.show_pending=!ready_flag&&(d.flags&uint32_t(FieldObjectFlag::ShowAfterIdle));
 s.visible=!ready_flag&&!s.show_pending;e.clear();return true;
}
bool FieldObjects::enter_ready(uint32_t id,bool ready_flag,std::string&e){
 const auto i=index(id);if(!prepared()||i==states_.size()||states_[i].ready||!states_[i].alive)return fail(e,"Field object ready owner/order rejected");
 const auto&d=data_.objects()[i];if(!(d.flags&uint32_t(FieldObjectFlag::PrecomputedReady))||(ready_flag&&!(d.flags&uint32_t(FieldObjectFlag::SuppressAtReady))))return fail(e,"Field object precomputed ready policy rejected");
 auto&s=states_[i];s.frame=d.resolved;s.timeout=float(1.0/d.fps);s.ready=true;s.queued=ready_flag;s.show_pending=!ready_flag&&(d.flags&uint32_t(FieldObjectFlag::ShowAfterIdle));s.visible=!ready_flag&&!s.show_pending;e.clear();return true;
}
bool FieldObjects::idle_frame(std::string&e){
 if(!prepared())return fail(e,"Field objects have no idle owner");
 for(auto&s:states_)if(s.alive&&s.ready&&s.show_pending){s.visible=true;s.show_pending=false;}
 e.clear();return true;
}
bool FieldObjects::advance_idle(double delta,std::string&e){
 if(!prepared()||!std::isfinite(delta)||delta<0||delta>1)return fail(e,"Field object idle delta rejected");
 // Godot 3.6.2 AnimatedSprite uses binary32 remaining/timeout and advances a
 // frame only when the next processing pass finds timeout <= 0. Hidden nodes
 // still process; pausing the Player does not pause these scene children.
 for(size_t i=0;i<states_.size();++i){auto&s=states_[i];const auto&d=data_.objects()[i];
  if(!s.alive||!s.ready||!(d.flags&uint32_t(FieldObjectFlag::Playing)))continue;
  float remaining=float(delta);while(remaining>0){
   if(s.timeout<=0){s.timeout=float(1.0/d.fps);if(s.frame>=d.count-1){if(d.flags&uint32_t(FieldObjectFlag::Loop))s.frame=0;}else ++s.frame;}
   const float amount=std::min(s.timeout,remaining);remaining-=amount;s.timeout-=amount;
  }
 }
 e.clear();return true;
}
bool FieldObjects::validate_story_object(uint32_t scene,std::string_view path,std::string&e)const{
 if(!prepared()||scene!=data_.scene_id())return fail(e,"Field story object scene rejected");
 for(const auto&d:data_.objects())if(d.path==path){e.clear();return true;}
 return fail(e,"Field story object path is unregistered");
}
bool FieldObjects::can_queue_free(std::string_view path,std::string&e)const{
 if(!prepared())return fail(e,"Field objects have no delete owner");
 for(size_t i=0;i<states_.size();++i)if(data_.objects()[i].path==path){
  if(!states_[i].ready||!states_[i].alive)return fail(e,"Field object delete target is unavailable");e.clear();return true;
 }
 return fail(e,"Field object delete path is unregistered");
}
bool FieldObjects::queue_free(std::string_view path,std::string&e){
 if(!can_queue_free(path,e))return false;
 for(size_t i=0;i<states_.size();++i)if(data_.objects()[i].path==path){states_[i].queued=true;break;}e.clear();return true;
}
bool FieldObjects::flush_queued(std::string&e){
 if(!prepared())return fail(e,"Field objects have no delete queue");
 for(auto&s:states_)if(s.alive&&s.queued){s.alive=false;s.visible=false;s.show_pending=false;}
 e.clear();return true;
}
}
