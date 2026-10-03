#include "room_fixture.hpp"
#include "encore/world.hpp"
#include <cstdio>
#include <cstring>
int main(){using namespace encore::upstream;
    OpeningWorld world;
    if(!world.initialize(encore_test::room())){std::fprintf(stderr,"%s\n",world.error());return 1;}
    if(world.player().position.x!=520||world.player().position.y!=397)return 2;
    for(unsigned i=0;i<120;++i)if(!world.advance({1,0,false,false,false}))return 3;
    const auto right=world.player().position;
    for(unsigned i=0;i<60;++i)if(!world.advance({1,0,false,false,false}))return 4;
    if(right.x!=584||right.x!=world.player().position.x||right.y!=world.player().position.y)return 5;
    if(!world.initialize(encore_test::room()))return 6;
    for(unsigned i=0;i<100;++i)if(!world.advance({0,-1,false,false,false}))return 7;
    if(world.player().position.y!=397||!world.healthy())return 8;
    if(!world.initialize(encore_test::room()))return 9;
    constexpr double delta=double(1.0f/60.0f);
    for(unsigned i=0;i<200&&world.stage()==OpeningStage::Walking;++i){
        if(!world.advance({-1,0,false,false,false})||!world.idle_frame(delta)){std::fprintf(stderr,"trigger: %s\n",world.error());return 10;}
    }
    if(!world.cutscene_active()||world.player().position.x!=430||!world.has_cutscene_actors())return 11;
    bool saw_queue=false;unsigned queued_frame=0,requested_frame=0;
    for(unsigned frame=0;frame<1200;++frame){
        if(!world.advance({1,1,true,false,false})||!world.idle_frame(delta)){
            std::fprintf(stderr,"frame %u phrase %u: %s\n",frame,world.phrase(),world.error());return 12;
        }
        if(world.battle_request().queued&&!saw_queue){saw_queue=true;queued_frame=frame;}
        if(world.stage()==OpeningStage::BattleRequested){requested_frame=frame;break;}
    }
    const auto& battle=world.battle_request();
    if(!world.healthy()||!battle.queued||!battle.requested||battle.can_run||battle.advantage!=0||!battle.overworld_music||std::strcmp(battle.enemy,"lamp")||std::strcmp(battle.win_flag,"poltergeist"))return 13;
    if(!saw_queue||requested_frame<=queued_frame||requested_frame-queued_frame<20)return 14;
    if(world.audio_request_count()!=5||!world.actor_cleanup_pending())return 15;
    unsigned moves=0,jumps=0,requests=0;
    for(const auto& event:world.action_trace()){
        if(event.action.kind==DialogueActionKind::MoveActor)++moves;
        if(event.action.kind==DialogueActionKind::JumpActor)++jumps;
        if(event.action.kind==DialogueActionKind::RequestBattle)++requests;
        std::printf("%llu,%llu,phrase%u,%s\n",(unsigned long long)event.physics_tick,(unsigned long long)event.idle_frame,event.action.phrase,dialogue_action_name(event.action.kind));
    }
    if(moves!=5||jumps!=4||requests!=1)return 16;
    if(world.player().position.x!=430||world.player().direction.x!=1)return 17;
    if(world.actor(world.battle_request().actor_index).position.x!=432||world.actor(world.battle_request().actor_index).position.y!=392)return 18;
    const auto frozen=world.actor(world.battle_request().actor_index).position;
    for(unsigned i=0;i<30;++i)if(!world.advance({-1,0,false,false,false})||!world.idle_frame(delta))return 19;
    if(world.actor(world.battle_request().actor_index).position.x!=frozen.x)return 20;
    std::printf("13-phrase lamp_attack dispatched to real battle request: queue frame %u, request frame %u; audio5 typed requests (no playback)\n",queued_frame,requested_frame);
    // User report: apparent bedroom escape at (485,471). The original wall
    // is y480 and the actor hull extends nine pixels below its origin.
    // Preserve that exact contact; source Above tiles hide the lower sprite.
    if(!world.initialize(encore_test::room())||!world.warp_same_scene({485,420},{0,1}))return 21;
    for(unsigned i=0;i<240;++i)if(!world.advance({0,1,false,false,false})||!world.idle_frame(delta))return 22;
    if(world.player().position.x!=485||world.player().position.y!=471)return 23;
    unsigned foreground=0;bool masks_contact=false;
    for(unsigned i=0;i<world.content().overlay_count();++i){
        const auto tile=world.content().overlay(i);
        if(tile.flags&uint16_t(RoomOverlayFlag::Foreground)){
            ++foreground;
            masks_contact|=tile.x==480&&tile.y==464&&tile.w==16&&tile.h==16;
        }
    }
    if(foreground!=276||!masks_contact)return 24;
}
