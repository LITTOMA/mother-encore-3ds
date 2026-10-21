#pragma once
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
}
