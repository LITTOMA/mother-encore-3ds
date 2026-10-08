#include "encore/introduction.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
uint32_t integer(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
bool fail(std::string&e,const char*s){e=s;return false;}
bool path(const std::string&s){if(s.empty()||s.front()=='/'||s.find(':')!=s.npos||s.find('\\')!=s.npos)return false;size_t at=0;while(at<s.size()){auto end=s.find('/',at);auto part=s.substr(at,end==s.npos?s.size()-at:end-at);if(part.empty()||part=="."||part=="..")return false;if(end==s.npos)return true;at=end+1;}return false;}
bool source(const std::string&s){return s.size()>6&&s.compare(0,6,"res://")==0&&path(s.substr(6));}
bool hash(const std::string&s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==s.npos;}
struct Reader{
 const uint8_t*p;size_t n;bool ok=true;
 uint32_t u(){if(n<4){ok=false;return 0;}auto x=integer(p);p+=4;n-=4;return x;}
 uint32_t count(uint32_t cap){auto x=u();if(x>cap){ok=false;return 0;}return x;}
 float f(){auto x=u();float v;std::memcpy(&v,&x,4);if(!std::isfinite(v)||std::abs(v)>100000)ok=false;return v;}
 double number(){uint64_t x=u();x|=uint64_t(u())<<32;double v;std::memcpy(&v,&x,8);if(!std::isfinite(v)||std::abs(v)>100000)ok=false;return v;}
 std::string text(){auto size=count(8192);if(size>n){ok=false;return{};}std::string s(reinterpret_cast<const char*>(p),size);p+=size;n-=size;std::u32string v;if(s.find('\0')!=s.npos||!utf8_decode(s,v))ok=false;return s;}
 IntroRect rect(){return{f(),f(),f(),f()};}
 std::vector<IntroTrack>tracks(uint32_t target,uint32_t images,float length){
  std::vector<IntroTrack>out;auto size=count(64);std::set<std::array<uint32_t,3>>seen;
  for(uint32_t i=0;i<size;++i){IntroTrack t;t.target=u();t.index=u();t.property=u();t.discrete=u();auto keys=count(256);
   if(t.target!=target&&!(target==0&&t.target==2))ok=false;
   if(t.property>3||t.discrete>1||t.index>=(t.target==0?images:t.target==1?4u:1u)||!keys)ok=false;
   if((t.target==1||t.target==2||t.target==3)&&t.property!=0)ok=false;
   if(!seen.insert({t.target,t.index,t.property}).second)ok=false;
   for(uint32_t k=0;k<keys;++k){IntroKey key;key.time=f();for(auto&v:key.value)v=f();key.transition=f();if(key.time<0||key.time>length||(k&&key.time<=t.keys.back().time))ok=false;
    if(t.target==3&&(key.value[0]<0||key.value[0]>1))ok=false;
    if(t.property==1&&(key.value[3]<0||key.value[3]>1))ok=false;
    if(t.property==2&&(key.value[0]!=0&&key.value[0]!=1))ok=false;
    if(t.property==3&&(key.value[0]<0||key.value[0]>=256||std::floor(key.value[0])!=key.value[0]))ok=false;
    t.keys.push_back(key);
   }out.push_back(std::move(t));
  }return out;
 }
 std::vector<IntroClip>clips(uint32_t target,uint32_t images){std::vector<IntroClip>out;auto size=count(16);for(uint32_t i=0;i<size;++i){IntroClip c;c.length=f();if(c.length<=0||c.length>180)ok=false;c.tracks=tracks(target,images,c.length);if(c.tracks.empty())ok=false;out.push_back(std::move(c));}return out;}
};
float ease(float t,float c){t=std::clamp(t,0.f,1.f);if(c>0)return c<1?1-std::pow(1-t,1/c):std::pow(t,c);if(c<0)return t<.5f?std::pow(t*2,-c)*.5f:(1-std::pow(1-(t-.5f)*2,-c))*.5f+.5f;return 0;}
std::array<float,4>sample(const IntroTrack&t,double time){size_t at=0;while(at+1<t.keys.size()&&t.keys[at+1].time<=time)++at;auto v=t.keys[at].value;if(!t.discrete&&at+1<t.keys.size()&&time>=t.keys[at].time){const auto&b=t.keys[at+1];const float f=ease(float((time-t.keys[at].time)/(b.time-t.keys[at].time)),t.keys[at].transition);for(size_t i=0;i<4;++i)v[i]+=(b.value[i]-v[i])*f;}return v;}
std::u32string spaceless(const std::string&s){std::u32string v,out;utf8_decode(s,v);for(auto cp:v)if(cp!=' '&&cp!='\n')out.push_back(cp);return out;}
}
bool IntroductionData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<24||n>1024*1024||std::memcmp(p,"ENCINTRO",8)||integer(p+8)!=1||integer(p+12)!=n||integer(p+20)!=1)return fail(e,"Introduction schema/size/capability rejected");
 if(crc(p+24,n-24)!=integer(p+16))return fail(e,"Introduction CRC rejected");
 IntroductionData d;Reader r{p+24,n-24};d.canvas_width=r.f();d.canvas_height=r.f();d.font_catalog=r.text();d.music=r.text();d.music_gain=r.f();d.music_fade=r.f();for(auto&v:d.hint_timing)v=r.f();
 for(auto&v:d.hint_curve){v=r.f();if(v==0||std::abs(v)>100)r.ok=false;}for(auto&v:d.fade_shader)v=r.f();d.background_color=r.u();auto bb=r.count(4096);if(bb>r.n||!d.blackbars.load(r.p,bb,e))r.ok=false;else{r.p+=bb;r.n-=bb;}
 d.finish_stop_slot=r.u();if(d.finish_stop_slot>1)r.ok=false;
 for(size_t i=0;i<5;++i)if(d.fade_shader[i]<=0||d.fade_shader[i]>8192)r.ok=false;
 if(d.canvas_width<=0||d.canvas_height<=0||d.canvas_width>1024||d.canvas_height>1024||!path(d.font_catalog)||!source(d.music)||d.music_gain<-120||d.music_gain>24||d.music_fade<=0||d.music_fade>60)r.ok=false;
 for(auto v:d.hint_timing)if(v<=0||v>60)r.ok=false;
 auto resources=r.count(64);std::set<std::string>paths;
 for(uint32_t i=0;i<resources;++i){IntroResource a;a.path=r.text();a.source=r.text();a.sha256=r.text();a.width=r.u();a.height=r.u();a.columns=r.u();a.rows=r.u();a.frame_count=r.u();a.source_width=r.u();a.source_height=r.u();a.trim_x=r.u();a.trim_y=r.u();a.bytes=r.u();a.crc32=r.u();
  if(!path(a.path)||a.path.size()<4||a.path.compare(a.path.size()-4,4,".t3x")!=0||!path(a.source)||!hash(a.sha256)||!paths.insert(a.path).second||!a.width||!a.height||a.width>1024||a.height>1024||!a.columns||!a.rows||a.columns>32||a.rows>32||a.width%a.columns||a.height%a.rows||!a.frame_count||a.frame_count>a.columns*a.rows||!a.bytes||a.bytes>16*1024*1024||!a.source_width||!a.source_height||a.source_width>1024||a.source_height>1024||a.trim_x>a.source_width||a.trim_y>a.source_height||a.width/a.columns>a.source_width-a.trim_x||a.height/a.rows>a.source_height-a.trim_y)r.ok=false;
  d.resources.push_back(std::move(a));
 }
 auto locales=r.count(32);std::set<std::string>codes;
 for(uint32_t i=0;i<locales;++i){IntroLocale l;l.code=r.text();l.old_font=r.text();l.now_font=r.text();l.hint_font=r.text();l.punctuation=r.text();l.skip=r.text();for(auto&v:l.character_spacing){v=r.f();if(std::abs(v)>100)r.ok=false;}auto count=r.count(128);for(uint32_t j=0;j<count;++j)l.texts.push_back(r.text());
  if(l.code.empty()||!codes.insert(l.code).second||!path(l.old_font)||!path(l.now_font)||!path(l.hint_font)||l.punctuation.empty()||l.skip.empty()||l.texts.empty())r.ok=false;
  for(const auto&t:l.texts)if(t.empty()||spaceless(t).empty())r.ok=false;
  d.locales.push_back(std::move(l));
 }
 auto scenes=r.count(2);uint32_t first=0;
 for(uint32_t i=0;i<scenes;++i){IntroScene s;s.delay=r.number();s.length=r.number();s.speed=r.number();s.slow=r.number();s.pause=r.number();s.hide=r.number();s.hide_y=r.number();s.pitch_min=r.number();s.pitch_max=r.number();s.text_first=r.u();s.text_count=r.u();s.round_images=r.u();s.text_clip=r.rect();auto images=r.count(16);if(s.round_images>images)r.ok=false;
  if(s.delay<=0||s.delay>10||s.length<=0||s.length>180||s.speed<=0||s.speed>1||s.slow<=0||s.slow>1||s.pause<=0||s.pause>10||s.hide<=0||s.hide>10||s.pitch_min<=0||s.pitch_max>2||s.pitch_min>s.pitch_max||s.text_first!=first||!s.text_count||s.text_count>128||s.text_clip.width<=0||s.text_clip.height<=0||!images)r.ok=false;
  first+=s.text_count;
  s.background.rect=r.rect();s.background.color=r.u();s.text_color=r.u();s.hint_color=r.u();s.line_spacing=r.f();s.hint_rect=r.rect();if(s.background.rect.width<=0||s.background.rect.height<=0||s.line_spacing<0||s.line_spacing>100||s.hint_rect.width<=0||s.hint_rect.height<=0)r.ok=false;
  for(uint32_t j=0;j<images;++j){auto resource=r.u();IntroImage image;image.rect=r.rect();image.alpha=r.f();auto visible=r.u();image.visible=visible!=0;image.frame=r.u();if(resource>=d.resources.size()||image.rect.width<=0||image.rect.height<=0||image.alpha<0||image.alpha>1||visible>1)r.ok=false;else{const auto&a=d.resources[resource];image.path=a.path;image.columns=a.columns;image.rows=a.rows;if(image.frame>=a.frame_count)r.ok=false;}s.resources.push_back(resource);s.images.push_back(std::move(image));}
  s.tracks=r.tracks(0,images,s.length);auto events=r.count(256);uint32_t captions=0;
  for(uint32_t j=0;j<events;++j){IntroEvent v;v.time=r.f();v.kind=r.u();v.arg=r.u();v.source=r.text();v.name=r.text();if(v.time<0||v.time>s.length+1||(j&&v.time<s.events.back().time)||v.kind>7||(v.kind==5&&(!source(v.source)||v.name.empty()||v.arg>1))||(v.kind==6&&v.arg>=5)||(v.kind==7&&v.arg!=0)||(v.kind<5&&v.arg!=0)||(v.kind!=5&&(!v.source.empty()||!v.name.empty())))r.ok=false;if(v.kind==0)++captions;s.events.push_back(std::move(v));}
  if(captions!=s.text_count)r.ok=false;
  d.scenes.push_back(std::move(s));
 }
 for(const auto&l:d.locales)if(l.texts.size()!=first)r.ok=false;
 d.borders=r.clips(1,0);auto masks=r.count(4);for(uint32_t i=0;i<masks;++i){IntroMask m;m.rect=r.rect();m.color=r.u();if(m.rect.width<=0||m.rect.height<=0)r.ok=false;d.border_rects.push_back(m);}
 auto clouds=r.clips(0,4);if(clouds.size()!=1)r.ok=false;else d.cloud=std::move(clouds[0]);
 d.fades=r.clips(3,0);auto mostly=r.count(6);for(uint32_t i=0;i<mostly;++i){float v=r.f();if(v<=0||i>=d.fades.size()||v>d.fades[i].length)r.ok=false;d.fade_mostly.push_back(v);}
 auto doors=r.count(3);for(uint32_t i=0;i<doors;++i){IntroDoor door;door.destination.scene=r.text();door.destination.x=r.f();door.destination.y=r.f();door.destination.dx=r.f();door.destination.dy=r.f();auto respawn=r.u(),unpause=r.u();door.destination.set_respawn=respawn!=0;door.destination.unpause=unpause!=0;door.in_kind=r.u();door.out_kind=r.u();door.in_speed=r.f();door.out_speed=r.f();if(!source(door.destination.scene)||respawn>1||unpause>1||door.in_kind>2||door.out_kind>2||door.in_speed<=0||door.out_speed<=0||door.in_speed>100||door.out_speed>100||std::abs(door.destination.dx)+std::abs(door.destination.dy)!=1)r.ok=false;d.doors.push_back(door);}
 for(auto&s:d.text_sounds){s=r.text();if(!source(s))r.ok=false;}
 if(!r.ok||r.n||d.resources.empty()||d.locales.empty()||d.scenes.size()!=2||d.scenes[1].images.size()<4||d.borders.size()!=5||d.border_rects.size()!=4||d.fades.size()!=6||d.fade_mostly.size()!=6||d.doors.size()!=3)return fail(e,"Introduction record/reference/finite/domain rejected");
 for(const auto&t:d.cloud.tracks)for(const auto&k:t.keys)if(t.index>=d.scenes[1].images.size()||(t.property==3&&k.value[0]>=d.resources[d.scenes[1].resources[t.index]].frame_count))return fail(e,"Introduction cloud frame reference rejected");
 for(const auto&s:d.scenes)for(const auto&t:s.tracks)if(t.target==0&&t.property==3)for(const auto&k:t.keys)if(k.value[0]>=d.resources[s.resources[t.index]].frame_count)return fail(e,"Introduction image frame reference rejected");
 d.valid_=true;*this=std::move(d);e.clear();return true;
}
bool IntroductionData::load_file(const char*path_name,std::string&e){FILE*f=std::fopen(path_name,"rb");if(!f)return fail(e,"Introduction unavailable");if(std::fseek(f,0,SEEK_END)){std::fclose(f);return fail(e,"Introduction seek rejected");}const long size=std::ftell(f);if(size<24||size>1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);return fail(e,"Introduction file size rejected");}std::vector<uint8_t>v(static_cast<size_t>(size));const bool ok=std::fread(v.data(),1,v.size(),f)==v.size();std::fclose(f);return ok?load(v.data(),v.size(),e):fail(e,"Introduction read rejected");}
const IntroLocale*IntroductionData::locale(const std::string&code)const{for(const auto&l:locales)if(l.code==code)return &l;return nullptr;}
bool Introduction::begin(const IntroductionData&d,SourceRandom&random,const std::string&code,float w,float h,std::string&e){const auto*l=d.locale(code);if(!d.valid()||!l||!std::isfinite(w)||!std::isfinite(h)||w<d.canvas_width||h<d.canvas_height||w>1024||h>1024)return fail(e,"Introduction locale/viewport/data rejected");*this=Introduction{};data_=&d;locale_=l;random_=&random;width_=w;height_=h;focus_x_=w/2;focus_y_=h/2;blackbars_=d.blackbars;enter_door(0);e.clear();return true;}
void Introduction::enter_scene(uint32_t index){scene_=index;next_=0;visible_=0;event_=0;time_=-data_->scenes[index].delay;text_.clear();text_time_=pause_=0;hide_time_=cloud_time_=hint_time_=-1;border_=UINT32_MAX;border_time_=0;finished_=process_=true;speed_=data_->scenes[index].speed;text_origin_=0;now_finishing_=false;blackbars_.reset();}
void Introduction::enter_door(uint32_t index){door_=index;phase_=IntroPhase::DoorIn;phase_time_=0;}
void Introduction::hide_text(){if(hide_time_<0)hide_time_=0;}
void Introduction::next_text(){const auto&s=data_->scenes[scene_];if(next_>=s.text_count)return;text_=locale_->texts[s.text_first+next_++];
 if(scene_==0){size_t start=0;while(start<text_.size()){auto end=text_.find('\n',start);if(end==text_.npos)end=text_.size();size_t count=0;utf8_count(std::string_view(text_).substr(start,end-start),count);if(count%2){text_.insert(end," ");++end;}start=end+1;}}
 visible_=0;text_time_=pause_=0;hide_time_=-1;text_origin_=0;finished_=false;process_=true;
}
void Introduction::event(const IntroEvent&e){switch(e.kind){case 0:next_text();break;case 1:hide_text();break;case 2:{IntroAudio a;a.kind=IntroAudioKind::FadeMusic;a.lane=IntroAudioLane::Music;a.seconds=data_->music_fade;audio_.push_back(a);break;}case 3:speed_=data_->scenes[scene_].slow;break;case 4:speed_=data_->scenes[scene_].speed;break;case 5:{IntroAudio a;a.source_path=e.source;a.name=e.name;a.effect_slot=e.arg;audio_.push_back(a);break;}case 6:border_=e.arg;border_time_=0;break;case 7:cloud_time_=0;break;default:break;}}
void Introduction::finish_now(){if(now_finishing_)return;now_finishing_=true;for(const auto&e:data_->scenes[scene_].events)if(e.kind==5&&e.arg==data_->finish_stop_slot){IntroAudio a;a.kind=IntroAudioKind::StopNamed;a.effect_slot=e.arg;a.name=e.name;a.source_path=e.source;audio_.push_back(a);break;}playtime_=true;enter_door(uint32_t(data_->doors.size()-1));}
bool Introduction::step(double delta,bool accept_hint,bool skip,std::string&e){
 if(!data_||!data_->valid()||!random_||!std::isfinite(delta)||delta<0||delta>1)return fail(e,"Introduction step rejected");
 if(phase_==IntroPhase::Complete){e.clear();return true;}
 if(accept_hint)hint_time_=0;
 if(hint_time_>=0)hint_time_+=delta;
 if(phase_==IntroPhase::DoorIn){const auto&door=data_->doors[door_];phase_time_+=delta*door.in_speed;if(phase_time_>=data_->fades[door.in_kind*2].length){phase_time_=0;if(door_+1<data_->doors.size())enter_scene(door_);phase_=IntroPhase::DoorOut;}e.clear();return true;}
 bool door_revealing=false;
 if(phase_==IntroPhase::DoorOut){
  const auto&door=data_->doors[door_];phase_time_+=delta*door.out_speed;
  const bool done=phase_time_>=data_->fades[door.out_kind*2+1].length;
  if(door_+1==data_->doors.size()){if(done){phase_time_=0;phase_=IntroPhase::Complete;}e.clear();return true;}
  // Source _ready owns its timer independently of the door AnimationPlayer.
  // Preserve the overlay while executing any scene time already activated.
  door_revealing=!done;if(done)phase_time_=0;
 }
 const auto&s=data_->scenes[scene_];
 if(skip&&time_>=0&&time_<s.length){if(scene_==0){IntroAudio a;a.kind=IntroAudioKind::FadeMusic;a.lane=IntroAudioLane::Music;a.seconds=data_->music_fade;audio_.push_back(a);enter_door(1);}else finish_now();e.clear();return true;}
 const double previous_time=time_;time_+=delta;
 if(time_<0){if(!door_revealing)phase_=IntroPhase::Waiting;e.clear();return true;}
 const double active_delta=std::max(0.,delta+std::min(previous_time,0.));
 if(!door_revealing)phase_=time_>=s.length?IntroPhase::TextTail:IntroPhase::Playing;
 if(scene_==1&&!blackbars_.update(true,active_delta))return fail(e,"Introduction blackbars rejected");
 if(border_!=UINT32_MAX)border_time_+=active_delta;
 if(cloud_time_>=0)cloud_time_+=active_delta;
 while(event_<s.events.size()&&s.events[event_].time<=std::min(time_,double(s.length))){event(s.events[event_++]);}
 if(scene_==0&&time_>=s.length){enter_door(1);e.clear();return true;}
 if(hide_time_>=0){hide_time_+=active_delta;if(scene_==1&&next_==s.text_count&&hide_time_>=s.hide){finish_now();e.clear();return true;}}
 if(pause_>0){pause_-=active_delta;if(pause_<=0)process_=true;else{e.clear();return true;}}
 if(process_&&!finished_){const auto chars=spaceless(text_);text_time_+=active_delta;if(text_time_>speed_){++visible_;text_time_=0;IntroAudio a;a.lane=IntroAudioLane::Text;a.source_path=data_->text_sounds[scene_];a.pitch=scene_==1?random_->rand_range(s.pitch_min,s.pitch_max):1;audio_.push_back(a);std::u32string punctuation;utf8_decode(locale_->punctuation,punctuation);if(visible_<chars.size()&&punctuation.find(chars[visible_-1])!=punctuation.npos){pause_=s.pause;process_=false;}}
  if(visible_>=chars.size()){visible_=uint32_t(chars.size());finished_=true;text_time_=0;if(scene_==1){pause_=s.pause;process_=false;}}
 }else if(process_&&finished_&&scene_==1&&next_==s.text_count)hide_text();
 e.clear();return true;
}
IntroductionPose Introduction::pose()const{IntroductionPose p;p.phase=phase_;if(!data_)return p;p.scene=scene_;p.scene_visible=!(phase_==IntroPhase::DoorIn&&door_==0)&&!house_ready();p.font_catalog=data_->font_catalog;p.font_source=scene_==0?locale_->old_font:locale_->now_font;p.hint_font_source=locale_->hint_font;p.skip_text=locale_->skip;p.text=text_;p.visible_characters=visible_;p.center_text_vertically=scene_==0;
 p.text_character_spacing=locale_->character_spacing[scene_];p.hint_character_spacing=locale_->character_spacing[2];
 const float ox=(width_-data_->canvas_width)/2,oy=(height_-data_->canvas_height)/2;const auto&s=data_->scenes[scene_];p.images=s.images;p.text_clip=s.text_clip;auto masks=data_->border_rects;p.background_color=data_->background_color;p.backgrounds={s.background};p.backgrounds[0].rect.x+=ox;p.backgrounds[0].rect.y+=oy;p.text_color=s.text_color;p.hint_color=s.hint_color;p.line_spacing=s.line_spacing;p.hint_rect=s.hint_rect;p.hint_rect.x+=ox;p.hint_rect.y+=oy;
 auto apply=[&](const IntroTrack&t,double time){auto v=sample(t,time);if(t.target==0){auto&i=p.images[t.index];if(t.property==0){i.rect.x=v[0];i.rect.y=v[1];}else if(t.property==1)i.alpha=v[3];else if(t.property==2)i.visible=v[0]!=0;else i.frame=uint32_t(v[0]);}else if(t.target==1){masks[t.index].rect.x=v[0];masks[t.index].rect.y=v[1];}else if(t.target==2){p.text_clip.x=v[0];p.text_clip.y=v[1];}};
 // Prior to an animation's first key the authored scene property survives.
 if(time_>=0)for(const auto&t:s.tracks)if(time_>=t.keys.front().time)apply(t,std::min(time_,double(s.length)));
 if(border_!=UINT32_MAX)for(const auto&t:data_->borders[border_].tracks)apply(t,border_time_);
 if(cloud_time_>=0)for(const auto&t:data_->cloud.tracks)apply(t,cloud_time_);
 for(size_t n=0;n<p.images.size();++n){auto&i=p.images[n];const auto&r=data_->resources[s.resources[n]];if(process_&&n<s.round_images){i.rect.x=std::round(i.rect.x);i.rect.y=std::round(i.rect.y);}i.rect.x+=ox+float(r.trim_x);i.rect.y+=oy+float(r.trim_y);i.rect.width=float(r.width/r.columns);i.rect.height=float(r.height/r.rows);}
 for(size_t n=0;n<masks.size();++n){auto&m=masks[n];m.rect.x+=ox;m.rect.y+=oy;if(n==0)m.rect={0,0,width_,std::max(0.f,m.rect.y+m.rect.height)};else if(n==1)m.rect={0,m.rect.y,width_,std::max(0.f,height_-m.rect.y)};else if(n==2)m.rect={0,0,std::max(0.f,m.rect.x+m.rect.width),height_};else m.rect={m.rect.x,0,std::max(0.f,width_-m.rect.x),height_};}p.masks=std::move(masks);p.text_clip.x+=ox;p.text_clip.y+=oy;
 if(scene_==1){const auto bars=blackbars_.pose(width_,height_);const auto color=bars.color;const auto rgba_color=(color&255)<<24|(color&0xff00)<<8|(color&0xff0000)>>8|(color>>24);for(const auto&b:bars.bars)p.masks.push_back({{b.x,b.y,b.w,b.h},rgba_color});}
 if(hide_time_>=0){const float weight=float(std::clamp(hide_time_/s.hide,0.,1.));p.text_y=text_origin_+float(s.hide_y-text_origin_)*weight;p.text_center_weight=1-weight;}else p.text_y=text_origin_;
 if(hint_time_>=0){const auto&h=data_->hint_timing;const double out=h[0]+h[2];if(hint_time_<h[0])p.hint_alpha=ease(float(hint_time_/h[0]),data_->hint_curve[0]);else if(hint_time_<out)p.hint_alpha=1;else p.hint_alpha=1-ease(float((hint_time_-out)/h[1]),data_->hint_curve[1]);}
 if(phase_==IntroPhase::DoorIn||phase_==IntroPhase::DoorOut){const auto&door=data_->doors[door_];p.door.index=door_;p.door.incoming=phase_==IntroPhase::DoorOut;p.door.kind=p.door.incoming?door.out_kind:door.in_kind;p.door.destination=door.destination;p.door.cut=door_transition_cut(*data_,p.door.kind,p.door.incoming,phase_time_);
  p.door.masks=door_transition_masks(*data_,p.door.kind,p.door.cut,width_,height_,focus_x_,focus_y_);
 }
 return p;
}
float door_transition_cut(const IntroductionData&d,uint32_t kind,bool incoming,double time){
 const size_t index=size_t(kind)*2+(incoming?1:0);
 if(index>=d.fades.size()||d.fades[index].tracks.empty())return 0;
 return sample(d.fades[index].tracks.front(),time)[0];
}
double door_transition_length(const IntroductionData&d,uint32_t kind,bool incoming){
 const size_t index=size_t(kind)*2+(incoming?1:0);return index<d.fades.size()?d.fades[index].length:0;
}
double door_transition_mostly(const IntroductionData&d,uint32_t kind,bool incoming){
 const size_t index=size_t(kind)*2+(incoming?1:0);return index<d.fade_mostly.size()?d.fade_mostly[index]:0;
}
std::vector<IntroMask> door_transition_masks(const IntroductionData&d,uint32_t kind,float cut,float width,float height,float focus_x,float focus_y){
 std::vector<IntroMask> masks;
 if(kind>2||d.border_rects.empty())return masks;
 const auto&v=d.fade_shader;
 const auto color=d.border_rects.front().color;
 if(kind==0){const float x=std::clamp(v[0]-cut,0.f,1.f),alpha=x*x*(3-2*x);masks.push_back({{0,0,width,height},(color&0xffffff00)|uint32_t(std::round(alpha*255))});}
 else {const float cx=(kind==1?focus_x:width/2)+v[5],cy=(kind==1?focus_y:height/2)+v[6];const double ratio=v[1]/v[2];
  for(int y=0;y<int(height);++y){const double dy=(y+.5-cy)/v[4],remaining=double(cut)*cut-dy*dy;
   if(remaining<=0){masks.push_back({{0,float(y),width,1},color});continue;}
   const double radius=std::sqrt(remaining)*v[3]/ratio;
   const int first=std::clamp(int(std::floor(cx-radius-.5))+1,0,int(width)),last=std::clamp(int(std::ceil(cx+radius-.5))-1,-1,int(width)-1);
   if(first>last)masks.push_back({{0,float(y),width,1},color});else{if(first>0)masks.push_back({{0,float(y),float(first),1},color});if(last+1<int(width))masks.push_back({{float(last+1),float(y),width-float(last+1),1},color});}}
 }
 return masks;
}
std::vector<IntroAudio>Introduction::take_audio(){std::vector<IntroAudio>v;v.swap(audio_);return v;}
}
