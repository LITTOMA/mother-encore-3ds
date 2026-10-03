#include "encore/world.hpp"
#include "encore/progression.hpp"
#include <cstdio>
#include <string>
using namespace encore::upstream;
struct Probe final:DialogueSink {double first_wait=-1;DialogueActor pending=kRoomNoActor;bool apply(const DialogueAction& a)override{if(a.kind==DialogueActionKind::BindActor)pending=a.actor;if(a.kind==DialogueActionKind::StartWait&&first_wait<0)first_wait=a.duration;return true;}};
int main(int argc,char**argv){
 if(argc!=2)return 2;
 RoomData data;std::string error;if(!data.load_file(argv[1],error)){std::fprintf(stderr,"%s\n",error.c_str());return 3;}
 const auto room=data.view();OpeningWorld world;if(!world.initialize(room)){std::fprintf(stderr,"%s\n",world.error());return 4;}
 WorldFlags flags;if(!flags.initialize(room))return 5;
 DialoguePlayer script;Probe sink;if(!script.start(room,room.trigger(0).program_index,sink,1))return 6;
 for(uint32_t i=0;i<room.actor_instance_count()&&sink.pending!=kRoomNoActor;++i){auto p=sink.pending;sink.pending=kRoomNoActor;if(!script.actor_ready(p,1,sink))return 7;}
 if(!script.idle_begin(sink)||sink.first_wait<0)return 8;
 int32_t xp;if(!experience_for_level(room,2,xp))return 9;
 const auto player=room.scene().player_instance_index;
 std::printf("{\"spawn_x\":%.9g,\"initial_animation_frame\":%u,\"first_script_wait\":%.17g,\"layer_sort_y\":%.9g,\"actor_sprite_offset_x\":%.9g,\"registered_flag_count\":%zu,\"level_2_required_xp\":%d,\"level_at_9xp\":%d,\"new_flag_known\":%s}\n",double(world.player().position.x),unsigned(world.animation().frame),sink.first_wait,double(room.overlay(0).sort_y),double(world.actor(player).sprite_offset.x),flags.story_flag_count(),xp,level_for_experience(room,9),flags.has_story_flag("data_only_probe_flag")?"true":"false");
}
