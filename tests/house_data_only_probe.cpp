#include "encore/house_data.hpp"
#include <cstdio>
using namespace encore::upstream;
int main(int argc,char**argv){
 if(argc!=2)return 2;
 HouseData data;std::string error;
 if(!data.load_file(argv[1],error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
 auto v=data.view();auto d=v.door(0);auto n=v.npc(0);auto q=v.interaction();auto text=v.string(v.token(v.segment(0).first_token).text);
 std::printf("destination=%.9g,%.9g fade_speed=%.17g ray_length=%.9g view_radius=%.9g text=%.*s\n",d.destination.x,d.destination.y,d.fade_in_speed,q.ray_length,n.view_radius,int(text.size()),text.data());
 auto o=v.openable_door(1);auto story=v.story_trigger(0);auto profile=v.profile(v.npc(1).profile);auto blocked=v.string(v.token(v.segment(v.dialogue(o.blocked_dialogue).first_segment).first_token).text);
 std::printf("close_delay=%.9g ram_y=%.9g ram_strength=%.9g sprite_y=%.9g story_center=%.9g,%.9g condition_value=%u blocked=%.*s\n",o.close_delay,o.ram_required_y,o.ram_strength,profile.sprite_offset.y,story.center.x,story.center.y,v.story_condition(story.first_condition).value,int(blocked.size()),blocked.data());
}
