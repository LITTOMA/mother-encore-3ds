#include "encore/field_props.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include "encore/field_swing.hpp"
#include "encore/audio_data.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t*p){auto bits=word(p);float v;std::memcpy(&v,&bits,4);return v;}
bool fail(std::string&e,const char*why){e=why;return false;}
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
bool safe(const std::string&s){if(s.empty()||s.front()=='/'||s.find("..")!=std::string::npos)return false;for(auto c:s)if(c<' '||c>'~'||c=='\\'||c==':')return false;return true;}
}
bool FieldPropsData::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,128*1024,e)&&load(b.data(),b.size(),e);}
bool FieldPropsData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<144||n>128*1024)return fail(e,"Field props size rejected");
 if(std::memcmp(p,"ENCPRP01",8)||word(p+8)!=1||word(p+12)!=n||word(p+20)<1||word(p+20)>3||word(p+24)||word(p+28)||word(p+52)!=5||word(p+56)||word(p+60))return fail(e,"Field props schema/capability/reserved rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail(e,"Field props pin rejected");}
 if(encore::crc32(p+32,n-32)!=word(p+16))return fail(e,"Field props checksum rejected");
 const auto capability=word(p+20);const uint32_t strides[]={1,capability==3?176u:96u,1,capability==3?116u:104u,8},limits[]={16384,1,65536,128,64};uint32_t off[5],count[5];size_t cursor=144;
 for(unsigned i=0;i<5;++i){auto*d=p+64+16*i;off[i]=word(d+4);count[i]=word(d+8);
  if(word(d)!=i+1||word(d+12)!=strides[i]||!count[i]||count[i]>limits[i]||off[i]%4||off[i]<cursor||off[i]>n||count[i]>(n-off[i])/strides[i])return fail(e,"Field props directory rejected");
  for(size_t k=cursor;k<off[i];++k)if(p[k])return fail(e,"Field props padding rejected");cursor=off[i]+count[i]*strides[i];
 }
 if(cursor!=n)return fail(e,"Field props trailing bytes rejected");
 FieldPropsData v;v.capability_=capability;
 auto text=[&](const uint8_t*r,std::string&value){auto first=word(r),size=word(r+4);if(!size||size>1024||first>count[0]||size>count[0]-first)return false;value.assign(reinterpret_cast<const char*>(p+off[0]+first),size);return safe(value);};
 std::set<std::string>names;for(uint32_t i=0;i<count[4];++i){std::string name;if(!text(p+off[4]+i*8,name)||!names.insert(name).second||name.find('/')!=std::string::npos)return fail(e,"Field prop flag name rejected");v.flags_.push_back(std::move(name));}
 auto flag=[&](uint32_t i){return i==kFieldNone||i<v.flags_.size();};const auto*r=p+off[1];auto&policy=v.policy_;
 policy.scene=word(r);policy.hidden=word(r+4);policy.idle=word(r+8);policy.grow=word(r+12);policy.broken=word(r+16);policy.clone_frame=word(r+20);policy.bat_flag=word(r+24);policy.width=word(r+28);policy.height=word(r+32);
 if(!policy.scene||policy.bat_flag>=v.flags_.size()||!policy.width||policy.width>1024||!policy.height||policy.height>1024||!text(r+36,policy.hard)||!text(r+44,policy.easy)||!text(r+52,policy.sound)||!text(r+60,policy.texture)||!text(r+84,policy.clone_parent)||(word(p+20)==1&&word(r+92)))return fail(e,"Field prop policy rejected");
 if(capability>=2)policy.clone_after=word(r+92);
 if(capability==3){auto&c=policy.contact;c.target_layer=word(r+96);c.target_mask=word(r+100);c.target_flags=word(r+104);c.emitter_layer=word(r+108);c.emitter_mask=word(r+112);c.emitter_flags=word(r+116);c.gain_db=real(r+120);c.pitch=real(r+124);c.bus=word(r+128);c.pause_mode=word(r+132);c.swing_bytes=word(r+136);c.swing_crc=word(r+140);std::copy(r+144,r+176,c.sound_sha256.begin());
  if((c.target_layer|c.target_mask|c.emitter_layer|c.emitter_mask)&~0xfffffu||c.target_flags>3||c.emitter_flags>3||!(c.target_flags&1)||!(c.emitter_flags&2)||!(c.target_mask&c.emitter_layer)||!std::isfinite(c.gain_db)||c.gain_db<-256||c.gain_db>128||!std::isfinite(c.pitch)||c.pitch<=0||c.pitch>16||c.bus!=uint32_t(FieldPropSoundBus::Sfx)||c.pause_mode||!c.swing_bytes||c.swing_bytes>64*1024||std::all_of(c.sound_sha256.begin(),c.sound_sha256.end(),[](uint8_t b){return b==0;}))return fail(e,"Field prop contact/audio policy rejected");
 }
 for(unsigned i=0;i<4;++i){policy.rumble[i]=real(r+68+i*4);if(!std::isfinite(policy.rumble[i])||policy.rumble[i]<0||policy.rumble[i]>(i==3?60.f:1.f))return fail(e,"Field prop rumble rejected");}
 if(!v.clips_.load(p+off[2],count[2],e))return false;
 std::set<uint32_t>clip_ids;for(auto id:{policy.hidden,policy.idle,policy.grow,policy.broken})if(!v.clips_.clip(id)||!clip_ids.insert(id).second)return fail(e,"Field prop animation binding rejected");
 if(policy.clone_frame>=v.clips_.frame_count())return fail(e,"Field prop clone frame rejected");
 std::set<uint32_t>ids,sprites,bodies,notices;std::set<std::string>paths;
 for(uint32_t i=0;i<count[3];++i){r=p+off[3]+i*strides[3];FieldProp row;row.id=word(r);row.sprite=word(r+4);row.body=word(r+8);row.clone_item=word(r+12);row.appear=word(r+16);row.finish=word(r+20);row.callback=word(r+24);
  if(!row.id||!ids.insert(row.id).second||!sprites.insert(row.sprite).second||!bodies.insert(row.body).second||!flag(row.appear)||!flag(row.finish)||(capability==1&&word(r+28))||(row.callback!=kFieldNone&&row.finish==kFieldNone)||(capability>=2&&(policy.clone_after==kFieldNone||policy.clone_after<=row.clone_item)))return fail(e,"Field prop identity/flags/callback/order rejected");
  if(capability>=2){
   const auto reference=word(r+28);if((row.callback==kFieldNone)!=(reference==0))return fail(e,"Field prop callback path binding rejected");
   if(reference){const auto at=reference-1;if(at>=count[0])return fail(e,"Field prop callback path span rejected");size_t size=0;while(size<count[0]-at&&size<=512&&p[off[0]+at+size])++size;
    if(!size||size>512||size==count[0]-at)return fail(e,"Field prop callback path termination rejected");row.callback_path.assign(reinterpret_cast<const char*>(p+off[0]+at),size);if(!safe(row.callback_path))return fail(e,"Field prop callback path rejected");}
  }
  if(capability==3){row.notice=word(r+104);if(row.notice==kFieldNone||!notices.insert(row.notice).second||!text(r+108,row.path)||row.path.size()>512||!paths.insert(row.path).second)return fail(e,"Field prop contact path/notice owner rejected");}
  Vec2*coords[]={&row.position,&row.sprite_center,&row.view_center,&row.view_extents,&row.hit_center,&row.hit_extents,&row.interact_center,&row.interact_extents,&row.body_extents};
  for(unsigned j=0;j<9;++j){*coords[j]={real(r+32+j*8),real(r+36+j*8)};if(!std::isfinite(coords[j]->x)||!std::isfinite(coords[j]->y)||std::fabs(coords[j]->x)>1000000||std::fabs(coords[j]->y)>1000000)return fail(e,"Field prop geometry rejected");}
  for(auto ext:{row.view_extents,row.hit_extents,row.interact_extents,row.body_extents})if(ext.x<=0||ext.y<=0||ext.x>4096||ext.y>4096)return fail(e,"Field prop extents rejected");
  v.props_.push_back(row);
 }
 v.encoded_bytes_=uint32_t(n);v.encoded_crc_=encore::crc32(p,n);v.valid_=true;*this=std::move(v);e.clear();return true;
}
bool FieldPropsData::bind(RoomView room,FieldMapView map,std::string&e)const{
 if(!valid_||!room.valid()||!map.valid()||policy_.scene!=map.scene_id()||policy_.scene!=room.scene().stable_id)return fail(e,"Field prop scene binding rejected");
 for(const auto&name:flags_){bool found=false;for(uint32_t i=0;i<room.flag_count();++i)found|=room.string(room.flag(i).name_string)==name;if(!found)return fail(e,"Field prop flag domain rejected");}
 if(rendering_bound()){
  if(policy_.clone_after>=map.count(FieldMapSection::Items))return fail(e,"Field prop clone insertion is outside map");
  const auto tail=map.item(policy_.clone_after);bool in_static=false;
  for(uint32_t i=0;i<map.count(FieldMapSection::Groups);++i){const auto g=map.group(i);if(policy_.clone_after>=g.first&&policy_.clone_after-g.first<g.count)in_static=g.kind==uint32_t(FieldGroupKind::Static);}
  if(!in_static||tail.kind!=uint16_t(FieldItemKind::LayerCells)||tail.count!=1||tail.conditions.count||map.text(map.layer(tail.first).name).substr(0,policy_.clone_parent.size()+1)!=policy_.clone_parent+"/")return fail(e,"Field prop clone subtree insertion differs");
 }
 for(const auto&r:props_){
  if(contact_bound()){if(r.notice>=map.count(FieldMapSection::Notices))return fail(e,"Field prop notice owner is outside map");const auto n=map.notice(r.notice);if(n.kind!=2||map.text(n.path)!=r.path||!same(n.position,r.position))return fail(e,"Field prop notice source identity/position differs");}
  if(r.sprite>=map.count(FieldMapSection::Sprites)||r.body>=map.count(FieldMapSection::Bodies)||r.clone_item>=map.count(FieldMapSection::Items))return fail(e,"Field prop map owner rejected");
  const auto s=map.sprite(r.sprite);const auto b=map.body(r.body);const auto item=map.item(r.clone_item);
  if(s.w!=policy_.width||s.h!=policy_.height||!same(s.position,{r.sprite_center.x-policy_.width*.5f,r.sprite_center.y-policy_.height*.5f})||s.flags||s.openable!=kFieldNoOpenable||s.conditions.count||b.conditions.count||b.shape_count!=1||item.kind!=uint16_t(FieldItemKind::LayerCells)||item.count!=1||item.conditions.count)return fail(e,"Field prop sprite/body/layer ownership differs");
  const auto layer=map.layer(item.first);if(map.text(layer.name)!=policy_.clone_parent||!same(layer.position,{})||layer.flags&uint32_t(FieldLayerFlag::YSort))return fail(e,"Field prop clone parent identity/transform/order differs");
  const auto sh=map.shape(b.shape_first);if(sh.kind!=uint16_t(FieldShapeKind::Convex)||sh.count!=4||!same(sh.minimum,{r.position.x-r.body_extents.x,r.position.y-r.body_extents.y})||!same(sh.maximum,{r.position.x+r.body_extents.x,r.position.y+r.body_extents.y}))return fail(e,"Field prop body geometry differs");
  if(r.callback!=kFieldNone&&(r.callback>=map.count(FieldMapSection::Boundaries)||map.boundary(r.callback).kind!=uint16_t(FieldBoundaryKind::Area)))return fail(e,"Field prop deferred target rejected");
  if(rendering_bound()&&r.callback!=kFieldNone&&map.text(map.boundary(r.callback).path)!=r.callback_path)return fail(e,"Field prop deferred source identity differs");
 }
 e.clear();return true;
}
bool FieldPropsData::matches_action(const FieldSwingData&data,std::string&e)const{
 if(!contact_bound()||!data.valid()||data.encoded_bytes()!=policy_.contact.swing_bytes||data.encoded_crc()!=policy_.contact.swing_crc)return fail(e,"Field prop contact action companion differs");e.clear();return true;
}
bool FieldPropsData::matches_audio(const AudioBank&bank,AudioAsset&out,std::string&e)const{
 AudioAsset asset;if(!contact_bound()||!bank.find_source("res://"+policy_.sound,policy_.contact.sound_sha256,asset)||asset.loops())return fail(e,"Field prop sound source/digest/loop differs");out=asset;e.clear();return true;
}
bool FieldProps::initialize(const FieldPropsData&d,const std::vector<bool>&flags,std::string&e){
 if(!d.valid()||flags.size()!=d.flags().size())return fail(e,"Field props initial flags rejected");FieldProps v;v.data_=d;
 for(const auto&r:d.props()){FieldPropState s;if(!s.animation.initialize(d.clips(),e)||!s.animation.play(r.appear==kFieldNone||flags[r.appear]?d.policy().idle:d.policy().hidden,e))return false;v.states_.push_back(std::move(s));}
 *this=std::move(v);e.clear();return true;
}
bool FieldProps::available(uint32_t i,std::string&e)const{if(!ready()||i>=states_.size()||!states_[i].alive)return fail(e,"Field prop handle unavailable");return true;}
bool FieldProps::set_visible(uint32_t i,bool value,std::string&e){if(!available(i,e))return false;states_[i].visible=value;e.clear();return true;}
bool FieldProps::grow(uint32_t i,std::string&e){
 if(!available(i,e)||states_[i].grow_waiters>=64||events_.size()>=512)return fail(e,"Field prop grow capacity/owner rejected");auto&s=states_[i];if(!s.animation.play(data_.policy().grow,e))return false;++s.grow_waiters;events_.push_back({FieldPropEventKind::Rumble,i});e.clear();return true;
}
bool FieldProps::hit(uint32_t i,std::string&e){
 if(!available(i,e)||clones_.size()>=512||events_.size()>=512)return fail(e,"Field prop hit capacity/owner rejected");auto&s=states_[i];const auto&r=data_.props()[i];const bool visible=s.animation.pose().visible;
 if(!s.animation.play(data_.policy().broken,e))return false;clones_.push_back({i,data_.policy().clone_frame,r.sprite_center,visible});s.prompt=false;events_.push_back({FieldPropEventKind::Sound,i});e.clear();return true;
}
bool FieldProps::can_hit(const std::vector<uint32_t>&indices,std::string&e)const{
 if(indices.size()>128||indices.size()>512-clones_.size()||indices.size()>512-events_.size())return fail(e,"Field prop contact batch capacity rejected");
 std::set<uint32_t>seen;for(auto i:indices)if(!available(i,e)||!seen.insert(i).second)return fail(e,"Field prop contact batch owner rejected");e.clear();return true;
}
bool FieldProps::interact(uint32_t i,const std::vector<bool>&flags,std::string&e){
 if(!available(i,e)||flags.size()!=data_.flags().size()||events_.size()>=512)return fail(e,"Field prop interaction flags/capacity rejected");
 if(states_[i].visible)events_.push_back({flags[data_.policy().bat_flag]?FieldPropEventKind::EasyDialogue:FieldPropEventKind::HardDialogue,i});e.clear();return true;
}
bool FieldProps::advance(double delta,const std::vector<bool>&flags,std::string&e){
 if(!ready()||flags.size()!=data_.flags().size()||!std::isfinite(delta)||delta<0||delta>1)return fail(e,"Field prop advance rejected");
 for(uint32_t i=0;i<states_.size();++i){auto&s=states_[i];if(!s.alive)continue;if(!s.animation.advance(delta,e))return false;
  for(auto id:s.animation.take_completed()){
   const auto&r=data_.props()[i];if(id==data_.policy().broken&&r.finish!=kFieldNone){s.queued=true;s.deferred=r.callback!=kFieldNone&&!flags[r.finish];}
   // Source yield waits for any animation_finished, including an interrupted
   // Grow followed by Break; sparse Idle keys apply on the next process pass.
   if(s.grow_waiters){s.grow_waiters=0;if(!s.animation.play(data_.policy().idle,e))return false;}
  }
 }
 e.clear();return true;
}
bool FieldProps::flush_deferred(FieldPropHost&host,std::string&e){
 if(!ready())return fail(e,"Field props are unprepared");
 for(uint32_t i=0;i<states_.size();++i)if(states_[i].alive&&states_[i].queued&&states_[i].deferred&&!host.can_call(data_.props()[i].callback,e))return false;
 // Godot's message queue runs deferred calls before its delete queue. Keep
 // every queued object alive throughout all callback dispatches this pass.
 for(uint32_t i=0;i<states_.size();++i){auto&s=states_[i];if(s.alive&&s.queued&&s.deferred&&!host.call(data_.props()[i].callback,i,e))return false;}
 for(auto&s:states_)if(s.alive&&s.queued){s.alive=false;s.deferred=false;s.grow_waiters=0;}
 e.clear();return true;
}
}
