#include "encore/field_steps.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
namespace encore::upstream {namespace {
constexpr uint32_t strides[]={1,24,56,64};constexpr uint32_t polygon_strides[]={1,24,56,72,16,8,32};constexpr char pin[]="7d9246600fffe518408f5830d4848635019005a3";
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float f(const uint8_t*p){auto n=u(p);float v;std::memcpy(&v,&n,4);return v;}
FieldText t(const uint8_t*p){return {u(p),u(p+4)};}
bool identifier(std::string_view s){if(s.empty()||s.size()>32||s.front()<'a'||s.front()>'z')return false;for(unsigned char c:s)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;return true;}
bool path(std::string_view s){if(s.empty()||s.size()>512)return false;for(unsigned char c:s)if(c<32||c>126||c==':'||c=='\\')return false;size_t start=0;while(start<=s.size()){auto end=s.find('/',start);if(end==s.npos)end=s.size();auto part=s.substr(start,end-start);if(part.empty()||part=="."||part=="..")return false;if(end==s.size())break;start=end+1;}return true;}
}
uint32_t FieldStepsView::count(uint32_t k)const{return p_&&k>=1&&k<=u(p_+52)?u(p_+64+(k-1)*16+8):0;}
const uint8_t*FieldStepsView::record(uint32_t k,uint32_t i)const{return i<count(k)?p_+u(p_+64+(k-1)*16+4)+size_t(i)*u(p_+64+(k-1)*16+12):nullptr;}
uint32_t FieldStepsView::scene_id()const{return p_?u(p_+24):0;}
uint32_t FieldStepsView::capability()const{return p_?u(p_+20):0;}
uint32_t FieldStepsView::sound_count()const{return count(3);}uint32_t FieldStepsView::area_count()const{return count(4);}
std::string_view FieldStepsView::text(FieldText s)const{return s.length&&s.offset<=count(1)&&s.length<=count(1)-s.offset?std::string_view(reinterpret_cast<const char*>(record(1,0))+s.offset,s.length):std::string_view{};}
FieldStepPolicy FieldStepsView::policy()const{auto*r=record(2,0);return r?FieldStepPolicy{u(r),t(r+4),t(r+12)}:FieldStepPolicy{};}
FieldStepSound FieldStepsView::sound(uint32_t i)const{FieldStepSound s;auto*r=record(3,i);if(r){s.name=t(r);s.source=t(r+8);std::memcpy(s.sha256.data(),r+16,32);s.id=u(r+48);}return s;}
FieldStepArea FieldStepsView::area(uint32_t i)const{auto*r=record(4,i);return r?FieldStepArea{t(r),u(r+8),u(r+12),{f(r+16),f(r+20)},{f(r+24),f(r+28)},t(r+32),t(r+40),u(r+48),u(r+52),u(r+56)!=0}:FieldStepArea{};}
uint32_t FieldStepsView::area_id(uint32_t i)const{auto*r=record(4,i);return r&&capability()==2?u(r+60):0;}
uint32_t FieldStepsView::first_part(uint32_t i)const{auto*r=record(4,i);return r&&capability()==2?u(r+64):0;}
uint32_t FieldStepsView::part_count(uint32_t i)const{auto*r=record(4,i);return r&&capability()==2?u(r+68):0;}
uint32_t FieldStepsView::part_count()const{return count(5);}uint32_t FieldStepsView::point_count()const{return count(6);}
FieldStepPart FieldStepsView::part(uint32_t i)const{auto*r=record(5,i);return r?FieldStepPart{u(r),u(r+4),u(r+8)}:FieldStepPart{};}
Vec2 FieldStepsView::point(uint32_t i)const{auto*r=record(6,i);return r?Vec2{f(r),f(r+4)}:Vec2{};}
uint32_t FieldStepsView::shadow_count()const{return count(7);}
FieldStepShadow FieldStepsView::shadow(uint32_t i)const{auto*r=record(7,i);return r?FieldStepShadow{t(r),f(r+8),u(r+12),u(r+16)!=0,u(r+20)!=0}:FieldStepShadow{};}
uint32_t FieldStepsView::find_shadow(std::string_view name)const{for(uint32_t i=0;i<shadow_count();++i)if(text(shadow(i).name)==name)return i;return kFieldNone;}
bool FieldStepsView::shadow_initial_playing()const{auto*r=record(2,0);return r&&capability()==2&&u(r+20)!=0;}
uint32_t FieldStepsView::find_sound(std::string_view name)const{for(uint32_t i=0;i<sound_count();++i)if(text(sound(i).name)==name)return i;return kFieldNone;}
bool FieldStepsView::matches(const AudioBank&bank,std::string&e)const{
 if(!valid()){e="Footstep bank binding has no metadata";return false;}
 for(uint32_t i=0;i<sound_count();++i){const auto s=sound(i);AudioAsset a;if(!bank.find(s.id,a)||a.source_path!=text(s.source)||a.source_sha256!=s.sha256||!a.loops()||a.loop_start){e="Footstep loop media source/identity mismatch";return false;}}
 e.clear();return true;
}
bool FieldStepsData::load(const uint8_t*p,size_t n,std::string&e){auto fail=[&](const char*s){e=s;return false;};
 if(!p||n<128||n>1048576)return fail("Footstep resource size rejected");
 const auto capability=u(p+20);const bool polygon=capability==2;const uint32_t section_count=polygon?7:4,header=64+section_count*16;const auto*layout=polygon?polygon_strides:strides;
 if(std::memcmp(p,"ENCSTEP1",8)||u(p+8)!=1||u(p+12)!=n||(capability!=1&&!polygon)||!u(p+24)||u(p+28)||u(p+52)!=section_count||u(p+56)||u(p+60))return fail("Footstep schema/capability/scene rejected");
 if(n<header||(!polygon&&n>65536))return fail("Footstep resource size rejected");
 for(size_t i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail("Footstep source pin rejected");}
 if(encore::crc32(p+32,n-32)!=u(p+16))return fail("Footstep checksum rejected");
 size_t cursor=header;for(uint32_t i=0;i<section_count;++i){auto*d=p+64+i*16;const auto off=u(d+4),count=u(d+8);if(u(d)!=i+1||u(d+12)!=layout[i])return fail("Footstep section directory rejected");if(!count){if(off)return fail("Footstep empty section offset rejected");continue;}if(off%4||off<cursor||off>n||count>(n-off)/layout[i])return fail("Footstep section range rejected");for(size_t j=cursor;j<off;++j)if(p[j])return fail("Footstep padding rejected");cursor=size_t(off)+size_t(count)*layout[i];}
 if(cursor!=n)return fail("Footstep trailing bytes rejected");FieldStepsView v;v.p_=p;
 if(!v.count(1)||v.count(1)>32768||v.count(2)!=1||!v.sound_count()||v.sound_count()>16||!v.area_count()||v.area_count()>64)return fail("Footstep capacity rejected");
 auto text_ok=[&](FieldText t,bool empty=false){return t.length?t.offset<=v.count(1)&&t.length<=v.count(1)-t.offset:empty&&!t.offset;};
 const auto policy=v.policy();if(policy.default_sound>=v.sound_count()||!text_ok(policy.shadow)||!text_ok(policy.slot)||!identifier(v.text(policy.shadow))||!identifier(v.text(policy.slot))||u(v.record(2,0)+20)>(polygon?1u:0u))return fail("Footstep initial policy rejected");
 if(polygon){
  if(!v.part_count()||v.part_count()>4096||!v.point_count()||v.point_count()>16384||!v.shadow_count()||v.shadow_count()>16)return fail("Footstep polygon/shadow capacity rejected");
  std::set<std::string_view>modes;
  for(uint32_t i=0;i<v.shadow_count();++i){const auto s=v.shadow(i);auto*r=v.record(7,i);if(!text_ok(s.name)||!identifier(v.text(s.name))||!modes.insert(v.text(s.name)).second||!std::isfinite(s.fps)||s.fps<=0||s.fps>60||!s.frames||s.frames>64||u(r+16)>1||u(r+20)>1||u(r+24)||u(r+28))return fail("Footstep Shadow policy rejected");}
  if(v.find_shadow(v.text(policy.shadow))==kFieldNone)return fail("Footstep initial Shadow is undeclared");
 }
 std::set<uint32_t>ids;std::set<std::string_view>names,paths;for(uint32_t i=0;i<v.sound_count();++i){const auto s=v.sound(i);auto source=v.text(s.source);bool digest=false;for(auto b:s.sha256)digest|=b!=0;
  if(!s.id||!ids.insert(s.id).second||!text_ok(s.name)||!identifier(v.text(s.name))||!names.insert(v.text(s.name)).second||!text_ok(s.source)||source.substr(0,6)!="res://"||!path(source.substr(6))||!paths.insert(source).second||!digest||u(v.record(3,i)+52)!=1)return fail("Footstep sound identity/source/loop rejected");}
 paths.clear();std::set<uint32_t>area_ids;uint32_t part_cursor=0,point_cursor=0;
 for(uint32_t i=0;i<v.area_count();++i){const auto a=v.area(i);auto*r=v.record(4,i);auto number=[](float x){return std::isfinite(x)&&std::fabs(x)<=1000000;};
  const auto shadow_ok=[&](FieldText name){return text_ok(name,true)&&(!name.length||v.find_shadow(v.text(name))!=kFieldNone);};
  const bool shadows=polygon?shadow_ok(a.enter_shadow)&&shadow_ok(a.exit_shadow):text_ok(a.enter_shadow)&&v.text(a.enter_shadow)==v.text(policy.shadow)&&text_ok(a.exit_shadow,true)&&!a.exit_shadow.length;
  if(!text_ok(a.path)||!path(v.text(a.path))||!paths.insert(v.text(a.path)).second||a.enter>=v.sound_count()||(a.exit!=kFieldNone&&a.exit>=v.sound_count())||!number(a.center.x)||!number(a.center.y)||!number(a.extents.x)||!number(a.extents.y)||a.extents.x<=0||a.extents.y<=0||!shadows||!a.layer||!a.mask||u(r+56)>1||(!polygon&&u(r+60)))return fail("Footstep area geometry/filter/shadow rejected");
  if(!polygon)continue;
  const auto identity=v.area_id(i),first=v.first_part(i),count=v.part_count(i);
  if(!identity||identity==kFieldNone||!area_ids.insert(identity).second||first!=part_cursor||!count||count>64||count>v.part_count()-part_cursor)return fail("Footstep polygon area identity/span rejected");
  Vec2 minimum{},maximum{};bool initial=true;
  for(uint32_t j=0;j<count;++j){const auto part=v.part(part_cursor);auto*record=v.record(5,part_cursor++);
   if(part.area!=i||part.first!=point_cursor||part.count<3||part.count>64||part.count>v.point_count()-point_cursor||u(record+12))return fail("Footstep convex part owner/span rejected");
   double signed_area=0;bool positive=false,negative=false;
   for(uint32_t k=0;k<part.count;++k){const auto point=v.point(part.first+k),next=v.point(part.first+(k+1)%part.count);
    if(!number(point.x)||!number(point.y))return fail("Footstep polygon coordinate rejected");
    if(initial){minimum=maximum=point;initial=false;}else{minimum.x=std::fmin(minimum.x,point.x);minimum.y=std::fmin(minimum.y,point.y);maximum.x=std::fmax(maximum.x,point.x);maximum.y=std::fmax(maximum.y,point.y);}
    signed_area+=double(point.x)*next.y-double(next.x)*point.y;
    for(uint32_t l=0;l<part.count;++l){const auto other=v.point(part.first+l);if(l!=k&&other.x==point.x&&other.y==point.y)return fail("Footstep repeated polygon vertex rejected");const double cross=(double(next.x)-point.x)*(double(other.y)-point.y)-(double(next.y)-point.y)*(double(other.x)-point.x);positive|=cross>0;negative|=cross<0;}
   }
   if(!std::isfinite(signed_area)||signed_area==0||(positive&&negative))return fail("Footstep nonconvex/degenerate part rejected");point_cursor+=part.count;
  }
  if(a.center.x!=(minimum.x+maximum.x)*0.5f||a.center.y!=(minimum.y+maximum.y)*0.5f||a.extents.x!=(maximum.x-minimum.x)*0.5f||a.extents.y!=(maximum.y-minimum.y)*0.5f)return fail("Footstep polygon bounds mismatch");
 }
 if(polygon&&(part_cursor!=v.part_count()||point_cursor!=v.point_count()))return fail("Footstep orphaned polygon records rejected");
 bytes_.assign(p,p+n);e.clear();return true;
}
bool FieldStepsData::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,1048576,e)&&load(b.data(),b.size(),e);}
bool FieldRunLoop::initialize(FieldStepsView data,std::string_view inherited,std::string&e){
 if(prepared()||!data.valid()){e="Run-loop preparation owner rejected";return false;}
 const auto sound=data.find_sound(inherited);if(sound==kFieldNone){e="Inherited run sound is not admitted";return false;}
 data_=data;wanted_=registered_=sound;for(uint32_t i=0;i<data.area_count();++i)enabled_.push_back(data.area(i).enabled);e.clear();return true;
}
bool FieldRunLoop::set_running(bool enabled,bool paused,bool move,bool climbing,std::string&e){
 if(!prepared()){e="Run-loop source operation has no metadata";return false;}
 running_=enabled;
 if(!paused&&move&&enabled&&!climbing){
  if(registered_!=wanted_){registered_=wanted_;playing_=true;events_.push_back({FieldRunEventKind::ReplacePlay,wanted_});}
  else if(!playing_){playing_=true;events_.push_back({FieldRunEventKind::Play,wanted_});}
 }
 if(playing_&&!enabled){playing_=false;events_.push_back({FieldRunEventKind::Stop,registered_});}
 e.clear();return true;
}
bool FieldRunLoop::stop_playback(std::string&e){
 if(!prepared()){e="Run-loop stop has no metadata";return false;}
 if(playing_){playing_=false;events_.push_back({FieldRunEventKind::Stop,registered_});}e.clear();return true;
}
bool FieldRunLoop::apply_walk_calls(const WalkAudioCalls&calls,std::string&e){
 if(!prepared()||calls.count>calls.calls.size()){e="Run-loop MOVE calls have no valid owner/count";return false;}
 FieldRunLoop next=*this;
 for(uint8_t i=0;i<calls.count;++i){switch(calls.calls[i]){
  case WalkAudioCall::SetRunningFalse:if(!next.set_running(false,false,true,false,e))return false;break;
  case WalkAudioCall::SetRunningTrue:if(!next.set_running(true,false,true,false,e))return false;break;
  case WalkAudioCall::StopIfPlaying:if(!next.stop_playback(e))return false;break;
  default:e="Unknown run-loop MOVE operation";return false;
 }}
 *this=std::move(next);e.clear();return true;
}
bool FieldRunLoop::set_area_enabled(uint32_t index,bool enabled,std::string&e){
 if(!prepared()||index>=enabled_.size()){e="Run-loop area enable identity rejected";return false;}
 enabled_[index]=enabled;e.clear();return true;
}
bool FieldRunLoop::area_event(uint32_t index,bool entered,bool player,std::string&e){
 if(!prepared()||index>=enabled_.size()){e="Run-loop area event identity rejected";return false;}
 if(player&&enabled_[index]){const auto a=data_.area(index);const auto sound=entered?a.enter:a.exit;if(sound!=kFieldNone)wanted_=sound;}
 e.clear();return true;
}

