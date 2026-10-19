#include "encore/field_objects.hpp"
#include "encore/content.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace{unsigned checks=0;std::string error;
void check(bool value,const char*why){++checks;if(!value){std::fprintf(stderr,"Object art %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
}
int main(int argc,char**argv){
 CHECK(argc==5);FieldObjectsData data;FieldMapData map;RoomData room;std::vector<uint8_t>texture;
 CHECK(data.load_file(argv[1],error)&&encore::read_file(argv[2],texture,1024*1024,error)&&map.load_file(argv[3],error)&&room.load_file(argv[4],error));
 CHECK(data.capabilities()==3&&data.rendering_bound()&&data.drawing_bound()&&data.bind(room.view(),map.view(),error));CHECK(data.texture_matches(texture.data(),texture.size(),error));
 CHECK(data.objects().size()==1&&data.frames().size()==6&&data.animation_frames().size()==40&&data.width()==32&&data.height()==32);
 const auto&d=data.objects()[0];const auto&main=data.frames()[d.main_frame];CHECK(main.width==20&&main.height==20&&d.path=="Objects/Items/Item"&&d.ready_flag=="got_shovel");CHECK(d.position.x==1432&&d.position.y==4664&&d.effect_offset.x==4&&d.effect_offset.y==-4);
 CHECK(d.initial==22&&d.resolved==6&&d.fps==12&&d.random_min==0&&d.random_max==47&&d.body==kFieldNone&&d.body_layer==4&&d.body_mask==0);
 for(auto frame:data.animation_frames())CHECK(data.frames()[frame].width==7&&data.frames()[frame].height==7);
 FieldObjects owner;CHECK(owner.prepare(data,room.view(),error)&&owner.enter_ready(d.id,false,error)&&owner.state(d.id)->frame==6);CHECK(!owner.state(d.id)->visible&&owner.idle_frame(error)&&owner.state(d.id)->visible);
 CHECK(!data.texture_matches(nullptr,texture.size(),error)&&!data.texture_matches(texture.data(),texture.size()-1,error));auto bad=texture;bad[bad.size()/2]^=1;CHECK(!data.texture_matches(bad.data(),bad.size(),error));CHECK(data.texture_matches(texture.data(),texture.size(),error));
 std::printf("Actual object art: %u checks; genuine tex3ds bytes, source main/Sparkles frame mappings, map bindings and byte corruption rejection; no GPU/emulator/hardware claim\n",checks);
}
