#pragma once
#include "encore/phone_data.hpp"
#include <vector>

namespace encore::upstream {
class PhoneFlagQuery {
public:
 virtual ~PhoneFlagQuery()=default;
 // False means an unresolved/rejected binding. Pack flags are resolved by the caller against Room.
 virtual bool get_phone_flag(uint32_t flag_ref,bool&value)const=0;
};
enum class PhoneSoundKind:uint32_t {Ring=1,Hangup=2};
// object is the source audio-player identity. Requests replace/restart that
// player's playback, rather than adding independent overlapping voices.
struct PhoneSoundRequest {PhoneSoundKind kind=PhoneSoundKind::Ring;uint32_t object=phone_no_index,resource=0,bus=0;Vec2 position{};bool positional=false;};
class PhoneSoundSink {public:virtual ~PhoneSoundSink()=default;virtual bool play_phone_sound(const PhoneSoundRequest&)=0;};
struct PhoneInteraction {uint32_t program=0,phone_location=0;PhoneSoundRequest sound{};};
struct PhonePose {uint32_t resource=phone_no_index,frame=0;Vec2 center{},sort_origin{};bool centered=false;};
class PhoneRuntime {
public:
 bool initialize(PhoneView);
 bool ring(uint32_t object);
 // Resolves before changing state. Caller opens returned program, then emits returned hangup request.
 bool interact(uint32_t object,const PhoneFlagQuery&,PhoneInteraction&);
 bool interaction_program(uint32_t object,const PhoneFlagQuery&,uint32_t&program)const;
 // Unpaused source AnimationPlayer idle delta, including during a dialogue/cutscene.
 bool advance(double delta,PhoneSoundSink&);
 PhonePose pose(uint32_t object)const;
 bool ringing(uint32_t object)const;
 double clip_time(uint32_t object)const;
 bool valid()const{return view_.valid()&&!failed_;}
 const char*error()const{return error_;}
 PhoneView view()const{return view_;}
private:
 struct State {uint32_t clip=phone_no_index,frame=0;double time=0;bool started=false;};
 PhoneView view_;std::vector<State>states_;bool failed_=false;const char*error_="";
 bool fail(const char*);PhoneSoundRequest sound(uint32_t object,uint32_t resource,PhoneSoundKind kind)const;
};
}
