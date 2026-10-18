#include "encore/field_scene.hpp"
#include <cstdio>
using namespace encore::upstream;
int main(){std::string e;RoomData r;FieldMapData m;WorldLinksData l;
if(!r.load_file("romfs/world/podunk/podunk.encroom",e)||!m.load_file("romfs/world/podunk/podunk.encmap",e)||!l.load_file("romfs/world/world.enclinks",e)){std::fprintf(stderr,"%s",e.c_str());return 1;}
std::vector<bool>flags(r.view().flag_count(),false);for(unsigned i=0;i<flags.size();++i){auto id=r.view().string(r.view().flag(i).name_string);flags[i]=id=="beat_dog_bridge"||id=="pippis_mom_call"||id=="pippis_mom_help";}
FieldScene f;if(!f.prepare(r.view(),m.view(),l,flags,{643,283},{0,1},{400,240},e)||!f.finish_transition())return 2;
struct Step{double x,y;Vec2 to;};for(auto step:{Step{-1,0,{584,283}},Step{0,-1,{584,264}},Step{-1,0,{400,264}},Step{1,0,{600,264}},Step{-1,0,{536,264}}}){unsigned still=0;Vec2 last{};for(unsigned tick=0;tick<400;++tick){auto p=f.world.player().position;if((step.x<0&&p.x<=step.to.x)||(step.x>0&&p.x>=step.to.x)||(step.y<0&&p.y<=step.to.y))break;WalkInput in{float(step.x),float(step.y)};if(!f.before_physics(in)||!f.world.advance(in)||!f.after_physics()||!f.world.idle_frame(1./60))return 3;p=f.world.player().position;still=p.x==last.x&&p.y==last.y?still+1:0;if(still>10){std::printf("blocked %.1f %.1f toward %.1f %.1f\n",p.x,p.y,step.to.x,step.to.y);return 4;}last=p;}auto p=f.world.player().position;std::printf("waypoint %.1f %.1f\n",p.x,p.y);}
}
