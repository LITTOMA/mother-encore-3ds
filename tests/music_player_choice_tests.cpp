#include "encore/music_player_choice.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Music player choice %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
bool near(float a,float b){return std::abs(a-b)<.00001f;}
bool unchanged(const MusicLoopChoice&a,const MusicLoopChoice&b){return a.player==b.player&&a.append_player==b.append_player&&a.restart_stream==b.restart_stream&&a.same_song==b.same_song&&a.volume==b.volume&&a.initial_gain_db==b.initial_gain_db&&a.target_gain_db==b.target_gain_db&&a.seconds==b.seconds&&a.curve==b.curve;}
}
int main(int argc,char**argv){
 CHECK(argc==2);std::ifstream file(argv[1]);CHECK(bool(file));std::string line;std::vector<std::string>sources(3);
 MusicLoopRequest request;double prior_fadeout=0;unsigned rows=0;bool policy=false;
 while(std::getline(file,line)){
  if(line.empty())continue;
  if(line.rfind("# stream ",0)==0){std::istringstream row(line.substr(9));unsigned id;CHECK(bool(row>>id)&&id>0&&id<sources.size());CHECK(row.get()==' ');CHECK(bool(std::getline(row,sources[id])));continue;}
  if(line.rfind("# policy ",0)==0){std::istringstream row(line.substr(9));CHECK(bool(row>>request.silence_db>>request.fade_to_seconds>>request.gain_db>>request.fadein_seconds>>prior_fadeout));policy=true;continue;}
  if(line[0]=='#')continue;
  CHECK(policy&&!sources[1].empty()&&!sources[2].empty());std::istringstream row(line);unsigned index,first,front,count;
  CHECK(bool(row>>index>>first>>front>>count));CHECK(index==rows++&&first<2&&front>0&&front<sources.size()&&count>0&&count<=16);
  std::vector<MusicPlayerSnapshot>before,after;
  auto read=[&](std::vector<MusicPlayerSnapshot>&target,unsigned n){for(unsigned i=0;i<n;++i){uint64_t id;unsigned source,playing;float gain;CHECK(bool(row>>id>>source>>playing>>gain));CHECK(source<sources.size()&&playing<2);target.push_back({id,sources[source],bool(playing),gain});}};
  read(before,count);CHECK(bool(row>>count));CHECK(count>0&&count<=16);read(after,count);
  uint64_t attached;double before_runtime,after_runtime;unsigned has_prior;float half_gain;
  CHECK(bool(row>>attached>>before_runtime>>after_runtime>>has_prior>>half_gain));std::string extra;CHECK(!(row>>extra));CHECK(has_prior<2);
  request.stream=sources[2];request.first_registered_stream=sources[front];request.requester_is_first=first!=0;MusicLoopChoice choice;
  CHECK(choose_music_loop(before,request,choice,error));CHECK(choice.target_gain_db==request.gain_db);
  const MusicPlayerSnapshot*selected=nullptr,*original=nullptr;
  for(const auto&p:after)if(p.identity==attached)selected=&p;
  for(const auto&p:before)if(p.identity==attached)original=&p;
  CHECK(selected);
  if(choice.append_player){
   CHECK(after.size()==before.size()+1&&choice.player==MusicLoopChoice::no_player&&!original);
   CHECK(choice.restart_stream&&choice.volume==MusicLoopVolume::FadeIn&&choice.curve==MusicRegionCurve::QuartOut&&choice.seconds==request.fadein_seconds);
  }else CHECK(after.size()==before.size()&&choice.player==attached&&original);
  CHECK(choice.initial_gain_db==selected->gain_db);
  if(choice.restart_stream)CHECK(selected->playing&&selected->stream==request.stream);
  else CHECK(original&&original->playing&&selected->playing&&selected->stream==original->stream);
  for(const auto&p:before)if(p.identity!=attached){const MusicPlayerSnapshot*found=nullptr;for(const auto&q:after)if(q.identity==p.identity)found=&q;CHECK(found&&found->stream==p.stream&&found->playing==p.playing&&found->gain_db==p.gain_db);}
  if(choice.volume==MusicLoopVolume::FadeIn||choice.volume==MusicLoopVolume::FadeTo)CHECK(choice.seconds==after_runtime);
  if(choice.volume==MusicLoopVolume::Set)CHECK(choice.restart_stream&&choice.initial_gain_db==request.gain_db&&before_runtime==after_runtime);
  if(choice.volume==MusicLoopVolume::None)CHECK(!choice.restart_stream&&before_runtime==after_runtime);
  if(has_prior){
   float sampled=123;
   if(choice.volume==MusicLoopVolume::None){
    CHECK(before_runtime==prior_fadeout&&choice.initial_gain_db==request.gain_db);
    CHECK(sample_music_tween(request.gain_db,request.silence_db,before_runtime,after_runtime/2,MusicRegionCurve::QuartIn,sampled,error));
   }else{
    CHECK(choice.volume==MusicLoopVolume::FadeTo&&choice.curve==MusicRegionCurve::Linear&&original);
    CHECK(sample_music_tween(original->gain_db,choice.target_gain_db,choice.seconds,after_runtime/2,choice.curve,sampled,error));
   }
   CHECK(near(sampled,half_gain));
  }
 }
 CHECK(rows==11);
 request={sources[2],sources[2],true,6,-80,1,1};std::vector<MusicPlayerSnapshot>good{{7,sources[1],true,0}};
 MusicLoopChoice result;CHECK(choose_music_loop(good,request,result,error));const auto preserved=result;
 auto reject=[&](const std::vector<MusicPlayerSnapshot>&players,MusicLoopRequest r){CHECK(!choose_music_loop(players,r,result,error));CHECK(unchanged(preserved,result));};
 reject({},request);auto bad=good;bad.push_back(bad.front());reject(bad,request);bad=good;bad[0].identity=MusicLoopChoice::no_player;reject(bad,request);bad=good;bad[0].stream={};reject(bad,request);
 for(float value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-121.f,25.f}){bad=good;bad[0].gain_db=value;reject(bad,request);auto r=request;r.gain_db=value;reject(good,r);}
 for(auto path:{"","res://../escape","res://bad//path","res://bad/./path","res://bad\\path","outside/path","res://bad/path/"}){
  auto r=request;r.stream=path;reject(good,r);r=request;r.first_registered_stream=path;reject(good,r);
 }
 auto null=request;null.stream=std::string_view(nullptr,4);reject(good,null);std::string long_path(1025,'a');null=request;null.stream=long_path;reject(good,null);
 for(double value:{-1.0,61.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){auto r=request;r.fadein_seconds=value;reject(good,r);r=request;r.fade_to_seconds=value;reject(good,r);}
 auto r=request;r.silence_db=-19;reject(good,r);
 std::vector<MusicPlayerSnapshot>full;for(unsigned i=0;i<256;++i)full.push_back({i,sources[1],true,0});reject(full,request);full.push_back({256,sources[1],true,0});reject(full,request);
 float sample=19;CHECK(!sample_music_tween(0,-80,1,.5,MusicRegionCurve(255),sample,error)&&sample==19);
 CHECK(!sample_music_tween(0,-80,-1,.5,MusicRegionCurve::Linear,sample,error)&&sample==19);CHECK(!sample_music_tween(0,-80,1,-1,MusicRegionCurve::Linear,sample,error)&&sample==19);
 CHECK(!sample_music_tween(25,-80,1,.5,MusicRegionCurve::Linear,sample,error)&&sample==19);CHECK(!sample_music_tween(0,-121,1,.5,MusicRegionCurve::Linear,sample,error)&&sample==19);
 CHECK(sample_music_tween(0,-80,0,0,MusicRegionCurve::QuartIn,sample,error)&&sample==-80);
 CHECK(sample_music_tween(0,-80,1,2,MusicRegionCurve::QuartOut,sample,error)&&sample==-80);
 std::printf("Music player choices: %u checks; 11 original node-selection/property outcomes, retained silent/stopped nodes and tweens, bad-input rollback; no global bank/NDSP integration claim\n",checks);
}
