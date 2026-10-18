#include "encore/music_scene_graph.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Music scene graph %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
bool near(float a,float b){return std::abs(a-b)<.0001f;}
std::string digest(const MusicSceneGraph&g){
 std::ostringstream out;out<<g.scene_epoch()<<g.active()<<g.players().ready()<<g.players().cleanup_waiting()<<g.players().deferred_finished_count()<<std::hexfloat;
 for(const auto&n:g.players().nodes())out<<' '<<n.identity<<' '<<n.generation<<' '<<n.stream<<' '<<n.finished_loop<<' '<<n.finished_index<<' '<<n.remove_on_finished<<' '<<n.gain_db<<' '<<n.playing<<' '<<n.pending_delete<<' '<<n.ensure_empty_on_delete<<' '<<n.tween.from<<' '<<n.tween.to<<' '<<n.tween.duration<<' '<<n.tween.elapsed<<' '<<n.tween.active;
 for(const auto&a:g.areas())out<<' '<<a.player<<a.inside<<a.registered<<a.pending_exit<<a.removed;
 for(auto id:g.registered())out<<' '<<id;
 return out.str();
}
void reference(const MusicRegionData&podunk,const MusicRegionData&mayor,const char*path){
 std::ifstream file(path);CHECK(bool(file));std::string line;std::vector<std::string>sources(32);MusicPlayersPolicy policy;policy.maximum_nodes=16;MusicSceneGraph graph;uint64_t epoch=0;unsigned rows=0,pending_scene=0,steps=0;MusicRegionContext context;std::map<std::string,bool>flags;
 context.flag=[&](std::string_view name){return flags[std::string(name)];};
 while(std::getline(file,line)){
  if(line.empty())continue;
  if(line.rfind("# stream ",0)==0){std::istringstream row(line.substr(9));unsigned id;CHECK(bool(row>>id)&&id>0&&id<sources.size());CHECK(row.get()==' ');CHECK(bool(std::getline(row,sources[id])));continue;}
  if(line.rfind("# policy ",0)==0){std::istringstream row(line.substr(9));CHECK(bool(row>>policy.silence_db>>policy.fade_to_seconds>>policy.empty_player_gain_db));continue;}
  if(line[0]=='#')continue;
  std::istringstream row(line);std::string kind;CHECK(bool(row>>kind));
  if(kind=="case"){
   unsigned index;CHECK(bool(row>>index)&&index==rows++);graph=MusicSceneGraph{};CHECK(graph.initialize(policy,error));epoch=0;pending_scene=0;flags.clear();context.is_player=context.has_collisions=true;context.in_cutscene=context.in_battle=false;continue;
  }
  if(kind=="op"){
   ++steps;std::string op,name;CHECK(bool(row>>op));
   if(op=="initial"||op=="settle")continue;
   if(op=="scene"){CHECK(bool(row>>pending_scene)&&pending_scene>0&&pending_scene<=2);}
   else if(op=="enter"){CHECK(bool(row>>name));CHECK(graph.enter(epoch,name,context,error));}
   else if(op=="exit"){CHECK(bool(row>>name));CHECK(graph.exit(epoch,name,context,error));}
   else if(op=="idle")CHECK(graph.idle_frame(epoch,error));
   else if(op=="stop"){double seconds;CHECK(bool(row>>name>>seconds));CHECK(graph.stop_area(epoch,name,seconds,error));}
   else if(op=="door"){double seconds;CHECK(bool(row>>seconds));CHECK(graph.stop_on_door(epoch,seconds,error));}
   else if(op=="context"){unsigned player,cutscene,battle,collisions;CHECK(bool(row>>player>>cutscene>>battle>>collisions));context.is_player=player!=0;context.in_cutscene=cutscene!=0;context.in_battle=battle!=0;context.has_collisions=collisions!=0;}
   else if(op=="flag"){unsigned value;CHECK(bool(row>>name>>value));flags[name]=value!=0;}
   else if(op=="external"){
    unsigned source,playing;float gain;uint64_t id;CHECK(bool(row>>source>>playing>>gain)&&source<sources.size());CHECK(graph.players().add_player(id,error));CHECK(graph.players().play_loop(id,sources[source],error));CHECK(graph.players().set_volume(id,gain,error));if(!playing)CHECK(graph.players().stop(id,error));
   }else if(op=="finish"){
    double remaining=0;for(const auto&n:graph.players().nodes())if(n.tween.active)remaining=std::max(remaining,n.tween.duration-n.tween.elapsed);
    CHECK(remaining>0&&remaining<=60);while(remaining>0){const double delta=std::min(1.,remaining);CHECK(graph.advance(delta,false,error));remaining-=delta;}
   }else if(op=="flush")CHECK(graph.flush_deletions(error));
   else CHECK(false);
   continue;
  }
  unsigned count;CHECK(bool(row>>count));
  if(kind=="exits"){
   for(unsigned i=0;i<count;++i){std::string name;CHECK(bool(row>>name));CHECK(graph.tree_exit(epoch,name,error));}
   if(pending_scene){if(epoch)CHECK(graph.finish_scene(epoch,error));CHECK(graph.attach_scene(++epoch,pending_scene==1?podunk:mayor,error));pending_scene=0;}
  }else if(kind=="voices"){
   CHECK(count==graph.players().nodes().size());for(unsigned i=0;i<count;++i){uint64_t id;unsigned source,playing,queued;float gain;CHECK(bool(row>>id>>source>>playing>>gain>>queued)&&source<sources.size());const auto&n=graph.players().nodes()[i];CHECK(n.identity==id&&n.stream==sources[source]&&n.playing==bool(playing)&&n.pending_delete==bool(queued)&&near(n.gain_db,gain));}
  }else if(kind=="registered"){
   CHECK(count==graph.registered().size());for(unsigned i=0;i<count;++i){std::string name;CHECK(bool(row>>name));CHECK(graph.data().regions()[graph.registered()[i]].source_path==name);}
  }else if(kind=="areas"){
   CHECK(count==graph.areas().size());for(unsigned i=0;i<count;++i){std::string name;unsigned inside,registered;int64_t attached;CHECK(bool(row>>name>>inside>>registered>>attached));const auto&a=graph.areas()[i];CHECK(graph.data().regions()[i].source_path==name&&a.inside==bool(inside)&&a.registered==bool(registered));const int64_t actual=graph.players().find(a.player)?int64_t(a.player):-1;CHECK(actual==attached);}
  }else CHECK(false);
 }
 CHECK(rows==10&&steps>60);
}
void negative(const MusicRegionData&data){
 MusicSceneGraph g;MusicRegionContext context;context.flag=[](std::string_view){return false;};MusicRegionData invalid;const auto path=data.regions()[0].source_path;
 auto before=digest(g);auto reject=[&](bool result){CHECK(!result&&digest(g)==before);};
 reject(g.attach_scene(1,data,error));reject(g.enter(1,path,context,error));reject(g.exit(1,path,context,error));reject(g.idle_frame(1,error));reject(g.tree_exit(1,path,error));reject(g.stop_area(1,path,1,error));reject(g.stop_on_door(1,1,error));reject(g.finish_scene(1,error));reject(g.advance(0,false,error));reject(g.flush_deletions(error));
 CHECK(g.initialize({data.silence_db(),0,data.fade_to_seconds(),2},error));before=digest(g);reject(g.initialize({data.silence_db(),0,data.fade_to_seconds(),2},error));reject(g.attach_scene(0,data,error));reject(g.attach_scene(1,invalid,error));
 {auto retained=data;CHECK(g.attach_scene(1,retained,error));}before=digest(g);reject(g.attach_scene(2,data,error));reject(g.enter(1,"Unbound",context,error));reject(g.exit(1,"Unbound",context,error));reject(g.tree_exit(1,"Unbound",error));reject(g.stop_area(1,path,-1,error));reject(g.stop_on_door(1,NAN,error));reject(g.finish_scene(1,error));
 CHECK(g.enter(1,path,context,error));CHECK(g.data().regions()[0].source_path==path);before=digest(g);
 CHECK(g.enter(0,"Unbound",context,error)&&digest(g)==before);CHECK(g.exit(9,"Unbound",context,error)&&digest(g)==before);CHECK(g.tree_exit(9,"Unbound",error)&&digest(g)==before);CHECK(g.stop_area(9,"Unbound",NAN,error)&&digest(g)==before);CHECK(g.stop_on_door(9,NAN,error)&&digest(g)==before);CHECK(g.idle_frame(9,error)&&digest(g)==before);CHECK(g.finish_scene(9,error)&&digest(g)==before);
 for(const auto&r:data.regions())if(!r.appear_flag.empty()||!r.disappear_flag.empty()){auto absent=context;absent.flag={};reject(g.enter(1,r.source_path,absent,error));break;}
 // A fully occupied global player list must preserve scene registrations and
 // all previous nodes when the source decision needs another player.
 uint64_t id;CHECK(g.players().add_player(id,error));CHECK(g.players().play_loop(id,"res://fixture/other",error));before=digest(g);for(const auto&r:data.regions())if(r.track_id!=data.regions()[0].track_id&&r.appear_flag.empty()&&r.disappear_flag.empty()){reject(g.enter(1,r.source_path,context,error));break;}
 auto copy=g;CHECK(copy.players().nodes()[0].stream==g.players().nodes()[0].stream);CHECK(copy.stop_area(1,path,0,error));CHECK(digest(g)==before);
 for(const auto&r:data.regions())CHECK(g.tree_exit(1,r.source_path,error));CHECK(g.finish_scene(1,error));before=digest(g);reject(g.enter(1,path,context,error));reject(g.exit(1,path,context,error));reject(g.idle_frame(1,error));reject(g.tree_exit(1,path,error));reject(g.finish_scene(1,error));reject(g.stop_on_door(1,0,error));reject(g.attach_scene(1,data,error));
 CHECK(g.attach_scene(2,data,error));CHECK(g.enter(1,"Unbound",context,error));CHECK(g.players().nodes().size()==2);
 MusicSceneGraph wrong;CHECK(wrong.initialize({data.silence_db(),0,data.fade_to_seconds()+1,16},error));CHECK(!wrong.attach_scene(1,data,error));
 // The callback itself does not check a disabled CollisionShape: that belongs
 // to the geometry emitter. Negative contexts are checked above the flags.
 CHECK(g.players().stop(1,error));CHECK(g.enter(2,path,context,error));CHECK(g.exit(2,path,context,error));for(unsigned i=0;i<1023;++i)CHECK(g.exit(2,path,context,error));before=digest(g);reject(g.exit(2,path,context,error));CHECK(g.idle_frame(2,error));
}
}
int main(int argc,char**argv){CHECK(argc==4);MusicRegionData podunk,mayor;CHECK(podunk.load_file(argv[1],error));CHECK(mayor.load_file(argv[2],error));reference(podunk,mayor,argv[3]);negative(podunk);std::printf("Music scene graph: %u checks; 10 original scene callback sequences, global ordered loop nodes, deferred exits, source flags/UI and negative ownership/epoch/capacity rollback; no physical bank/DSP integration claim\n",checks);}
