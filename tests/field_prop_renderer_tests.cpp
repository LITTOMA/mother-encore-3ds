// Actual unchanged FieldRenderer header, isolated texture/draw/actor services.
// Core scene/collision/props use real data; this is not GPU or pixel matching.
#include "field_renderer.hpp"
#include "encore/field_scene_resources.hpp"
#include "encore/resource_catalog.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Prop renderer %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
bool near(float a,float b){return std::fabs(a-b)<.00001f;}
}
int main(int argc,char**argv){
 CHECK(argc==4);const std::string root=argv[1];auto path=[&](const char*p){return root+"/"+p;};ResourceCatalog catalog;WorldLinksData links;RoomData domain;ItemData items;FieldSceneResources resources;
 CHECK(catalog.load_file(path("system/native.encresources").c_str(),error)&&links.load_file(path("world/world.enclinks").c_str(),error));CHECK(domain.load_file(path("world/house/opening.encroom").c_str(),error)&&items.load_file(path("ui/opening.encitems").c_str(),error));CHECK(resources.load(catalog,links,2,root.c_str(),domain.view(),items.view(),error));CHECK(resources.props().rendering_bound());
 FieldMapData legacy;FieldPropsData props;CHECK(legacy.load_file(argv[2],error)&&props.load_file(argv[3],error)&&props.capability()==2);const auto map=legacy.view();const auto&p=props.policy();const auto&r=props.props()[0];const Vec2 player{r.position.x-40,r.position.y};std::vector<bool>flags(resources.room().flag_count(),false);FieldScene scene;
 CHECK(scene.prepare(resources.room(),map,links,flags,player,{1,0},{400,240},error)&&scene.bind_props(props,error)&&scene.idle_frame(0,false));
 for(uint32_t i=0;i<map.count(FieldMapSection::Atlases);++i){const auto a=map.atlas(i);draw_test::specs["romfs:/"+std::string(map.text(a.path))]={i+1,a.width,a.height,false,false};}
 const auto texture="romfs:/"+p.texture;draw_test::specs[texture]={8000,p.width*props.clips().frame_count(),p.height,false,false};FieldRenderer renderer;OpeningActorRenderer actors;CHECK(renderer.load(map,"en",error,&props));
 const auto draw=[&]{draw_test::calls.clear();CHECK(renderer.draw(scene,actors,r.position.x-200,r.position.y-120,400,240,0,0,0));};
 draw();unsigned roots=0,same_spot=0;const auto s=map.sprite(r.sprite);const float sx=std::floor(s.position.x-(r.position.x-200)+.5f),sy=std::floor(s.position.y-(r.position.y-120)+.5f);
 for(auto c:draw_test::calls){if(c.id==8000){++roots;CHECK(c.sub.width==p.width&&c.sub.height==p.height);CHECK(near(c.sub.left,float(props.clips().initial().frame*p.width)/1024));}if(c.x==sx&&c.y==sy&&c.sub.width==p.width&&c.sub.height==p.height)++same_spot;}
 CHECK(roots==1&&same_spot==1); // No second static frame12 under the owner.
 CHECK(scene.hit_prop(0,error)&&scene.idle_frame(0,false));draw();unsigned clone_at=UINT32_MAX,root_at=UINT32_MAX,player_at=UINT32_MAX;roots=0;
 for(uint32_t i=0;i<draw_test::calls.size();++i){auto c=draw_test::calls[i];if(c.id==9000)player_at=i;if(c.id==8000){++roots;if(near(c.sub.left,float(p.clone_frame*p.width)/1024))clone_at=i;else root_at=i;}}
 CHECK(roots==2&&clone_at<root_at&&clone_at<player_at); // Source BelowPlayer1 subtree before sorted actors/root.
 CHECK(scene.idle_frame(1,false));draw();roots=0;for(auto c:draw_test::calls)if(c.id==8000)++roots;CHECK(roots==2); // Source leaves the stopped frame0 and independent clone alive.
 renderer.free();CHECK(draw_test::live==0&&!renderer.ready());
 for(unsigned bad=0;bad<4;++bad){auto before=draw_test::specs[texture];auto&spec=draw_test::specs[texture];if(bad==0)spec.width-=1;if(bad==1)spec.height+=1;if(bad==2)spec.rotated=true;if(bad==3)spec.missing=true;CHECK(!renderer.load(map,"en",error,&props)&&!renderer.ready()&&draw_test::live==0);draw_test::specs[texture]=before;CHECK(renderer.load(map,"en",error,&props));renderer.free();CHECK(draw_test::live==0);}
 CHECK(renderer.load(map,"en",error));CHECK(!renderer.draw(scene,actors,0,0,400,240,0,0,0));renderer.free();CHECK(draw_test::live==0);
 std::printf("Field props renderer: %u checks; unchanged production header with explicit texture/draw service doubles, source frame UVs, no static duplicate, clone below actor/root, owner mismatch and allocation cleanup; no GPU/pixel/emulator claim\n",checks);
}
