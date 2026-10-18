#include "encore/field_steps.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
constexpr uint32_t strides[]={1,24,56,64};constexpr char pin[]="7d9246600fffe518408f5830d4848635019005a3";
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float f(const uint8_t*p){auto n=u(p);float v;std::memcpy(&v,&n,4);return v;}
FieldText t(const uint8_t*p){return {u(p),u(p+4)};}
bool identifier(std::string_view s){if(s.empty()||s.size()>32||s.front()<'a'||s.front()>'z')return false;for(unsigned char c:s)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;return true;}
bool path(std::string_view s){if(s.empty()||s.size()>512)return false;for(unsigned char c:s)if(c<32||c>126||c==':'||c=='\\')return false;size_t start=0;while(start<=s.size()){auto end=s.find('/',start);if(end==s.npos)end=s.size();auto part=s.substr(start,end-start);if(part.empty()||part=="."||part=="..")return false;if(end==s.size())break;start=end+1;}return true;}
}
uint32_t FieldStepsView::count(uint32_t k)const{return p_&&k>=1&&k<=4?u(p_+64+(k-1)*16+8):0;}
const uint8_t*FieldStepsView::record(uint32_t k,uint32_t i)const{return i<count(k)?p_+u(p_+64+(k-1)*16+4)+size_t(i)*strides[k-1]:nullptr;}
uint32_t FieldStepsView::scene_id()const{return p_?u(p_+24):0;}
uint32_t FieldStepsView::sound_count()const{return count(3);}uint32_t FieldStepsView::area_count()const{return count(4);}
std::string_view FieldStepsView::text(FieldText s)const{return s.length&&s.offset<=count(1)&&s.length<=count(1)-s.offset?std::string_view(reinterpret_cast<const char*>(record(1,0))+s.offset,s.length):std::string_view{};}
FieldStepPolicy FieldStepsView::policy()const{auto*r=record(2,0);return r?FieldStepPolicy{u(r),t(r+4),t(r+12)}:FieldStepPolicy{};}
FieldStepSound FieldStepsView::sound(uint32_t i)const{FieldStepSound s;auto*r=record(3,i);if(r){s.name=t(r);s.source=t(r+8);std::memcpy(s.sha256.data(),r+16,32);s.id=u(r+48);}return s;}
FieldStepArea FieldStepsView::area(uint32_t i)const{auto*r=record(4,i);return r?FieldStepArea{t(r),u(r+8),u(r+12),{f(r+16),f(r+20)},{f(r+24),f(r+28)},t(r+32),t(r+40),u(r+48),u(r+52),u(r+56)!=0}:FieldStepArea{};}
uint32_t FieldStepsView::find_sound(std::string_view name)const{for(uint32_t i=0;i<sound_count();++i)if(text(sound(i).name)==name)return i;return kFieldNone;}
bool FieldStepsView::matches(const AudioBank&bank,std::string&e)const{
 if(!valid()){e="Footstep bank binding has no metadata";return false;}
 for(uint32_t i=0;i<sound_count();++i){const auto s=sound(i);AudioAsset a;if(!bank.find(s.id,a)||a.source_path!=text(s.source)||a.source_sha256!=s.sha256||!a.loops()||a.loop_start){e="Footstep loop media source/identity mismatch";return false;}}
 e.clear();return true;
}
bool FieldStepsData::load(const uint8_t*p,size_t n,std::string&e){auto fail=[&](const char*s){e=s;return false;};
 if(!p||n<128||n>65536)return fail("Footstep resource size rejected");
 if(std::memcmp(p,"ENCSTEP1",8)||u(p+8)!=1||u(p+12)!=n||u(p+20)!=1||!u(p+24)||u(p+28)||u(p+52)!=4||u(p+56)||u(p+60))return fail("Footstep schema/capability/scene rejected");
 for(size_t i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail("Footstep source pin rejected");}
 if(encore::crc32(p+32,n-32)!=u(p+16))return fail("Footstep checksum rejected");
 size_t cursor=128;for(uint32_t i=0;i<4;++i){auto*d=p+64+i*16;const auto off=u(d+4),count=u(d+8);if(u(d)!=i+1||u(d+12)!=strides[i])return fail("Footstep section directory rejected");if(!count){if(off)return fail("Footstep empty section offset rejected");continue;}if(off%4||off<cursor||off>n||count>(n-off)/strides[i])return fail("Footstep section range rejected");for(size_t j=cursor;j<off;++j)if(p[j])return fail("Footstep padding rejected");cursor=size_t(off)+size_t(count)*strides[i];}
 if(cursor!=n)return fail("Footstep trailing bytes rejected");FieldStepsView v;v.p_=p;
 if(!v.count(1)||v.count(1)>32768||v.count(2)!=1||!v.sound_count()||v.sound_count()>16||!v.area_count()||v.area_count()>64)return fail("Footstep capacity rejected");
 auto text_ok=[&](FieldText t,bool empty=false){return t.length?t.offset<=v.count(1)&&t.length<=v.count(1)-t.offset:empty&&!t.offset;};
 const auto policy=v.policy();if(policy.default_sound>=v.sound_count()||!text_ok(policy.shadow)||!text_ok(policy.slot)||!identifier(v.text(policy.shadow))||!identifier(v.text(policy.slot))||u(v.record(2,0)+20))return fail("Footstep initial policy rejected");
 std::set<uint32_t>ids;std::set<std::string_view>names,paths;for(uint32_t i=0;i<v.sound_count();++i){const auto s=v.sound(i);auto source=v.text(s.source);bool digest=false;for(auto b:s.sha256)digest|=b!=0;
  if(!s.id||!ids.insert(s.id).second||!text_ok(s.name)||!identifier(v.text(s.name))||!names.insert(v.text(s.name)).second||!text_ok(s.source)||source.substr(0,6)!="res://"||!path(source.substr(6))||!paths.insert(source).second||!digest||u(v.record(3,i)+52)!=1)return fail("Footstep sound identity/source/loop rejected");}
 paths.clear();for(uint32_t i=0;i<v.area_count();++i){const auto a=v.area(i);auto*r=v.record(4,i);auto number=[](float x){return std::isfinite(x)&&std::fabs(x)<=1000000;};
  if(!text_ok(a.path)||!path(v.text(a.path))||!paths.insert(v.text(a.path)).second||a.enter>=v.sound_count()||(a.exit!=kFieldNone&&a.exit>=v.sound_count())||!number(a.center.x)||!number(a.center.y)||!number(a.extents.x)||!number(a.extents.y)||a.extents.x<=0||a.extents.y<=0||!text_ok(a.enter_shadow)||v.text(a.enter_shadow)!=v.text(policy.shadow)||!text_ok(a.exit_shadow,true)||a.exit_shadow.length||!a.layer||!a.mask||u(r+56)>1||u(r+60))return fail("Footstep area geometry/filter/shadow rejected");}
 bytes_.assign(p,p+n);e.clear();return true;
}
bool FieldStepsData::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,65536,e)&&load(b.data(),b.size(),e);}
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
bool FieldRunLoop::set_area_enabled(uint32_t index,bool enabled,std::string&e){
 if(!prepared()||index>=enabled_.size()){e="Run-loop area enable identity rejected";return false;}
 enabled_[index]=enabled;e.clear();return true;
}
bool FieldRunLoop::area_event(uint32_t index,bool entered,bool player,std::string&e){
 if(!prepared()||index>=enabled_.size()){e="Run-loop area event identity rejected";return false;}
 if(player&&enabled_[index]){const auto a=data_.area(index);const auto sound=entered?a.enter:a.exit;if(sound!=kFieldNone)wanted_=sound;}
 e.clear();return true;
}

}
