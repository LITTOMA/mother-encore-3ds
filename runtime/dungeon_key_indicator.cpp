#include "encore/dungeon_key_indicator.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace encore::upstream {
namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
double real(const uint8_t*p){const uint64_t u=uint64_t(word(p))|uint64_t(word(p+4))<<32;double f;std::memcpy(&f,&u,8);return f;}
bool bad(std::string&e,const char*why){e=why;return false;}
}
struct DungeonKeyIndicatorData::State {KeyIndicatorConfig config;DungeonKeysData keys;};
const KeyIndicatorConfig&DungeonKeyIndicatorData::config()const{static const KeyIndicatorConfig empty;return state_?state_->config:empty;}
DungeonKeysView DungeonKeyIndicatorData::keys()const{return state_?state_->keys.view():DungeonKeysView{};}
bool DungeonKeyIndicatorData::load_file(const char*path,std::string&e){std::vector<uint8_t>bytes;return encore::read_file(path,bytes,512*1024,e)&&load(bytes.data(),bytes.size(),e);}
bool DungeonKeyIndicatorData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<176||n>512*1024||std::memcmp(p,"ENCKUI01",8)||word(p+8)!=1||word(p+12)!=n||word(p+16)!=encore::crc32(p+32,n-32)||word(p+20)!=1||word(p+24)!=176||word(p+28))return bad(e,"Key indicator header/version/capability/CRC rejected");
 const uint32_t scene=word(p+32),offset=word(p+36),amount=word(p+40),text=word(p+44),size=word(p+48);
 if(offset!=176||amount<32||amount>n-176||text!=176+amount||uint64_t(text)+size!=n||word(p+52)!=9||word(p+56)>3||word(p+60)>1||word(p+160)!=1||word(p+168)||word(p+172))return bad(e,"Key indicator canonical sections/curve rejected");
 auto next=std::make_shared<State>();if(!next->keys.load(p+offset,amount,e)||next->keys.view().scene_id()!=scene)return bad(e,"Key indicator nested scene identity rejected");
 auto&c=next->config;c.flags=word(p+56);c.diagnostics=word(p+60);c.curve=word(p+160);c.default_curve=word(p+168);const uint32_t threshold=word(p+164);std::memcpy(&c.threshold,&threshold,4);if(c.threshold< -65535||c.threshold>65535)return bad(e,"Key indicator close threshold rejected");for(size_t i=0;i<c.parameters.size();++i){const double v=real(p+64+i*8);if(!std::isfinite(v)||std::abs(v)>65535)return bad(e,"Key indicator nonfinite/unbounded position rejected");c.parameters[i]=v;}
 for(size_t i:{4u,7u,8u})if(c.parameters[i]<=0||c.parameters[i]>60||!std::isnormal(float(c.parameters[i])))return bad(e,"Key indicator duration rejected");
 if(c.parameters[2]==c.parameters[3])return bad(e,"Key indicator open/close positions coincide");
 uint32_t cursor=0;std::array<std::string*,3>strings{{&c.initial_text,&c.prefix,&c.suffix}};
 for(size_t i=0;i<strings.size();++i){const uint32_t at=word(p+136+i*8),length=word(p+140+i*8);if(at!=cursor||cursor>size||length>256||length>size-cursor)return bad(e,"Key indicator text ownership rejected");for(uint32_t j=0;j<length;++j)if(p[text+at+j]<32||p[text+at+j]>=127)return bad(e,"Key indicator text encoding rejected");strings[i]->assign(reinterpret_cast<const char*>(p+text+at),length);cursor+=length;}
 if(cursor!=size)return bad(e,"Key indicator unowned text rejected");state_=std::move(next);e.clear();return true;
}
bool DungeonKeyIndicator::validate(uint64_t epoch,std::string&e)const{return epoch_&&epoch==epoch_&&data_.valid()?true:bad(e,"Key indicator scene epoch/resource rejected");}
bool DungeonKeyIndicator::prepare(const DungeonKeyIndicatorData&d,uint64_t epoch,std::string&e){
 if(epoch_||!d.valid()||!epoch||epoch<=last_epoch_)return bad(e,"Key indicator preparation owner/epoch rejected");
 data_=d;epoch_=last_epoch_=epoch;const auto&c=d.config();view_={};view_.position={float(c.parameters[0]),float(c.parameters[1])};view_.text=c.initial_text;view_.count=d.keys().policy().initial;view_.visible=c.flags&1;view_.open=c.flags&2;e.clear();return true;
}
void DungeonKeyIndicator::replace(Segment segment){segments_.clear();segments_.push_back(segment);cursor_=0;has_tween_=true;started_=false;root_curve_=data_.config().default_curve;view_.running=true;}
void DungeonKeyIndicator::finish_close(){const auto&c=data_.config();const auto&p=c.parameters;view_.open=false;Segment s;s.to=p[3];s.duration=float(p[4]);s.curve=c.curve;s.hide=true;replace(s);}
void DungeonKeyIndicator::start_segment(){if(cursor_>=segments_.size())return;auto&s=segments_[cursor_];if(!s.explicit_from)s.from=s.y?view_.position.y:view_.position.x;s.began=true;s.elapsed=0;}
bool DungeonKeyIndicator::update(int32_t count,std::string&e){
 const auto&c=data_.config();const auto&p=c.parameters;view_.text=c.prefix+std::to_string(count)+c.suffix;
 if(view_.position.x!=p[2])return true;
 if(!has_tween_||!view_.running){segments_.clear();cursor_=0;has_tween_=true;started_=false;root_curve_=c.curve;view_.running=true;}
 if(started_){if(!(c.diagnostics&1)||diagnostics_.size()>=256)return bad(e,"Key indicator original append failure is not admitted or diagnostic capacity exhausted");diagnostics_.push_back({epoch_,KeyIndicatorDiagnosticKind::AppendAfterStarted,count});return true;}
 if(segments_.size()+2>256)return bad(e,"Key indicator tween sequence capacity exhausted");
 Segment first;first.y=true;first.explicit_from=true;first.from=view_.position.y+p[5];first.to=view_.position.y+p[6];first.duration=float(p[7]);first.curve=root_curve_;segments_.push_back(first);
 Segment second;second.y=true;second.to=view_.position.y;second.duration=float(p[8]);second.curve=root_curve_;segments_.push_back(second);return true;
}
bool DungeonKeyIndicator::open(int32_t count,std::string&e){
 if(!update(count,e))return false;if(view_.open)return true;
 view_.open=view_.visible=true;const auto&c=data_.config();const auto&p=c.parameters;Segment s;s.to=p[2];s.duration=float(p[4]);s.curve=c.curve;replace(s);return true;
}
bool DungeonKeyIndicator::update_counter(uint64_t epoch,int32_t count,std::string&e){
 if(!validate(epoch,e))return false;const auto policy=data_.keys().policy();if(count<policy.minimum||count>policy.maximum)return bad(e,"Key indicator counter outside bound policy");
 auto next=*this;next.view_.count=count;if(count>data_.config().threshold){if(!next.open(count,e))return false;}else if(next.view_.open)next.finish_close();*this=std::move(next);e.clear();return true;
}
bool DungeonKeyIndicator::close(uint64_t epoch,std::string&e){if(!validate(epoch,e))return false;if(view_.open)finish_close();e.clear();return true;}
bool DungeonKeyIndicator::advance(uint64_t epoch,double delta,std::string&e){
 if(!validate(epoch,e)||!std::isfinite(delta)||delta<0||delta>60)return bad(e,"Key indicator delta/epoch rejected");
 if(!view_.running){e.clear();return true;}if(!started_){start_segment();started_=true;}float remaining=float(delta);
 while(remaining>0&&cursor_<segments_.size()){
  auto&s=segments_[cursor_];auto&value=s.y?view_.position.y:view_.position.x;
  s.elapsed+=remaining;const float time=std::min(s.elapsed,s.duration);
  if(time<s.duration){const float t=time/s.duration,from=float(s.from),change=float(s.to-s.from);value=s.curve?float(from+change*std::sin(t*std::acos(-1.)*.5)):from+change*time/s.duration;remaining=0;}
  else{value=float(s.to);remaining=s.elapsed-s.duration;if(s.hide)view_.visible=false;++cursor_;start_segment();}
 }
 view_.running=cursor_<segments_.size();e.clear();return true;
}
bool DungeonKeyIndicator::retire(uint64_t epoch,std::string&e){if(!validate(epoch,e))return false;data_={};epoch_=0;view_={};segments_.clear();diagnostics_.clear();has_tween_=started_=false;cursor_=0;e.clear();return true;}
}
