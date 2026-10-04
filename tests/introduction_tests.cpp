#include "encore/introduction.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
using namespace encore::upstream;
namespace {int checks=0,failed=0;void check(bool v,const char*s){++checks;if(!v){++failed;std::cerr<<"FAIL "<<s<<'\n';}}
uint32_t u(const std::vector<uint8_t>&v,size_t p){return uint32_t(v[p])|uint32_t(v[p+1])<<8|uint32_t(v[p+2])<<16|uint32_t(v[p+3])<<24;}
void put(std::vector<uint8_t>&v,size_t p,uint32_t x){for(int i=0;i<4;++i)v[p+i]=uint8_t(x>>(8*i));}
void fix(std::vector<uint8_t>&v){put(v,12,uint32_t(v.size()));uint32_t c=~0u;for(size_t i=24;i<v.size();++i){c^=v[i];for(int j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}put(v,16,~c);}
size_t text(const std::vector<uint8_t>&v,size_t p){return p+4+u(v,p);}
}
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>raw((std::istreambuf_iterator<char>(f)),{});IntroductionData data;std::string error;check(data.load(raw.data(),raw.size(),error),"actual original pack loads");if(!data.valid())return 1;
 auto reject=[&](std::vector<uint8_t>v){const auto before=data.resources.front().path;check(!data.load(v.data(),v.size(),error),"corruption rejected");check(data.valid()&&data.resources.front().path==before,"failed load preserves admitted state");};
 auto bad=raw;put(bad,8,2);reject(bad);bad=raw;put(bad,20,2);reject(bad);bad=raw;bad.back()^=1;reject(bad);bad=raw;bad.push_back(0);fix(bad);reject(bad);bad=raw;put(bad,24,0x7fc00000);fix(bad);reject(bad);
 size_t at=text(raw,text(raw,32));at+=20+8+28+4;const auto bb=u(raw,at);at+=8+bb;const auto resource_count=at;check(u(raw,at)==data.resources.size(),"resource offset schema");at+=4;const auto path_position=at;at=text(raw,text(raw,text(raw,at)));const auto geometry=at;
 bad=raw;put(bad,resource_count,0xffffffff);fix(bad);reject(bad);bad=raw;bad[path_position+4]=':';fix(bad);reject(bad);bad=raw;put(bad,geometry+8,0);fix(bad);reject(bad);bad=raw;put(bad,geometry+28,0xffffffff);fix(bad);reject(bad);
 size_t scene_position=resource_count+4;for(uint32_t i=0;i<data.resources.size();++i){scene_position=text(raw,text(raw,text(raw,scene_position)))+44;}const auto locale_count=u(raw,scene_position);scene_position+=4;
 for(uint32_t i=0;i<locale_count;++i){for(int j=0;j<6;++j)scene_position=text(raw,scene_position);scene_position+=12;const auto texts=u(raw,scene_position);scene_position+=4;for(uint32_t j=0;j<texts;++j)scene_position=text(raw,scene_position);}
 scene_position+=4;const auto images=u(raw,scene_position+100);size_t track_position=scene_position+152+images*32;check(u(raw,track_position)==data.scenes.front().tracks.size(),"track offset schema");track_position+=4;
 bad=raw;put(bad,track_position,99);fix(bad);reject(bad);bad=raw;put(bad,track_position+4,0xffffffff);fix(bad);reject(bad);bad=raw;put(bad,track_position+12,2);fix(bad);reject(bad);bad=raw;put(bad,track_position+24,0x7f800000);fix(bad);reject(bad);
 for(uint32_t i=0;i<data.scenes.front().tracks.size();++i)track_position+=20+u(raw,track_position+16)*24;
 track_position+=4;
 bad=raw;put(bad,track_position+4,99);fix(bad);reject(bad);bad=raw;bad[path_position+4]=0xff;fix(bad);reject(bad);
 for(size_t n:{size_t(0),size_t(23),size_t(24),path_position,geometry,raw.size()-1}){bad.assign(raw.begin(),raw.begin()+n);if(n>=24)fix(bad);reject(bad);}
 check(data.house_destination().x==520&&data.house_destination().y==397&&data.house_destination().dy==1&&data.house_destination().set_respawn,"source final Door -7 and respawn semantics");
 for(const auto&locale:data.locales){SourceRandom random(124),expected(124);Introduction intro;check(intro.begin(data,random,locale.code,400,240,error),"native400x240 entry");check(!intro.pose().scene_visible,"naming owner survives initial fade");uint32_t captions=0,last_scene=UINT32_MAX;std::string last;bool cloud=false,tail=false,house=false,unpause=false;size_t glyphs=0,fade_count=0,teleport=0,thunder=0,stops=0;uint64_t house_rng=0;
  for(int frame=0;frame<16000&&!intro.complete();++frame){check(intro.step(1./60,false,false,error),"natural frame accepted");auto pose=intro.pose();if(pose.scene!=last_scene){last_scene=pose.scene;last.clear();}if(!pose.text.empty()&&pose.text!=last){++captions;last=pose.text;}if(pose.scene==1&&pose.images.size()>3&&pose.images[3].visible&&pose.images[3].frame==29)cloud=true;if(pose.phase==IntroPhase::TextTail&&pose.scene==1)tail=true;
   for(const auto&a:intro.take_audio()){if(a.kind==IntroAudioKind::FadeMusic){++fade_count;check(a.seconds==data.music_fade,"source fade_all duration");}else if(a.kind==IntroAudioKind::StopNamed){++stops;check(a.effect_slot==0,"stop source teleport slot");}else if(a.lane==IntroAudioLane::Text&&pose.scene==1){++glyphs;const double pitch=expected.rand_range(data.scenes[1].pitch_min,data.scenes[1].pitch_max);check(a.pitch==pitch,"exact source global RNG pitch");}else if(a.lane==IntroAudioLane::Effect){a.effect_slot?++thunder:++teleport;}}
   if(intro.house_ready()){if(!house){house=true;house_rng=random.state();}check(random.state()==house_rng,"intro no RNG after HouseReady");}unpause|=intro.house_unpaused();
  }check(intro.complete()&&house&&unpause&&intro.playtime_started(),"natural chain closes House entry and unpause");check(captions==locale.texts.size(),"all original captions visited");check(cloud&&glyphs&&random.state()==expected.state(),"cloud and glyph RNG source stream");check(fade_count==1&&teleport==1&&thunder==1&&stops==1,"complete original audio timeline");
  // The source 116.5 hide key is preserved as data but never executed by the
  // nonlooping 116-second old scene; two scene transitions must still close.
  check(data.scenes[0].events.back().time>data.scenes[0].length,"unreachable source key retained");if(locale.code=="en")check(tail,"final source text outlasts 22-second animation");
 }
 SourceRandom random(999);Introduction skipped;check(skipped.begin(data,random,"en",320,180,error),"explicit320x180 reference");check(!skipped.begin(data,random,"missing",400,240,error)&&skipped.active(),"unsupported locale preserves current run");check(!skipped.step(std::numeric_limits<double>::quiet_NaN(),false,false,error),"nonfinite step rejected");check(!skipped.step(-1,false,false,error),"negative step rejected");
 bool old=false,now=false;for(int i=0;i<1000&&!skipped.complete();++i){const auto p=skipped.pose();const bool skip=p.phase==IntroPhase::Playing;if(skip){p.scene==0?old=true:now=true;}check(skipped.step(1./60,true,skip,error),"skip frame");skipped.take_audio();}check(old&&now&&skipped.complete()&&skipped.house_ready()&&skipped.house_unpaused(),"both source ui_select exits close House");check(random.raw_draw_count()==0,"skip before any Now glyph does not consume RNG");
 std::cout<<checks<<" Introduction checks; "<<failed<<" failures\n";return failed?1:0;
}
