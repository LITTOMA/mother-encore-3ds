#include "encore/music_players.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Music finished %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
std::string digest(const MusicPlayers&p){std::ostringstream out;out<<p.ready()<<p.cleanup_waiting()<<p.deferred_finished_count();for(const auto&n:p.nodes())out<<' '<<n.identity<<' '<<n.generation<<' '<<n.stream<<' '<<n.finished_loop<<' '<<n.finished_index<<' '<<n.remove_on_finished<<' '<<n.playing<<' '<<n.stream_paused<<' '<<n.pending_delete<<' '<<n.ensure_empty_on_delete<<' '<<n.gain_db;return out.str();}
void reference(const char*path){
 std::ifstream file(path);CHECK(bool(file));std::string line;std::vector<std::string>sources(4);MusicPlayersPolicy policy;policy.maximum_nodes=16;MusicPlayers players;unsigned cases=0,steps=0;
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
   if(op=="play"){id=node();unsigned current,loop;CHECK(bool(row>>current>>loop)&&current>0&&current<sources.size()&&loop<sources.size());CHECK(players.play_track(id,sources[current],sources[loop],error));}
   else if(op=="add")CHECK(players.add_player(id,error));
   else if(op=="remove"){id=node();CHECK(players.queue_remove(id,error));}
   else if(op=="stop"){id=node();CHECK(players.stop(id,error));}
   else if(op=="emit"){id=node();CHECK(players.signal_finished(id,players.find(id)->generation,error));}
   else if(op=="flush"){CHECK(players.dispatch_finished(error));CHECK(players.flush_deletions(error));}
   else CHECK(false);
   continue;
  }
  CHECK(kind=="state");unsigned count;CHECK(bool(row>>count)&&count==players.nodes().size());
  for(unsigned i=0;i<count;++i){uint64_t id;unsigned source,playing,queued,loop,remove;int index;float gain;CHECK(bool(row>>id>>source>>playing>>queued>>gain>>loop>>index>>remove)&&source<sources.size()&&loop<sources.size());const auto&n=players.nodes()[i];CHECK(n.identity==id&&n.stream==sources[source]&&n.playing==bool(playing)&&n.pending_delete==bool(queued)&&std::abs(n.gain_db-gain)<.00001);CHECK(n.finished_loop==sources[loop]&&n.remove_on_finished==bool(remove));CHECK((n.finished_loop.empty()?-1:int(n.finished_index))==index);}
 }
 CHECK(cases==10&&steps>50);
}
void negative(){
 MusicPlayers p;const char*source="res://fixture/intro",*loop="res://fixture/loop";auto before=digest(p);auto reject=[&](bool result){CHECK(!result&&digest(p)==before);};
 reject(p.play_track(0,source,loop,error));reject(p.signal_finished(0,1,error));reject(p.dispatch_finished(error));
 CHECK(p.initialize({-80,0,1,4},error));before=digest(p);
 for(auto bad:{"","outside","res://../escape","res://bad//path","res://path/"}){reject(p.play_track(0,bad,loop,error));if(*bad)reject(p.play_track(0,source,bad,error));}
 reject(p.play_track(0,std::string_view(nullptr,3),loop,error));reject(p.play_track(0,source,std::string_view(nullptr,3),error));reject(p.play_track(9,source,loop,error));reject(p.signal_finished(0,0,error));reject(p.signal_finished(0,1,error));reject(p.signal_finished(9,1,error));
 CHECK(p.play_track(0,source,{},error));before=digest(p);reject(p.play_track(0,source,{},error));reject(p.signal_finished(0,2,error));
 CHECK(p.play_loop(0,loop,error));CHECK(p.find(0)->remove_on_finished&&p.find(0)->finished_loop==loop);CHECK(p.signal_finished(0,2,error));CHECK(p.find(0)->pending_delete&&!p.find(0)->remove_on_finished&&p.deferred_finished_count()==1);
 CHECK(p.dispatch_finished(error));CHECK(p.find(0)->generation==3&&p.find(0)->playing&&p.find(0)->pending_delete);CHECK(p.flush_deletions(error));CHECK(p.nodes().size()==1&&p.nodes()[0].identity==1&&p.nodes()[0].finished_loop.empty()&&!p.nodes()[0].remove_on_finished);
 CHECK(p.play_track(1,source,loop,error));CHECK(p.signal_finished(1,1,error));auto retained=p;CHECK(p.stop(1,error)&&p.find(1)->finished_loop.empty());CHECK(p.dispatch_finished(error)&&p.find(1)->stream==loop&&p.find(1)->generation==2);CHECK(retained.find(1)->stream==source&&retained.deferred_finished_count()==1);CHECK(retained.dispatch_finished(error)&&retained.find(1)->stream==loop);
 // Signal dispatch is ordered independently of disconnection and retains its
 // own URI, matching the source MessageQueue rather than a mutable node view.
 CHECK(p.play_track(1,source,loop,error));for(unsigned i=0;i<1024;++i)CHECK(p.signal_finished(1,3,error));before=digest(p);reject(p.signal_finished(1,3,error));CHECK(p.stop(1,error)&&p.deferred_finished_count()==1024);CHECK(p.dispatch_finished(error)&&p.deferred_finished_count()==0&&p.find(1)->generation==1027&&p.find(1)->stream==loop);before=digest(p);reject(p.signal_finished(1,3,error));
 // Callers may pass views into the current node; both strings must be retained
 // before replacing either source or loop callback storage.
 CHECK(p.play_track(1,p.find(1)->finished_loop,p.find(1)->stream,error));CHECK(p.find(1)->stream==loop&&p.find(1)->finished_loop==loop);
 CHECK(p.queue_remove(1,error));CHECK(p.flush_deletions(error));before=digest(p);reject(p.signal_finished(1,1028,error));
}
}
int main(int argc,char**argv){CHECK(argc==2);reference(argv[1]);negative();std::printf("Music finished: %u checks; 10 original synthetic signal/callback sequences, one-shot removal, deferred saved child indexes, queued stop/replacement, negative identity/capacity/rollback; no natural end or live DSP claim\n",checks);}
