#include "encore/music_players.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Music all %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
std::string digest(const MusicPlayers&p){std::ostringstream out;out<<p.ready()<<p.cleanup_waiting();for(const auto&n:p.nodes())out<<' '<<n.identity<<' '<<n.generation<<' '<<n.stream<<' '<<n.stream_paused<<' '<<n.playing<<' '<<n.pending_delete<<' '<<n.gain_db<<' '<<n.tween.active<<' '<<n.tween.elapsed;return out.str();}
void reference(const char*path){
 std::ifstream file(path);CHECK(bool(file));std::string line;std::vector<std::string>sources(3);MusicPlayersPolicy policy;policy.maximum_nodes=16;MusicPlayers players;unsigned cases=0,steps=0;
 while(std::getline(file,line)){
  if(line.empty())continue;
  if(line.rfind("# stream ",0)==0){std::istringstream row(line.substr(9));unsigned id;CHECK(bool(row>>id)&&id>0&&id<sources.size());CHECK(row.get()==' ');CHECK(bool(std::getline(row,sources[id])));continue;}
  if(line.rfind("# policy ",0)==0){std::istringstream row(line.substr(9));CHECK(bool(row>>policy.silence_db>>policy.fade_to_seconds>>policy.empty_player_gain_db));continue;}
  if(line[0]=='#')continue;
  std::istringstream row(line);std::string kind;CHECK(bool(row>>kind));
  if(kind=="case"){unsigned index;CHECK(bool(row>>index)&&index==cases++);players=MusicPlayers{};CHECK(players.initialize(policy,error));continue;}
  if(kind=="op"){
   ++steps;std::string op;CHECK(bool(row>>op));uint64_t index,id;double seconds;float gain;
   auto node=[&](){CHECK(bool(row>>index)&&index<players.nodes().size());return players.nodes()[index].identity;};
   if(op=="initial"||op=="settle")continue;
   if(op=="fadeall"){CHECK(bool(row>>seconds));CHECK(players.fade_all(seconds,error));}
   else if(op=="play"){id=node();unsigned track;CHECK(bool(row>>track)&&track>0&&track<sources.size());CHECK(players.play_loop(id,sources[track],error));}
   else if(op=="add")CHECK(players.add_player(id,error));
   else if(op=="remove"){id=node();CHECK(players.queue_remove(id,error));}
   else if(op=="stop"){id=node();CHECK(players.stop(id,error));}
   else if(op=="volume"){id=node();CHECK(bool(row>>gain));CHECK(players.set_volume(id,gain,error));}
   else if(op=="pause")CHECK(players.pause_all(error));
   else if(op=="resume")CHECK(players.resume_all(error));
   else if(op=="flush")CHECK(players.flush_deletions(error));
   else if(op=="fadeout"){id=node();CHECK(bool(row>>seconds));CHECK(players.fade_out(id,seconds,error));}
   else if(op=="fadein"||op=="fadeto"){id=node();CHECK(bool(row>>gain>>seconds));CHECK(op=="fadein"?players.fade_in(id,gain,seconds,error):players.fade_to(id,gain,seconds,error));}
   else if(op=="half"||op=="finish"){
    double remaining=0;for(const auto&n:players.nodes())if(n.tween.active)remaining=std::max(remaining,n.tween.duration-n.tween.elapsed);
    if(op=="half")remaining*=.5;
    while(remaining>0){const auto delta=std::min(1.,remaining);CHECK(players.advance(delta,false,error));remaining-=delta;}
   }else CHECK(false);
   continue;
  }
  CHECK(kind=="state");unsigned count;CHECK(bool(row>>count)&&count==players.nodes().size());
  for(unsigned i=0;i<count;++i){uint64_t id;unsigned source,playing,paused,queued;float gain;CHECK(bool(row>>id>>source>>playing>>gain>>paused>>queued)&&source<sources.size());const auto&n=players.nodes()[i];const bool same=n.identity==id&&n.stream==sources[source]&&n.playing==bool(playing)&&n.stream_paused==bool(paused)&&n.pending_delete==bool(queued)&&std::abs(n.gain_db-gain)<.00001;if(!same)std::fprintf(stderr,"case=%u node=%llu expected=%llu gain=%g/%g paused=%u/%u queued=%u/%u playing=%u/%u\n",cases-1,(unsigned long long)n.identity,(unsigned long long)id,n.gain_db,gain,n.stream_paused,paused,n.pending_delete,queued,n.playing,playing);CHECK(same);}
 }
 CHECK(cases==8&&steps>50);
}
void negative(){
 MusicPlayers p;auto before=digest(p);CHECK(!p.fade_all(.2,error)&&digest(p)==before);CHECK(p.initialize({-80,0,1,8},error));before=digest(p);
 for(double value:{-1.,61.,double(NAN),double(INFINITY)})CHECK(!p.fade_all(value,error)&&digest(p)==before);
 CHECK(p.fade_all(.2,error));CHECK(p.advance(1,false,error)&&digest(p)==before);
 CHECK(p.play_loop(0,"res://fixture.mp3",error)&&p.fade_out(0,.2,error)&&p.stop(0,error)&&p.fade_all(.2,error));before=digest(p);CHECK(p.advance(1,false,error)&&digest(p)==before&&!p.find(0)->pending_delete);
 uint64_t other;CHECK(p.add_player(other,error)&&p.play_loop(other,"res://other.mp3",error)&&p.fade_in(other,6,.2,error));CHECK(p.advance(.2,false,error)&&p.find(0)->pending_delete&&!p.find(other)->pending_delete);CHECK(p.flush_deletions(error)&&!p.find(0)&&p.find(other));
}

}
int main(int argc,char**argv){CHECK(argc==2);reference(argv[1]);negative();std::printf("Music all: %u checks; 8 original fadeout_all/remove_all/completion sequences, stopped/paused tracks, retained waiters and rollback; no live frontend/audio claim\n",checks);}
