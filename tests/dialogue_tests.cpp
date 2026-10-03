#include "encore/dialogue.hpp"
#include "room_fixture.hpp"
// Preserve the generated native trace unchanged, including its source labels.
// These legacy record types exist only in the fixture namespace.
namespace encore::upstream::reference {
enum class DialogueActor { None, Ninten, Lamp };
struct DialogueAction {
    DialogueActionKind kind; DialogueActor actor; uint32_t phrase; Vec2 vector;
    double value,duration; const char* text; const char* detail;
};
}
#include "fixtures/lamp_dialogue_v0410.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;
bool check(bool ok,const char* message) {++checks;if(!ok)std::fprintf(stderr,"FAIL: %s\n",message);return ok;}
struct Sink final : DialogueSink {
    unsigned frame=0;
    bool reject=false;
    struct Event { unsigned frame; DialogueAction action; };
    std::vector<Event> events;
    bool apply(const DialogueAction& action) override { if(reject)return false; events.push_back({frame,action});return true; }
};
bool native_action(const reference::DialogueAction& source,DialogueAction& expected) {
    const auto room=encore_test::room();
    expected.kind=source.kind;expected.phrase=source.phrase;expected.vector=source.vector;
    expected.value=source.value;expected.duration=source.duration;
    expected.actor=source.actor==reference::DialogueActor::None?kRoomNoActor:
        uint16_t(encore_test::actor(source.actor==reference::DialogueActor::Ninten?"Ninten":"lamp"));
    const std::string_view text=source.text,detail=source.detail;
    using Kind=DialogueActionKind;
    switch(source.kind) {
        case Kind::BindActor: {
            const auto binding=room.actor_instance(expected.actor).binding_kind;
            return detail.empty()&&((text=="leader"&&binding==1)||(text=="Objects/lamp"&&binding==2));
        }
        case Kind::AnimateActor:
            // Source's play request value=1 is represented by this explicit
            // operation; timeline completion/sampling reside in Clip.flags.
            if(source.value!=1||!detail.empty()||(text!="Idle"&&text!="Open"))return false;
            expected.value=0;expected.target_index=encore_test::actor_clip("lamp",text);
            return expected.target_index!=kRoomNoIndex;
        case Kind::EmoteActor:
            if(text!="surprise"||!detail.empty())return false;
            expected.target_index=encore_test::actor_clip("Ninten",text);return expected.target_index!=kRoomNoIndex;
        case Kind::MoveActor:return text=="position"&&detail.empty();
        case Kind::ShakeCamera:return text=="small"&&detail.empty();
        case Kind::MoveCamera:
            if(text!="sine"||detail!="out")return false;
            expected.target_index=1;return true; // Supported SineOut engine enum.
        case Kind::PlaySound:
            if(detail!="dialogBoxSound")return false;
            expected.target_index=encore_test::resource(text);return true;
        case Kind::CallObjectDeferred: {
            const uint16_t kind=text=="Poltergeist/MusicArea"&&detail=="play_music"?1:
                (text=="Room Shaker"&&detail=="delayed_start"?uint16_t(RoomBindingKind::PeriodicCameraShake):0);
            if(!kind)return false;
            for(uint32_t i=0;i<room.binding_count();++i)if(room.binding(i).kind==kind){expected.target_index=i;return true;}
            return false;
        }
        case Kind::QueueBattle:case Kind::RequestBattle:
            for(uint32_t i=0;i<room.battle_count();++i) {
                const auto battle=room.battle(i);
                if(battle.actor_instance_index==expected.actor&&room.string(battle.enemy_string)==text&&
                   room.string(room.flag(battle.win_flag_index).name_string)==detail){expected.target_index=i;return true;}
            }
            return false;
        default:return text.empty()&&detail.empty();
    }
}
bool same(const DialogueAction& a,const reference::DialogueAction& source) {
    DialogueAction b;if(!native_action(source,b))return false;
    return a.kind==b.kind && a.actor==b.actor && a.phrase==b.phrase && a.vector.x==b.vector.x && a.vector.y==b.vector.y &&
        a.value==b.value && a.duration==b.duration && a.target_index==b.target_index && a.auxiliary_index==b.auxiliary_index && a.flags==b.flags;
}
bool bind(DialoguePlayer& d,Sink& s,uint32_t generation=7) {
    return d.start(encore_test::room(),encore_test::opening_program(),s,generation)&&d.actor_ready(uint16_t(encore_test::actor("Ninten")),generation,s)&&d.actor_ready(uint16_t(encore_test::actor("lamp")),generation,s);
}
}
int main() {
    Sink s;DialoguePlayer d;
    if(!check(d.start(encore_test::room(),encore_test::opening_program(),s,7),"start"))return 1;
    if(!check(d.status()==DialogueStatus::AwaitActor && s.events.size()==2 && d.wait_remaining()==0,"first actor-ready barrier"))return 1;
    if(!check(!d.actor_ready(uint16_t(encore_test::actor("lamp")),7,s),"wrong actor signal rejected"))return 1;
    if(!check(!d.actor_ready(uint16_t(encore_test::actor("Ninten")),6,s),"stale generation rejected"))return 1;
    if(!check(d.actor_ready(uint16_t(encore_test::actor("Ninten")),7,s) && d.status()==DialogueStatus::AwaitActor,"second actor-ready barrier"))return 1;
    if(!check(d.actor_ready(uint16_t(encore_test::actor("lamp")),7,s) && d.status()==DialogueStatus::AwaitIdle && d.wait_remaining()==0,"initial idle yield"))return 1;
    const double delta=static_cast<double>(1.0f/60.0f);
    for(unsigned frame=1;frame<=500 && d.active();++frame) {
        s.frame=frame;
        if(!check(d.idle_begin(s),"idle signal"))return 1;
        if(!check(d.idle_process(delta,s),"idle WaitTimer"))return 1;
        if(frame==363 && !check(d.status()==DialogueStatus::AwaitTimer,"queued battle must wait final .35"))return 1;
        if(frame==383 && !check(d.active(),"battle not requested one tick early"))return 1;
    }
    if(!check(d.status()==DialogueStatus::BattleRequested && s.frame==384,"native final battle handoff frame"))return 1;
    if(!check(s.events.size()==reference::lamp_trace.size(),"native event count"))return 1;
    for(size_t i=0;i<s.events.size();++i) {
        const auto& a=s.events[i];const auto& b=reference::lamp_trace[i];
        if(!check(a.frame==b.frame && same(a.action,b.action),"native command order/payload/frame")) {
            std::fprintf(stderr,"event %zu actual frame%u %s expected frame%u %s\n",i,a.frame,dialogue_action_name(a.action.kind),b.frame,dialogue_action_name(b.action.kind));return 1;
        }
    }
    // Calls after the terminal boundary cannot request battle again.
    if(!check(d.idle_begin(s)&&d.idle_process(delta,s)&&s.events.size()==55,"terminal scheduler inert"))return 1;
    if(!check(!d.start(encore_test::room(),encore_test::opening_program(),s,8)&&!d.actor_ready(uint16_t(encore_test::actor("lamp")),7,s),"completed program cannot restart accidentally"))return 1;
    {
        DialoguePlayer t;Sink log;if(!check(bind(t,log),"setup cancellation"))return 1;
        t.cancel();const auto size=log.events.size();
        if(!check(t.status()==DialogueStatus::Cancelled && !t.active()&&!t.actor_ready(uint16_t(encore_test::actor("lamp")),7,log),"cancel invalidates ready callbacks"))return 1;
        if(!check(t.idle_begin(log)&&t.idle_process(delta,log)&&log.events.size()==size,"cancel has no queued effects"))return 1;
    }
    for(double invalid:{0.,-0.1,0.251,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        DialoguePlayer t;Sink log;if(!bind(t,log)||!t.idle_begin(log))return 1;
        if(!check(!t.idle_process(invalid,log)&&t.status()==DialogueStatus::Error,"unsupported delta fails closed"))return 1;
    }
    {
        DialoguePlayer t;Sink log;log.reject=true;
        if(!check(!t.start(encore_test::room(),encore_test::opening_program(),log)&&t.status()==DialogueStatus::Error,"sink rejection stops dispatch"))return 1;
    }
    {
        DialoguePlayer t;Sink log;if(!bind(t,log))return 1;
        if(!check(!t.idle_process(delta,log),"WaitTimer before idle signal fails"))return 1;
    }
    {
        DialoguePlayer t;Sink log;if(!bind(t,log)||!t.idle_begin(log))return 1;
        if(!check(!t.idle_begin(log),"duplicate idle signal fails"))return 1;
    }
    {
        DialoguePlayer t;Sink log;if(!bind(t,log)||!t.idle_begin(log))return 1;
        // The native Timer uses <0. Exactly reaching zero has no timeout.
        for(unsigned i=0;i<4;++i) {
            if(i&&!t.idle_begin(log))return 1;
            if(!t.idle_process(.25,log))return 1;
        }
        if(!check(t.phrase()==0 && t.wait_remaining()==0 && t.active(),"strictly negative timeout edge"))return 1;
        if(!t.idle_begin(log)||!t.idle_process(.01,log))return 1;
        if(!check(t.phrase()==1 && t.wait_remaining()==1.5,"timeout overshoot discarded on timer restart"))return 1;
    }
    std::printf("Lamp dialogue: %u checks; 55 actions match native Godot frames; final request at frame384\n",checks);
}
