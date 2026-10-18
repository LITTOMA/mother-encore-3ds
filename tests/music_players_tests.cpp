#include "encore/music_players.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Music players %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
bool near(float a,float b){return std::abs(a-b)<.0001f;}
std::string state(const MusicPlayers&p){
 std::ostringstream out;out<<p.ready()<<p.cleanup_waiting()<<std::hexfloat;
 for(const auto&n:p.nodes())out<<' '<<n.identity<<' '<<n.generation<<' '<<n.stream<<' '<<n.gain_db<<' '<<n.playing<<' '<<n.pending_delete<<' '<<n.ensure_empty_on_delete<<' '<<n.tween.from<<' '<<n.tween.to<<' '<<n.tween.duration<<' '<<n.tween.elapsed<<' '<<unsigned(n.tween.curve)<<' '<<n.tween.active;
 return out.str();
}
void streams(const std::string&line,std::vector<std::string>&sources){
 std::istringstream row(line.substr(9));unsigned id;CHECK(bool(row>>id)&&id>0&&id<sources.size());CHECK(row.get()==' ');CHECK(bool(std::getline(row,sources[id])));
}
void match(const MusicPlayers&p,const std::vector<MusicPlayerSnapshot>&expected){
 CHECK(p.nodes().size()==expected.size());for(size_t i=0;i<expected.size();++i){const auto&a=p.nodes()[i];const auto&b=expected[i];CHECK(a.identity==b.identity&&a.stream==b.stream&&a.playing==b.playing&&near(a.gain_db,b.gain_db));}
}
void choices(const char*path){
 std::ifstream file(path);CHECK(bool(file));std::string line;std::vector<std::string>sources(3);MusicLoopRequest request;double prior=0;unsigned rows=0;
 while(std::getline(file,line)){
  if(line.empty())continue;
  if(line.rfind("# stream ",0)==0){streams(line,sources);continue;}
  if(line.rfind("# policy ",0)==0){std::istringstream row(line.substr(9));CHECK(bool(row>>request.silence_db>>request.fade_to_seconds>>request.gain_db>>request.fadein_seconds>>prior));continue;}
  if(line[0]=='#')continue;
  std::istringstream row(line);unsigned index,first,front,count;CHECK(bool(row>>index>>first>>front>>count));CHECK(index==rows++);
  std::vector<MusicPlayerSnapshot>before,after;
  auto read=[&](std::vector<MusicPlayerSnapshot>&target,unsigned n){for(unsigned i=0;i<n;++i){uint64_t id;unsigned source,playing;float value;CHECK(bool(row>>id>>source>>playing>>value)&&source<sources.size());target.push_back({id,sources[source],bool(playing),value});}};
  read(before,count);CHECK(bool(row>>count));read(after,count);uint64_t attached;double old_runtime,runtime;unsigned has_prior;float half;
  CHECK(bool(row>>attached>>old_runtime>>runtime>>has_prior>>half));
  MusicPlayers players;CHECK(players.initialize({request.silence_db,0,request.fade_to_seconds,16},error));
  for(size_t i=0;i<before.size();++i){const auto&p=before[i];uint64_t id=0;if(i)CHECK(players.add_player(id,error));CHECK(id==p.identity);if(!p.stream.empty())CHECK(players.play_loop(id,p.stream,error));CHECK(players.set_volume(id,p.gain_db,error));if(!p.playing)CHECK(players.stop(id,error));}
  if(has_prior){CHECK(players.set_volume(0,request.gain_db,error));CHECK(players.fade_out(0,prior,error));CHECK(players.set_volume(0,before[0].gain_db,error));}
  request.stream=sources[2];request.first_registered_stream=sources[front];request.requester_is_first=first!=0;uint64_t selected=MusicLoopChoice::no_player;
  CHECK(players.choose_loop(request,selected,error)&&selected==attached);match(players,after);
  if(has_prior){CHECK(players.advance(runtime/2,false,error));CHECK(near(players.find(selected)->gain_db,half));CHECK(players.cleanup_waiting());}
  auto owned=players;players=MusicPlayers{};CHECK(!owned.nodes().empty());CHECK(owned.find(selected)->stream==after.back().stream||owned.find(selected)->stream==after.front().stream);
 }
 CHECK(rows==11);
}
void lifetime(const char*path){
 std::ifstream file(path);CHECK(bool(file));std::vector<std::string>sources(3);std::string line;MusicPlayersPolicy policy;policy.maximum_nodes=16;MusicPlayers players;unsigned cases=0,steps=0;double elapsed=0;bool first_completion=false;
 while(std::getline(file,line)){
  if(line.empty())continue;
  if(line.rfind("# stream ",0)==0){streams(line,sources);continue;}
  if(line.rfind("# policy ",0)==0){std::istringstream row(line.substr(9));CHECK(bool(row>>policy.silence_db>>policy.fade_to_seconds>>policy.empty_player_gain_db));continue;}
  if(line[0]=='#')continue;
  std::istringstream row(line);std::string kind;CHECK(bool(row>>kind));
  if(kind=="case"){
   unsigned index;CHECK(bool(row>>index)&&index==cases++);players=MusicPlayers{};CHECK(players.initialize(policy,error));elapsed=0;continue;
  }
  if(kind=="op"){
   ++steps;std::string op;CHECK(bool(row>>op));uint64_t index,id;float value;double seconds;
   auto node_id=[&](){CHECK(bool(row>>index)&&index<players.nodes().size());return players.nodes()[index].identity;};
   if(op=="initial"||op=="settle")continue;
   if(op=="play"){id=node_id();unsigned source;CHECK(bool(row>>source)&&source>0&&source<sources.size());CHECK(players.play_loop(id,sources[source],error));}
   else if(op=="add")CHECK(players.add_player(id,error));
   else if(op=="volume"){id=node_id();CHECK(bool(row>>value));CHECK(players.set_volume(id,value,error));}
   else if(op=="stop"){id=node_id();CHECK(players.stop(id,error));}
   else if(op=="fadeout"){id=node_id();CHECK(bool(row>>seconds));CHECK(players.fade_out(id,seconds,error));}
   else if(op=="fadeto"){id=node_id();CHECK(bool(row>>value>>seconds));CHECK(players.fade_to(id,value,seconds,error));}
   else if(op=="remove"){id=node_id();CHECK(players.queue_remove(id,error));}
   else if(op=="clear")CHECK(players.queue_clear(error));
   else if(op=="flush")CHECK(players.flush_deletions(error));
   else if(op=="first_complete"){first_completion=true;}
   else if(op=="finish"){
    double delta=op=="finish"?0:61;
    for(const auto&node:players.nodes())if(node.tween.active){const double remaining=node.tween.duration-node.tween.elapsed;delta=op=="finish"?std::max(delta,remaining):std::min(delta,remaining);}
    CHECK(delta>=0&&delta<=1);CHECK(players.advance(delta,false,error));elapsed+=delta;
   }else CHECK(false);
   continue;
  }
  CHECK(kind=="state");unsigned count,waiting;double native_elapsed;CHECK(bool(row>>count>>waiting>>native_elapsed));
  if(first_completion){CHECK(native_elapsed>=elapsed&&native_elapsed-elapsed<=1);CHECK(players.advance(native_elapsed-elapsed,false,error));elapsed=native_elapsed;first_completion=false;}
  CHECK(players.nodes().size()==count);CHECK(players.cleanup_waiting()==bool(waiting));
  for(unsigned i=0;i<count;++i){uint64_t id;unsigned source,playing,queued;float value;CHECK(bool(row>>id>>source>>playing>>value>>queued)&&source<sources.size());const auto&node=players.nodes()[i];CHECK(node.identity==id&&node.stream==sources[source]&&node.playing==bool(playing)&&node.pending_delete==bool(queued)&&near(node.gain_db,value));}
  if(native_elapsed>0)CHECK(std::abs(elapsed-native_elapsed)<.00001);
 }
 CHECK(cases==12&&steps>60);
}
void negative_and_ownership(){
 const float nan=std::numeric_limits<float>::quiet_NaN();const double inf=std::numeric_limits<double>::infinity();
 MusicPlayers empty;uint64_t id=27;auto reject_empty=[&](bool result){CHECK(!result&&!empty.ready()&&empty.nodes().empty()&&id==27);};
 reject_empty(empty.add_player(id,error));reject_empty(empty.play_loop(0,"res://song",error));reject_empty(empty.stop(0,error));reject_empty(empty.set_volume(0,0,error));reject_empty(empty.fade_in(0,0,1,error));reject_empty(empty.fade_to(0,0,1,error));reject_empty(empty.fade_out(0,1,error));reject_empty(empty.queue_remove(0,error));reject_empty(empty.queue_clear(error));reject_empty(empty.flush_deletions(error));reject_empty(empty.advance(0,false,error));
 for(auto p:{MusicPlayersPolicy{},MusicPlayersPolicy{-19,0,1,16},MusicPlayersPolicy{-121,0,1,16},MusicPlayersPolicy{-80,25,1,16},MusicPlayersPolicy{-80,0,-1,16},MusicPlayersPolicy{-80,0,61,16},MusicPlayersPolicy{-80,0,1,257},MusicPlayersPolicy{nan,0,1,16},MusicPlayersPolicy{-80,nan,1,16},MusicPlayersPolicy{-80,0,inf,16}})reject_empty(empty.initialize(p,error));
 MusicPlayers p;CHECK(p.initialize({-80,0,1,2},error));const auto start=state(p);
 auto reject=[&](bool result,const std::string&expected){CHECK(!result&&state(p)==expected);};
 reject(p.initialize({-80,0,1,2},error),start);
 for(auto path:{"","outside/song","res://../song","res://bad//song","res://song/","res://bad\\song"})reject(p.play_loop(0,path,error),start);
 reject(p.play_loop(0,std::string_view(nullptr,3),error),start);reject(p.play_loop(9,"res://song",error),start);reject(p.stop(9,error),start);reject(p.queue_remove(9,error),start);
 for(float value:{nan,-121.f,25.f}){reject(p.set_volume(0,value,error),start);reject(p.fade_in(0,value,1,error),start);reject(p.fade_to(0,value,1,error),start);}
 for(double value:{-1.,61.,inf}){reject(p.fade_in(0,0,value,error),start);reject(p.fade_to(0,0,value,error),start);reject(p.fade_out(0,value,error),start);reject(p.advance(value,false,error),start);}
 reject(p.advance(1.001,false,error),start);reject(p.fade_in(9,0,1,error),start);
 std::string source="res://owned/song";CHECK(p.play_loop(0,source,error));source.assign(500,'x');CHECK(p.find(0)->stream=="res://owned/song"&&p.find(0)->generation==1);
 CHECK(p.play_loop(0,p.find(0)->stream,error));CHECK(p.find(0)->generation==2);CHECK(p.add_player(id,error)&&id==1);auto full=state(p);uint64_t rejected=72;reject(p.add_player(rejected,error),full);CHECK(rejected==72);
 CHECK(p.play_loop(1,"res://other/song",error));auto request=MusicLoopRequest{"res://third/song","res://third/song",true,0,-80,1,1};full=state(p);rejected=72;reject(p.choose_loop(request,rejected,error),full);CHECK(rejected==72);
 request.fade_to_seconds=2;reject(p.choose_loop(request,rejected,error),full);request.fade_to_seconds=1;request.silence_db=-79;reject(p.choose_loop(request,rejected,error),full);
 CHECK(p.fade_out(0,.1,error));CHECK(p.advance(.1,true,error));CHECK(p.cleanup_waiting()&&!p.find(0)->pending_delete);CHECK(p.advance(0,false,error));CHECK(!p.cleanup_waiting()&&p.find(0)->pending_delete);
 CHECK(p.flush_deletions(error));CHECK(p.nodes().size()==1&&p.nodes()[0].identity==1);CHECK(p.queue_clear(error));CHECK(p.flush_deletions(error));CHECK(p.nodes().empty());CHECK(p.add_player(id,error)&&id==2);
 CHECK(p.fade_in(id,6,0,error));CHECK(p.advance(0,false,error));CHECK(p.find(id)->gain_db==6&&!p.find(id)->tween.active);
 CHECK(p.stop(id,error));CHECK(p.fade_out(id,.5,error));CHECK(!p.cleanup_waiting());
}
}
int main(int argc,char**argv){CHECK(argc==3);choices(argv[1]);lifetime(argv[2]);negative_and_ownership();std::printf("Music players: %u checks; 11 original choices, 12 original lifecycle sequences, owned streams, deferred deletion, shared Tween barrier and negative rollback; no live bank/DSP integration claim\n",checks);}
