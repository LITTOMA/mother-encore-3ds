#include "encore/cutscene_camera.hpp"
#include "room_fixture.hpp"
#include "fixtures/cutscene_camera_v0410.hpp"
#include "fixtures/doll_camera_v0410.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0,failures=0;
void check(bool value,const char* name,unsigned tick,const char* field){++checks;if(!value){if(failures<60)std::fprintf(stderr,"%s tick%u: %s\n",name,tick,field);++failures;}}
bool close(Vec2 a,Vec2 b){return std::abs(a.x-b.x)<.00025f&&std::abs(a.y-b.y)<.00025f;}
}
int main(){
    const auto room=encore_test::room();
    const auto magnitude=room.rule_f64(RoomRuleKey::CameraSmallMagnitude);
    const auto shake_duration=room.rule_f64(RoomRuleKey::CameraDefaultShakeSeconds);
    const auto move_duration=room.rule_f64(RoomRuleKey::CameraDefaultMoveSeconds);
    const Vec2 shake_direction{1,0}; // Explicit native fixture's shaker axis.
    for(const auto& c:cutscene_camera_reference::cases){
        CutsceneCamera cam;Vec2 lamp=c.lamp;
        check(cam.initialize(room,c.player),c.name,0,"initialize");
        for(unsigned tick=0;tick<c.frame_count;++tick){
            for(unsigned i=0;i<c.action_count;++i)if(c.actions[i].tick==tick&&!std::strcmp(c.actions[i].op,"lamp"))lamp=c.actions[i].target;
            check(cam.physics_frame(lamp),c.name,tick,"physics");
            const auto& e=c.frames[tick];
            check(close(cam.global_position(),e.physics_global),c.name,tick,"pre-idle global position");
            check(close(cam.center(),e.physics_center),c.name,tick,"pre-idle center");
            for(unsigned i=0;i<c.action_count;++i){
                const auto& a=c.actions[i];if(a.tick!=tick)continue;
                bool ok=true;
                if(!std::strcmp(a.op,"change"))ok=cam.change_to_actor(lamp);
                else if(!std::strcmp(a.op,"move"))ok=cam.move_to(a.target,a.length);
                else if(!std::strcmp(a.op,"lamp"))lamp=a.target;
                else if(!std::strcmp(a.op,"shake"))ok=cam.shake(magnitude,a.length,shake_direction);
                else if(!std::strcmp(a.op,"restore"))ok=cam.restore_player(c.player);
                else if(!std::strcmp(a.op,"pause"))cam.pause();
                else ok=false;
                check(ok,c.name,tick,a.op);
            }
            check(cam.idle_frame(1.0/60.0),c.name,tick,"idle");
            if(!close(cam.center(),e.center)&&failures<60)std::fprintf(stderr,"  center native %.7g,%.7g helper %.7g,%.7g\n",e.center.x,e.center.y,cam.center().x,cam.center().y);
            check(close(cam.center(),e.center),c.name,tick,"center");
            check(close(cam.global_position(),e.global),c.name,tick,"global position");
            check(cam.follows_actor()==e.lamp,c.name,tick,"current camera");
        }
    }
    const auto lamp_checks=checks;
    for(const auto& c:doll_camera_reference::cases){
        CutsceneCamera cam;Vec2 doll=c.doll,mimmie=c.mimmie;
        check(cam.initialize(room,c.player,c.viewport),c.name,0,"initialize");
        check(cam.update_actor_position(11,doll)&&cam.update_actor_position(29,mimmie),c.name,0,"register cameras");
        for(unsigned tick=0;tick<c.frame_count;++tick){
            for(unsigned i=0;i<c.action_count;++i){const auto& a=c.actions[i];if(a.tick!=tick)continue;
                if(!std::strcmp(a.op,"lamp"))doll=a.target;
                if(!std::strcmp(a.op,"mimmie"))mimmie=a.target;
            }
            check(cam.update_actor_position(11,doll)&&cam.update_actor_position(29,mimmie),c.name,tick,"update all actor parents");
            check(cam.physics_frame(1.0/60.0),c.name,tick,"physics");
            const auto& e=c.frames[tick];
            check(close(cam.global_position(),e.physics_global),c.name,tick,"pre-idle global");
            check(close(cam.center(),e.physics_center),c.name,tick,"pre-idle center");
            for(unsigned i=0;i<c.action_count;++i){const auto& a=c.actions[i];if(a.tick!=tick)continue;
                bool ok=true;
                if(!std::strcmp(a.op,"change"))ok=cam.change_to_actor(11,doll);
                else if(!std::strcmp(a.op,"change_mimmie"))ok=cam.change_to_actor(29,mimmie);
                else if(!std::strcmp(a.op,"return"))ok=cam.return_offset({},a.length);
                else if(!std::strcmp(a.op,"move"))ok=cam.move_to(a.target,a.length);
                else if(!std::strcmp(a.op,"shake"))ok=cam.shake(magnitude,a.length,shake_direction);
                else if(!std::strcmp(a.op,"restore"))ok=cam.restore_player(c.player);
                else if(!std::strcmp(a.op,"pause"))cam.pause();
                else if(std::strcmp(a.op,"lamp")&&std::strcmp(a.op,"mimmie"))ok=false;
                check(ok,c.name,tick,a.op);
            }
            check(cam.idle_frame(1.0/60.0),c.name,tick,"idle");
            check(close(cam.center(),e.center),c.name,tick,"center");
            check(close(cam.global_position(),e.global),c.name,tick,"global");
            check(close(cam.display_offset(),e.offset),c.name,tick,"offset");
            check(cam.follows_actor()==(e.camera!=0),c.name,tick,"current camera");
        }
    }
    std::printf("Camera native samples: %u lamp checks, %u Doll/Mimmie checks\n",lamp_checks,checks-lamp_checks);
    CutsceneCamera invalid;
    check(!invalid.change_to_actor({}),"negative",0,"uninitialized change");
    check(!invalid.restore_player({}),"negative",0,"uninitialized restore");
    check(!invalid.move_to({},move_duration),"negative",0,"uninitialized move");
    check(!invalid.shake(magnitude,shake_duration,shake_direction),"negative",0,"uninitialized shake");
    check(!invalid.physics_frame({}),"negative",0,"uninitialized physics");
    check(!invalid.idle_frame(.1),"negative",0,"uninitialized idle");
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    check(!invalid.initialize(room,{nan,0}),"negative",0,"nonfinite initialize");
    check(!invalid.update_actor_position(1,{}),"negative",0,"uninitialized actor");
    check(!invalid.remove_actor(1),"negative",0,"uninitialized remove");
    invalid.initialize(room,{432,397});
    check(!invalid.update_actor_position(1,{nan,0}),"negative",0,"nonfinite actor");
    check(!invalid.remove_actor(1),"negative",0,"missing actor");
    check(invalid.change_to_actor(1,{432,397}),"lifetime",0,"actor create");
    check(!invalid.remove_actor(1),"negative",0,"current actor cannot be freed");
    check(invalid.restore_player({432,397}),"lifetime",0,"player restore");
    check(invalid.remove_actor(1),"lifetime",0,"free dormant actor");
    check(!invalid.remove_actor(1),"negative",0,"double free actor");
    check(!invalid.change_to_actor({0,nan}),"negative",0,"nonfinite change");
    check(!invalid.restore_player({nan,0}),"negative",0,"nonfinite restore");
    check(!invalid.move_to({nan,0},move_duration),"negative",0,"nonfinite move");
    check(!invalid.move_to({},-1),"negative",0,"negative duration");
    check(!invalid.move_to({},nan),"negative",0,"nonfinite duration");
    check(!invalid.shake(magnitude,.001,shake_direction),"negative",0,"too-short shake");
    check(!invalid.shake(magnitude,nan,shake_direction),"negative",0,"nonfinite shake");
    check(!invalid.shake(magnitude,61,shake_direction),"negative",0,"unsupported shake");
    check(!invalid.physics_frame({},-1),"negative",0,"negative physics delta");
    check(!invalid.physics_frame({nan,0}),"negative",0,"nonfinite lamp");
    check(!invalid.idle_frame(nan),"negative",0,"nonfinite idle delta");
    std::printf("Cutscene camera: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
