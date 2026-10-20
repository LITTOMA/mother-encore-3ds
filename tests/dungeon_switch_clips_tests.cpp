#include "encore/prop_clips.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Dungeon clips %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
struct Row {std::string file,op,completed;double value=0;uint32_t frame=0,visible=0,body=0,playing=0,current=0;};
struct Model {PropClipsData data;PropClipPlayer player;};
}
int main(int argc,char**argv){
 CHECK(argc==3);std::ifstream input(argv[2]);CHECK(bool(input));std::string line;CHECK(bool(std::getline(input,line)));std::vector<Row>rows;
 while(std::getline(input,line)){if(line.empty())continue;Row r;std::istringstream stream(line);std::string extra;CHECK(bool(stream>>r.file>>r.op>>r.value>>r.frame>>r.visible>>r.body>>r.playing>>r.current>>r.completed)&&!(stream>>extra));CHECK(r.file.find_first_of("/\\:")==std::string::npos&&r.file.size()<64);rows.push_back(r);}
 CHECK(rows.size()>500);std::vector<std::string>first_projection;
 for(unsigned pass=0;pass<2;++pass){std::map<std::string,Model>models;std::vector<std::string>projection;unsigned index=0;
  for(auto row:rows){auto found=models.find(row.file);if(found==models.end()){auto&model=models[row.file];CHECK(model.data.load_file((std::string(argv[1])+"/"+row.file).c_str(),error));CHECK(model.player.initialize(model.data,error));found=models.find(row.file);}auto&player=found->second.player;
   if(row.op=="play")CHECK(row.value==uint32_t(row.value)&&player.play(uint32_t(row.value),error));else CHECK(row.op=="advance"&&player.advance(row.value,error));
   std::ostringstream emitted;for(auto id:player.take_completed()){if(emitted.tellp()>0)emitted<<',';emitted<<id;}auto completed=emitted.str().empty()?"-":emitted.str();auto pose=player.pose();
   bool matches=pose.frame==row.frame&&pose.visible==bool(row.visible)&&pose.body==bool(row.body)&&!pose.hitbox&&!pose.interaction&&player.playing()==bool(row.playing)&&(player.playing()?player.assigned_clip():0)==row.current&&completed==row.completed;
   if(!matches)std::fprintf(stderr,"Original row%u %s %s %.17g actual frame%u vis%u body%u playing%u id%u time%.17g completed%s; expected frame%u body%u playing%u id%u completed%s\n",index,row.file.c_str(),row.op.c_str(),row.value,pose.frame,pose.visible,pose.body,player.playing(),player.assigned_clip(),player.position(),completed.c_str(),row.frame,row.body,row.playing,row.current,row.completed.c_str());
   CHECK(matches);std::ostringstream state;state<<pose.frame<<' '<<pose.visible<<' '<<pose.body<<' '<<player.playing()<<' '<<player.assigned_clip()<<' '<<completed;projection.push_back(state.str());++index;
  }
  CHECK(models.size()==3);if(pass==0)first_projection=projection;else CHECK(projection==first_projection);
 }
 std::printf("Dungeon frame/body clips: %u checks;%zu original-engine observations twice across3profiles/11clips; shared property player with original zero-delta sampling, no live scene/texture/contact/audio verification\n",checks,rows.size());
}
