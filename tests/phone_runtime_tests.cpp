#include "encore/phone_runtime.hpp"
#include "encore/content.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
using namespace encore::upstream;
namespace {
int failures=0;
void check(bool v,const char*m){if(!v){std::cerr<<"FAIL: "<<m<<'\n';++failures;}}
struct Flags:PhoneFlagQuery {std::vector<bool>values;bool resolved=true;bool get_phone_flag(uint32_t index,bool&v)const override{if(!resolved||index>=values.size())return false;v=values[index];return true;}};
struct Sounds:PhoneSoundSink {std::vector<PhoneSoundRequest>requests;bool accepts=true;bool play_phone_sound(const PhoneSoundRequest&r)override{requests.push_back(r);return accepts;}};
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);}
}
int main(int argc,char**argv){
 if(argc!=3){std::cerr<<"usage: phone_runtime_tests opening.encphone native-animation.bin\n";return 2;}
 PhoneData data;std::string error;check(data.load_file(argv[1],error),error.c_str());if(!data.view())return 1;
 auto view=data.view();auto object=view.object(0);PhoneRuntime runtime;Flags flags;flags.values.resize(view.count(PhoneSection::FlagRefs),false);Sounds sounds;
 check(runtime.initialize(view),"initialize");check(!runtime.ringing(0),"default idle never rings from flags");
 check(runtime.pose(0).frame==object.initial_frame,"source initial frame");
 check(runtime.advance(double(1.0f/60),sounds)&&sounds.requests.empty(),"idle emits no sound");
 PhoneInteraction result;
 check(runtime.interact(0,flags,result)&&result.program==object.default_program,"default dispatch");
 check(result.sound.kind==PhoneSoundKind::Hangup&&result.sound.resource==object.hangup_sound&&result.sound.positional,"typed hangup request");
 check(result.sound.position.x==object.audio_center.x&&result.sound.position.y==object.audio_center.y&&result.sound.bus==object.audio_bus,"sound spatial metadata");
 check(result.phone_location==object.phone_location,"phone location returned");
 for(uint32_t i=0;i<object.dispatch_count;++i){auto d=view.dispatch(object.first_dispatch+i);flags.values[d.flag_ref]=true;check(runtime.interact(0,flags,result)&&result.program==d.program,"last matching override wins");}
 check(runtime.ring(0),"ring start");check(runtime.advance(double(1.0f/60),sounds),"ring update");auto time=runtime.clip_time(0);auto frame=runtime.pose(0).frame;
 check(runtime.ring(0)&&runtime.clip_time(0)==time&&runtime.pose(0).frame==frame,"already-ringing call is idempotent");
 check(runtime.interact(0,flags,result)&&!runtime.ringing(0),"interaction stops ring");
 check(runtime.advance(double(1.0f/60),sounds)&&runtime.pose(0).frame==object.initial_frame,"Idle key applies next native process");
 check(runtime.pose(0).center.x==object.sprite_center.x&&runtime.pose(0).sort_origin.y==object.position.y,"renderer pose");
 // Match original Godot AnimationPlayer at every step, including late loop-key
 // crossings caused by real_t rounding, idempotent ring and Idle transition.
 std::vector<uint8_t>reference;check(encore::read_file(argv[2],reference,1024*1024,error),"read native oracle");
 if(reference.size()<20||std::memcmp(reference.data(),"PNREF001",8)){check(false,"native oracle header");return 1;}
 auto count=get(reference,8);uint64_t bits=uint64_t(get(reference,12))|(uint64_t(get(reference,16))<<32);double delta;std::memcpy(&delta,&bits,8);
 check(reference.size()==20+size_t(count)*24,"native oracle size");if(reference.size()!=20+size_t(count)*24)return 1;
 check(runtime.initialize(view),"oracle reset");flags.values.assign(flags.values.size(),false);sounds.requests.clear();
 for(uint32_t i=0;i<count;++i){size_t at=20+size_t(i)*24;auto tick=get(reference,at),action=get(reference,at+4),before=get(reference,at+8),after=get(reference,at+12),sound_count=get(reference,at+16),ringing=get(reference,at+20);
  check(tick==i,"oracle tick ordering");
  if(action==1)check(runtime.ring(0),"oracle ring");else if(action==2)check(runtime.interact(0,flags,result),"oracle interact");else check(action==0,"oracle action");
  if(runtime.pose(0).frame!=before){std::cerr<<"Before frame mismatch at "<<tick<<'\n';++failures;}
  sounds.requests.clear();check(runtime.advance(delta,sounds),"oracle advance");
  if(runtime.pose(0).frame!=after||sounds.requests.size()!=sound_count||runtime.ringing(0)!=bool(ringing)){
   std::cerr<<"Oracle mismatch at "<<tick<<" frame="<<runtime.pose(0).frame<<" expected="<<after<<" sounds="<<sounds.requests.size()<<" expected="<<sound_count<<'\n';++failures;
  }
 }
 check(runtime.initialize(view),"invalid query reset");flags.resolved=false;result.program=UINT32_MAX;
 check(!runtime.interact(0,flags,result)&&result.program==UINT32_MAX&&!runtime.valid(),"unresolved flags reject without result");
 for(double invalid:{0.0,-1.0,.251,double(INFINITY),double(NAN)}){check(runtime.initialize(view),"delta reset");check(!runtime.advance(invalid,sounds)&&!runtime.valid(),"invalid delta rejected");}
 check(runtime.initialize(view)&&!runtime.ring(UINT32_MAX),"invalid object rejected");
 check(runtime.initialize(view)&&runtime.ring(0),"sink reset");sounds.accepts=false;
 check(!runtime.advance(.1,sounds)&&!runtime.valid(),"audio failure surfaced");
 std::cout<<"Phone runtime/native oracle: "<<count<<" frames, "<<failures<<" failures\n";return failures?1:0;
}