bool FieldStepShadowState::initialize(FieldStepsView data,std::string&e){
 if(prepared()||!data.valid()||data.capability()!=2){e="Shadow preparation requires fresh polygon stepping metadata";return false;}
 for(uint32_t i=0;i<data.shadow_count();++i){const double duration=1.0/double(data.shadow(i).fps);if(!std::isfinite(duration)||duration<=0||duration>std::numeric_limits<float>::max()){e="Shadow frame duration is not finitely executable";return false;}}
 const auto initial=data.find_shadow(data.text(data.policy().shadow));if(initial==kFieldNone){e="Shadow initial policy is undeclared";return false;}
 data_=data;animation_=initial;frame_=0;playing_=data.shadow_initial_playing();timeout_=playing_?float(1.0/data_.shadow(animation_).fps):0;e.clear();return true;
}
bool FieldStepShadowState::set_animation(std::string_view name,std::string&e){
 if(!prepared()){e="Shadow source call has no metadata";return false;}const auto index=data_.find_shadow(name);if(index==kFieldNone){e="Unknown Shadow animation rejected";return false;}
 const bool reset=animation_!=index;if(reset){animation_=index;frame_=0;}if(reset||!playing_)timeout_=float(1.0/data_.shadow(animation_).fps);playing_=true;e.clear();return true;
}
bool FieldStepShadowState::set_frame(uint32_t frame,std::string&e){
 if(!prepared()||frame>=data_.shadow(animation_).frames){e="Shadow checked frame range rejected";return false;}if(frame_!=frame){frame_=frame;if(playing_)timeout_=float(1.0/data_.shadow(animation_).fps);}e.clear();return true;
}
bool FieldStepShadowState::advance_idle(double delta,bool process,std::string&e){
 if(!prepared()||!std::isfinite(delta)||delta<0||delta>1){e="Shadow idle clock owner/delta rejected";return false;}
 if(!playing_||!process){e.clear();return true;}
 // Godot3.6.2 AnimatedSprite,3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8.
 // Its original MIT copyright notice is retained in THIRD_PARTY_NOTICES and
 // docs/licenses/Godot-3.6.2-LICENSE.txt.
 // Editor UPDATE_VITAL_ONLY throttling is outside this ordinary game clock.
 // Keep binary32 remaining/timeout and defer frame change until timeout<=0.
 const auto mode=data_.shadow(animation_);float remaining=float(delta);
 while(remaining>0){if(timeout_<=0){timeout_=float(1.0/mode.fps);if(frame_>=mode.frames-1){if(mode.loop)frame_=0;}else ++frame_;}const float amount=std::fmin(timeout_,remaining);remaining-=amount;timeout_-=amount;}
 e.clear();return true;
}
bool apply_step_callback(FieldRunLoop&loop,FieldStepShadowState*shadow,uint32_t index,bool entered,FieldStepBody kind,std::string&e){
 const bool party=kind==FieldStepBody::Player||kind==FieldStepBody::Party;
 if(!loop.prepared()||loop.content().capability()!=2||index>=loop.content().area_count()||(!party&&kind!=FieldStepBody::Other)||(party&&(!shadow||!shadow->prepared()||shadow->content().bytes_identity()!=loop.content().bytes_identity()))||(!party&&shadow)){e="Stepping callback body/metadata owner rejected";return false;}
 auto next=loop;if(!next.area_event(index,entered,kind==FieldStepBody::Player,e))return false;
 if(party){auto next_shadow=*shadow;const auto area=loop.content().area(index);const auto mode=entered?area.enter_shadow:area.exit_shadow;if(mode.length&&!next_shadow.set_animation(loop.content().text(mode),e))return false;*shadow=next_shadow;}
 loop=std::move(next);e.clear();return true;
}
bool step_polygon_overlap(FieldStepsView data,uint32_t index,const Vec2*body,size_t count,bool&overlap,std::string&e){
 auto fail=[&](){e="Stepping convex body/area query rejected";return false;};
 if(!data.valid()||data.capability()!=2||index>=data.area_count()||!body||count<3||count>64)return fail();
 bool positive=false,negative=false;double area=0;
 for(size_t i=0;i<count;++i){const auto p=body[i],q=body[(i+1)%count];if(!std::isfinite(p.x)||!std::isfinite(p.y)||std::fabs(p.x)>1000000||std::fabs(p.y)>1000000)return fail();area+=double(p.x)*q.y-double(q.x)*p.y;
  for(size_t j=0;j<count;++j){const auto r=body[j];if(i!=j&&p.x==r.x&&p.y==r.y)return fail();const double cross=(double(q.x)-p.x)*(double(r.y)-p.y)-(double(q.y)-p.y)*(double(r.x)-p.x);positive|=cross>0;negative|=cross<0;}
 }
 if(!std::isfinite(area)||area==0||(positive&&negative))return fail();
 auto separated=[](const Vec2*a,size_t an,const Vec2*b,size_t bn){
  for(size_t i=0;i<an;++i){const auto p=a[i],q=a[(i+1)%an];const double x=double(q.y)-p.y,y=double(p.x)-q.x;double amin=double(a[0].x)*x+double(a[0].y)*y,amax=amin,bmin=double(b[0].x)*x+double(b[0].y)*y,bmax=bmin;
   for(size_t j=1;j<an;++j){const double value=double(a[j].x)*x+double(a[j].y)*y;amin=std::fmin(amin,value);amax=std::fmax(amax,value);}
   for(size_t j=1;j<bn;++j){const double value=double(b[j].x)*x+double(b[j].y)*y;bmin=std::fmin(bmin,value);bmax=std::fmax(bmax,value);}
   if(amax<bmin||bmax<amin)return true;
  }return false;
 };
 bool result=false;
 for(uint32_t i=0;i<data.part_count(index);++i){const auto part=data.part(data.first_part(index)+i);std::array<Vec2,64>points{};for(uint32_t j=0;j<part.count;++j)points[j]=data.point(part.first+j);if(!separated(body,count,points.data(),part.count)&&!separated(points.data(),part.count,body,count)){result=true;break;}}
 overlap=result;e.clear();return true;
}

}
