// Actual presentation headers and real metadata, isolated texture/draw APIs.
// This verifies draw ownership/UVs/order, not GPU pixels or hardware output.
#include "opening_renderer.hpp"
#include "encore/resource_catalog.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Player renderer %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
bool near(float a,float b){return std::fabs(a-b)<.00001f;}
}
int main(int argc,char**argv){
 CHECK(argc==2);const std::string root=argv[1];ResourceCatalog catalog;CHECK(catalog.load_file((root+"/system/native.encresources").c_str(),error));FieldSwingResources resources;CHECK(resources.load(catalog,root.c_str(),error));RoomData room;CHECK(room.load_file((root+"/"+catalog.path(ResourceRole::Room)).c_str(),error));const auto view=room.view();
 for(uint32_t i=0;i<view.resource_count();++i){const auto r=view.resource(i);if(r.kind==1)draw_test::specs["romfs:/"+std::string(view.string(r.path_string))]={100+i,r.width,r.height};}
 for(size_t i=0;i<2;++i){const auto&layer=resources.graphics().layers()[i];draw_test::specs[root+"/"+layer.path]={unsigned(1000+i),layer.width,layer.height};}
 FieldPlayerRenderer renderer;FieldSwingResources empty;CHECK(!renderer.load(empty,root.c_str(),error)&&!renderer.ready()&&!draw_test::live);
 for(size_t i=0;i<2;++i){auto&spec=draw_test::specs[root+"/"+resources.graphics().layers()[i].path];const auto saved=spec;
  spec.missing=true;CHECK(!renderer.load(resources,root.c_str(),error)&&!renderer.ready()&&!draw_test::live);spec=saved;spec.rotated=true;CHECK(!renderer.load(resources,root.c_str(),error)&&!draw_test::live);spec=saved;++spec.width;CHECK(!renderer.load(resources,root.c_str(),error)&&!draw_test::live);spec=saved;
 }
 CHECK(renderer.load(resources,root.c_str(),error)&&renderer.ready()&&draw_test::live==2);CHECK(!renderer.load(resources,root.c_str(),error)&&renderer.ready()&&draw_test::live==2);
 OpeningActorRenderer actors;CHECK(actors.load(view,error));const auto retained=draw_test::live;const auto profile=view.actor_profile(view.actor_instance(view.scene().player_instance_index).profile_index);CHECK(profile.flags&1);const Vec2 pos{73.4f,95.1f};const float cx=19.2f,cy=27.8f;bool saw_front=false,saw_behind=false,saw_hidden_main=false;
 FieldPlayerTree unready;CHECK(!renderer.draw(unready,pos,cx,cy));
 for(const auto&clip:resources.swing().clips()){
  FieldPlayerTree tree;CHECK(tree.initialize(resources.entry(),resources.action(),resources.swing(),resources.entry().idle(),clip.direction,0,"",error));SwingActionContext context;context.direction=clip.direction;context.skill_available=true;SwingMotionFlags motion;bool accepted=false;CHECK(tree.input(resources.action().strings()[2],true,context,motion,accepted,error)&&accepted);
  for(unsigned tick=0;tick<50;++tick){CHECK(tree.advance(1.0/60,error));const auto&pose=tree.pose();CHECK(tree.main_frame()<=UINT16_MAX);const AnimationSample sample{uint16_t(tree.main_frame()),pose.main,pose.special};draw_test::calls.clear();
   if(tick==0){CHECK(!actors.draw_player(pos,sample,cx,cy,&tree)&&draw_test::calls.empty());actors.set_player_renderer(&renderer);}
   CHECK(actors.draw_player(pos,sample,cx,cy,&tree));const auto&calls=draw_test::calls;CHECK(calls.size()==1+unsigned(pose.main)+unsigned(pose.special)*2);CHECK(calls[0].id==100+profile.shadow_resource);CHECK(near(calls[0].x,std::floor(pos.x+profile.shadow_offset.x-cx-view.resource(profile.shadow_resource).width*.5f+.5f)));
   if(pose.main)CHECK(calls[1].id==100+profile.primary_resource);
   if(pose.special){saw_hidden_main|=!pose.main;saw_front|=!pose.behind;saw_behind|=pose.behind;const auto at=1+unsigned(pose.main);CHECK(calls[at].id==unsigned(pose.behind?1001:1000)&&calls[at+1].id==unsigned(pose.behind?1000:1001));const auto*frame=resources.graphics().frame(pose.frame);CHECK(frame);
    for(size_t j=at;j<calls.size();++j){const auto index=calls[j].id-1000;const auto&layer=resources.graphics().layers()[index];auto origin=pos;if(layer.parent){origin.x+=resources.graphics().layers()[0].position.x;origin.y+=resources.graphics().layers()[0].position.y;}origin.x+=layer.position.x+layer.offset.x;origin.y+=layer.position.y+layer.offset.y;if(layer.flags&1){origin.x-=frame->width*.5f;origin.y-=frame->height*.5f;}
     CHECK(near(calls[j].x,std::floor(origin.x-cx+.5f))&&near(calls[j].y,std::floor(origin.y-cy+.5f)));CHECK(calls[j].sub.width==frame->width&&calls[j].sub.height==frame->height);CHECK(near(calls[j].sub.left,float(frame->x)/512)&&near(calls[j].sub.right,float(frame->x+frame->width)/512)&&near(calls[j].sub.top,1-float(frame->y)/512)&&near(calls[j].sub.bottom,1-float(frame->y+frame->height)/512));
    }
   }
   CHECK(tree.flush(motion,error));if(!tree.attacking()&&tick>4)break;
  }
  actors.set_player_renderer(nullptr);
 }
 CHECK(saw_front&&saw_behind&&saw_hidden_main&&draw_test::live==retained);actors.free();renderer.free();CHECK(!draw_test::live&&!renderer.ready());std::printf("Player renderer: %u checks; actual eight-way metadata, padded UVs, parent origins, front/behind bat, independent shadow, rejected GPU admission and cleanup; API double only\n",checks);
}
