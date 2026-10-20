#include "encore/prop_clips.hpp"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0,failures=0;
void check(bool v,const char*s){++checks;if(!v){if(failures<15)std::fprintf(stderr,"Prop play continuation: %s\n",s);++failures;}}
struct Model {PropClipsData data;PropClipPlayer player;};
}
int main(int argc,char**argv){
 if(argc!=3)return 2;std::vector<std::string>first;
 for(unsigned pass=0;pass<2;++pass){
  std::ifstream input(argv[2]);if(!input)return 2;std::string line;std::getline(input,line);std::map<std::string,Model>models;std::vector<std::string>projection;unsigned rows=0;std::string error;
  while(std::getline(input,line)){
   std::istringstream s(line);std::string file,op,completed,extra;double value=0,position=0;uint32_t frame=0,visible=0,body=0,playing=0,current=0;
   if(!(s>>file>>op>>value>>frame>>visible>>body>>playing>>current>>position>>completed)||(s>>extra)||file.find_first_of("/\\:")!=std::string::npos)return 2;
   auto found=models.find(file);if(found==models.end()){
    auto&model=models[file];if(!model.data.load_file((std::string(argv[1])+"/"+file).c_str(),error)||!model.player.initialize(model.data,error))return 2;found=models.find(file);
   }
   auto&player=found->second.player;
   if(op=="play")check(value==uint32_t(value)&&player.play(uint32_t(value),error),"play accepted");else check(op=="advance"&&player.advance(value,error),"advance accepted");
   std::ostringstream emitted;for(auto id:player.take_completed()){if(emitted.tellp()>0)emitted<<',';emitted<<id;}const auto actual=emitted.str().empty()?"-":emitted.str();const auto pose=player.pose();
   const bool position_matches=std::abs(player.position()-position)<.0000001;
   if(!position_matches&&failures<15)std::fprintf(stderr,"Original row%u %s %s %.17g: time actual%.17g expected%.17g\n",rows,file.c_str(),op.c_str(),value,player.position(),position);
   check(position_matches,"original play/advance position");check(pose.frame==frame&&pose.visible==bool(visible)&&pose.body==bool(body)&&!pose.hitbox&&!pose.interaction,"original properties");
   check(player.playing()==bool(playing)&&(player.playing()?player.assigned_clip():0)==current,"original playing state");check(actual==completed,"original completions");
   const auto before=player.position();check(!player.play(65535,error)&&before==player.position(),"unknown request preserves continuation");
   std::ostringstream state;state<<std::setprecision(17)<<player.position()<<' '<<pose.frame<<' '<<pose.body<<' '<<player.playing()<<' '<<player.assigned_clip()<<' '<<actual;projection.push_back(state.str());++rows;
  }
  check(rows==231&&models.size()==3,"all source clips observed");if(!pass)first=projection;else check(first==projection,"repeat native projection identical");
 }
 std::printf("Prop same-animation play: %u checks,%u failures;231 original observations replayed twice; exact source position and properties\n",checks,failures);return failures?1:0;
}
