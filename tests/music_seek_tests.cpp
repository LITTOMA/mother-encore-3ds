#include "encore/music_players.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Music seek %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
std::string digest(const MusicPlayers&p){std::ostringstream out;out<<p.ready()<<p.cleanup_waiting()<<p.deferred_finished_count();for(const auto&n:p.nodes())out<<' '<<n.identity<<' '<<n.generation<<' '<<n.stream<<' '<<n.finished_loop<<' '<<n.finished_index<<' '<<n.remove_on_finished<<' '<<n.playing<<' '<<n.stream_paused<<' '<<n.pending_delete<<' '<<n.ensure_empty_on_delete<<' '<<n.gain_db<<' '<<n.start_seconds;return out.str();}
void reference(const char*path){
 std::ifstream file(path);CHECK(bool(file));std::string line;std::vector<std::string>sources(6);MusicPlayersPolicy policy;policy.maximum_nodes=16;MusicPlayers players;unsigned cases=0,steps=0;
 while(std::getline(file,line)){
  if(line.empty())continue;
  if(line.rfind("# stream ",0)==0){std::istringstream row(line.substr(9));unsigned id;CHECK(bool(row>>id)&&id>0&&id<sources.size());CHECK(row.get()==' ');CHECK(bool(std::getline(row,sources[id])));continue;}
  if(line.rfind("# policy ",0)==0){std::istringstream row(line.substr(9));CHECK(bool(row>>policy.silence_db>>policy.fade_to_seconds>>policy.empty_player_gain_db));continue;}
  if(line[0]=='#')continue;
  std::istringstream row(line);std::string kind;CHECK(bool(row>>kind));
  if(kind=="case"){unsigned index;CHECK(bool(row>>index)&&index==cases++);players=MusicPlayers{};CHECK(players.initialize(policy,error));continue;}
  if(kind=="op"){
   ++steps;std::string op;CHECK(bool(row>>op));uint64_t index,id;
   auto node=[&](){CHECK(bool(row>>index)&&index<players.nodes().size());return players.nodes()[index].identity;};
   if(op=="initial"||op=="settle")continue;
   if(op=="play"){id=node();unsigned current,loop;CHECK(bool(row>>current>>loop)&&current>0&&current<sources.size()&&loop<sources.size());double start;CHECK(bool(row>>start));CHECK(players.play_track(id,sources[current],sources[loop],start,error));}
   else if(op=="add")CHECK(players.add_player(id,error));
   else if(op=="remove"){id=node();CHECK(players.queue_remove(id,error));}
   else if(op=="stop"){id=node();CHECK(players.stop(id,error));}
   else if(op=="emit"){id=node();CHECK(players.signal_finished(id,players.find(id)->generation,error));}
   else if(op=="flush"){CHECK(players.dispatch_finished(error));CHECK(players.flush_deletions(error));}
   else CHECK(false);
   continue;
  }
  CHECK(kind=="state");unsigned count;CHECK(bool(row>>count)&&count==players.nodes().size());
  for(unsigned i=0;i<count;++i){uint64_t id;unsigned source,playing,queued,loop,remove;int index;float gain;double position;CHECK(bool(row>>id>>source>>playing>>queued>>gain>>loop>>index>>remove>>position)&&source<sources.size()&&loop<sources.size());const auto&n=players.nodes()[i];CHECK(n.identity==id&&n.stream==sources[source]&&n.playing==bool(playing)&&n.pending_delete==bool(queued)&&std::abs(n.gain_db-gain)<.00001);CHECK(n.finished_loop==sources[loop]&&n.remove_on_finished==bool(remove));CHECK(std::abs(n.start_seconds-position)<1e-15);CHECK((n.finished_loop.empty()?-1:int(n.finished_index))==index);}
 }
 CHECK(cases==8&&steps>20);
}
void negative(){
 MusicPlayers p;auto before=digest(p);auto reject=[&](bool result){CHECK(!result&&digest(p)==before);};
 reject(p.play_track(0,"res://fixture.mp3","res://fixture.mp3",1.5,error));CHECK(p.initialize({-80,0,1,4},error));before=digest(p);
 for(double value:{-1.,double(NAN),double(INFINITY),1e100})reject(p.play_track(0,"res://fixture.mp3","res://fixture.mp3",value,error));
 reject(p.play_track(99,"res://fixture.mp3","res://fixture.mp3",1.5,error));reject(p.play_track(0,"res://../escape.mp3","res://fixture.mp3",1.5,error));
 CHECK(p.play_track(0,"res://fixture.mp3","res://loop.mp3",.123456789,error)&&p.find(0)->start_seconds==double(float(.123456789)));auto retained=p;
 CHECK(p.signal_finished(0,1,error)&&p.dispatch_finished(error)&&p.find(0)->generation==2&&p.find(0)->start_seconds==0);CHECK(retained.find(0)->generation==1&&retained.find(0)->start_seconds==double(float(.123456789)));
 CHECK(p.play_track(0,"res://fixture.mp3","res://fixture.mp3",2000,error)&&p.find(0)->start_seconds==2000);CHECK(p.stop(0,error)&&p.find(0)->start_seconds==2000);
}

}
int main(int argc,char**argv){CHECK(argc==2);reference(argv[1]);negative();std::printf("Music seek: %u checks; 8 original paused pending-start/callback sequences, float32 forwarding, deferred zero starts, invalid start/URI rollback; no natural audio-clock or hardware claim\n",checks);}
