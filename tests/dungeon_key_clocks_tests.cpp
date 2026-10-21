#include "encore/dungeon_key_clocks.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Key clocks line%d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(8*i));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
std::vector<DungeonKeyFlagValue>flags(const DungeonKeyClocksData&d,uint32_t saved=0){std::vector<DungeonKeyFlagValue>out;for(uint32_t i=0;i<d.metadata().node_count();++i){auto n=d.metadata().node(i);out.push_back({n.id,n.id==saved});}return out;}
struct Pose {unsigned alive,queued,frame,visible,root_visible,playing,clip,audio,disabled,prompt,opened;double x,y,time;};
Pose read_pose(std::istringstream&s){Pose p{};CHECK(bool(s>>p.alive>>p.queued>>p.frame>>p.visible>>p.root_visible>>p.x>>p.y>>p.playing>>p.clip>>p.time>>p.audio>>p.disabled>>p.prompt>>p.opened));return p;}
void pose(const KeyClockPose&actual,bool playing,uint32_t clip,float time,const Pose&expected){CHECK(actual.frame==expected.frame);CHECK(actual.visible==bool(expected.visible));CHECK(playing==bool(expected.playing));CHECK(clip==expected.clip);CHECK(std::abs(time-expected.time)<.000004);CHECK(std::abs(actual.position.x-expected.x)<.00003);CHECK(std::abs(actual.position.y-expected.y)<.00003);CHECK(actual.audio==bool(expected.audio));}
std::string joined(const std::vector<std::string>&v){std::string out;for(const auto&s:v){if(!out.empty())out+=',';out+=s;}return out.empty()?"-":out;}
}
int main(int argc,char**argv){
 CHECK(argc==3);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,128*1024,error));DungeonKeyClocksData data;CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(data.scene()==4&&data.metadata().node_count()==6&&data.profiles().size()==3&&data.clips().size()==7&&data.audios().size()==2);
 const auto before=data.profiles().data();auto bad=[&](std::vector<uint8_t>b){sign(b);CHECK(!data.load(b.data(),b.size(),error));CHECK(data.profiles().data()==before);};
 for(auto at:{8u,20u,24u,28u,32u,52u,56u,60u}){auto b=bytes;put(b,at,UINT32_MAX);bad(b);}
 for(unsigned s=0;s<7;++s)for(unsigned field=0;field<4;++field){auto b=bytes;put(b,64+s*16+field*4,UINT32_MAX);bad(b);}
 const auto profiles=get(bytes,64+2*16+4),clips=get(bytes,64+3*16+4),tracks=get(bytes,64+4*16+4),keys=get(bytes,64+5*16+4),audios=get(bytes,64+6*16+4);
 for(auto at:{profiles,profiles+4,profiles+8,profiles+12,profiles+16,profiles+20,profiles+28,profiles+44,profiles+48,profiles+56,profiles+60,clips,clips+4,clips+8,clips+12,clips+16,clips+20,clips+24,tracks,tracks+4,tracks+8,tracks+12,tracks+16,tracks+20,keys,keys+4,keys+8,keys+12,keys+20,audios,audios+4,audios+44,audios+48,audios+52,audios+56,audios+60,audios+64}){auto b=bytes;put(b,at,UINT32_MAX);bad(b);}
 for(size_t n=0;n<bytes.size();++n){CHECK(!data.load(bytes.data(),n,error));CHECK(data.profiles().data()==before);}CHECK(!data.load(nullptr,bytes.size(),error));
 {auto b=bytes;b.back()^=1;CHECK(!data.load(b.data(),b.size(),error));CHECK(data.profiles().data()==before);}
 CHECK(!data.load_file("build/absent-key-clock-fixture.encklc",error));CHECK(data.profiles().data()==before);
 {auto b=bytes;const auto nested=get(bytes,64+16+4);b[nested+32]^=1;bad(b);}
 {DungeonKeyClocksData shared=data;CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(shared.profiles().data()==before&&shared.metadata().valid());}
 {auto b=bytes;const float edge=.01f;uint32_t raw;std::memcpy(&raw,&edge,4);put(b,audios+48,raw);sign(b);DungeonKeyClocksData lower_bound;CHECK(lower_bound.load(b.data(),b.size(),error));CHECK(lower_bound.audios()[0].pitch==edge);}
 // Known callback/initial-property bindings are resource data, not fixed
 // defaults hidden in the C++ decoder. Current source generation remains exact.
 {auto b=bytes;put(b,profiles+16,get(b,profiles+16)|2);put(b,profiles+64+16,get(b,profiles+64+16)&~4u);put(b,profiles+64+56,0);put(b,profiles+128+56,0);sign(b);DungeonKeyClocksData bindings;CHECK(bindings.load(b.data(),b.size(),error));CHECK(bindings.profiles()[0].mask&2);CHECK(!(bindings.profiles()[1].mask&4));CHECK(bindings.profiles()[1].diagnostics==0&&bindings.profiles()[2].diagnostics==0);}
 std::ifstream input(argv[2]);CHECK(input.good());std::string line;unsigned old_case=UINT32_MAX,seen=0;DungeonKeyClocks world;DungeonKeyClockPlayer solo;uint64_t epoch=0;
 while(std::getline(input,line)){
  std::istringstream row(line);unsigned ci,index,op,profile,saved,role;uint32_t owner;double delta;int counter;unsigned flag;CHECK(bool(row>>ci>>index>>op>>profile>>owner>>saved>>role>>delta>>counter>>flag));const auto expected=read_pose(row),child=read_pose(row);std::string root_audio,child_audio,completed;unsigned missing,invalid;CHECK(bool(row>>root_audio>>child_audio>>completed>>missing>>invalid));
  if(ci!=old_case){CHECK(index==0);old_case=ci;++epoch;if(owner){CHECK(world.prepare(data,epoch,1,flags(data,saved?owner:0),error));world.clear_events();}else CHECK(solo.initialize(data,profile,error));}
  if(owner){
   if(op==1)CHECK(world.play_animation(epoch,owner,KeyClockRole(role),error));else if(op==2)CHECK(world.advance(epoch,delta,error));else if(op==3)CHECK(world.idle_frame(epoch,error));else if(op==4)CHECK(world.interact(epoch,owner,error));else if(op==5)CHECK(world.pickup(epoch,owner,true,false,error));else if(op==6)CHECK(world.audio_finished(epoch,owner,error));else CHECK(op==0);
   DungeonKeyClockView view;CHECK(world.state(epoch,owner,view,error));CHECK(view.object.alive==bool(expected.alive));CHECK(world.counter()==counter);bool value=false;CHECK(world.flag(epoch,owner,value,error)&&value==bool(flag));
   if(expected.alive){pose(view.root,view.root_playing,view.root_clip,view.root_time,expected);CHECK(view.object.queued==bool(expected.queued));CHECK(view.object.visible==bool(expected.root_visible));CHECK(view.object.collision_disabled==bool(expected.disabled));CHECK(view.object.prompt_enabled==bool(expected.prompt));CHECK(view.object.opened==bool(expected.opened));if(view.has_child){pose(view.child,view.child_playing,view.child_clip,view.child_time,child);CHECK(view.root.child_visible==bool(child.root_visible));}}
   std::vector<std::string>a,b,c;unsigned m=0,v=0;for(const auto&e:world.events())if(e.source==DungeonKeyClockEventSource::Clock){CHECK(e.epoch==epoch);const auto&k=e.clock;if(k.kind==KeyClockEventKind::Audio){(k.profile==profile?a:b).push_back(k.value?"1":"0");}else if(k.kind==KeyClockEventKind::Completed)c.push_back(std::to_string(k.profile)+":"+std::to_string(k.clip));else if(k.kind==KeyClockEventKind::MissingHandler)++m;else if(k.kind==KeyClockEventKind::InvalidSignal)++v;}
   if(joined(a)!=root_audio||joined(b)!=child_audio||joined(c)!=completed||m!=missing||v!=invalid)std::fprintf(stderr,"Native event mismatch case%u row%u audio%s/%s child%s/%s completed%s/%s diagnostic%u/%u,%u/%u\n",ci,index,joined(a).c_str(),root_audio.c_str(),joined(b).c_str(),child_audio.c_str(),joined(c).c_str(),completed.c_str(),m,missing,v,invalid);
   CHECK(joined(a)==root_audio&&joined(b)==child_audio&&joined(c)==completed&&m==missing&&v==invalid);world.clear_events();
  }else{
   if(op==1){const auto*p=data.profile(profile);CHECK(p&&role>=1&&role<=4&&solo.play(p->roles[role-1],error));}else if(op==2)CHECK(solo.advance(delta,error));else CHECK(op==3||op==0);
   pose(solo.pose(),solo.playing(),solo.assigned(),solo.position(),expected);std::vector<std::string>a,c;unsigned m=0,v=0;for(const auto&e:solo.events()){if(e.kind==KeyClockEventKind::Audio)a.push_back(e.value?"1":"0");else if(e.kind==KeyClockEventKind::Completed)c.push_back(std::to_string(e.profile)+":"+std::to_string(e.clip));else if(e.kind==KeyClockEventKind::MissingHandler)++m;else if(e.kind==KeyClockEventKind::InvalidSignal)++v;}CHECK(joined(a)==root_audio&&joined(c)==completed&&m==missing&&v==invalid);solo.clear_events();
  }++seen;
 }
 CHECK(input.eof()&&seen>400);
 const KeyClockProfile*door=nullptr,*key=nullptr,*child_profile=nullptr;for(const auto&p:data.profiles()){if(p.kind==KeyClockKind::Door)door=&p;else if(p.kind==KeyClockKind::Key)key=&p;else child_profile=&p;}CHECK(door&&key&&child_profile);uint32_t door_id=0,key_id=0;for(uint32_t i=0;i<data.metadata().node_count();++i){auto n=data.metadata().node(i);if(n.kind==DungeonKeyKind::Lock)door_id=n.id;else key_id=n.id;}
 CHECK(!world.prepare(data,epoch,1,flags(data),error));auto missing_flags=flags(data);missing_flags.pop_back();CHECK(!world.prepare(data,epoch+1,1,missing_flags,error));auto duplicate_flags=flags(data);duplicate_flags[1]=duplicate_flags[0];CHECK(!world.prepare(data,epoch+1,1,duplicate_flags,error));CHECK(world.prepare(data,++epoch,1,flags(data),error));CHECK(!world.advance(epoch-1,.1,error));CHECK(!world.advance(epoch,-1,error));CHECK(!world.advance(epoch,NAN,error));CHECK(!world.interact(epoch,0,error));CHECK(!world.pickup(epoch,door_id,true,false,error));CHECK(!world.interact(epoch,key_id,error));CHECK(!world.play_animation(epoch,key_id,KeyClockRole::Opened,error));CHECK(!world.play_animation(epoch,key_id,KeyClockRole(99),error));CHECK(!world.audio_finished(epoch,door_id,error));
 CHECK(world.pickup(epoch,key_id,false,false,error));CHECK(world.pickup(epoch,key_id,true,true,error));CHECK(world.counter()==1);CHECK(world.pickup(epoch,key_id,true,false,error));CHECK(world.counter()==2);CHECK(world.pickup(epoch,key_id,true,false,error));CHECK(world.counter()==2);CHECK(world.audio_finished(epoch,key_id,error));CHECK(world.idle_frame(epoch,error));CHECK(!world.audio_finished(epoch,key_id,error));
 CHECK(world.retire(epoch,error));CHECK(!world.interact(epoch,door_id,error));CHECK(!world.retire(epoch,error));CHECK(!world.prepare(data,epoch,1,flags(data),error));CHECK(world.prepare(data,++epoch,1,flags(data),error));world.clear_events();
 for(unsigned i=0;i<512;++i){CHECK(world.play_animation(epoch,door_id,KeyClockRole::Reset,error));CHECK(world.play_animation(epoch,door_id,KeyClockRole::Open,error));CHECK(world.advance(epoch,.0001,error));}CHECK(world.pending()==512);const auto event_count=world.events().size();CHECK(world.play_animation(epoch,door_id,KeyClockRole::Reset,error));CHECK(world.play_animation(epoch,door_id,KeyClockRole::Open,error));CHECK(!world.advance(epoch,.0001,error));CHECK(world.pending()==512&&world.events().size()==event_count);CHECK(world.idle_frame(epoch,error));CHECK(world.pending()==0);
 CHECK(solo.initialize(data,child_profile->id,error));CHECK(solo.play(child_profile->roles[0],error));unsigned filled=0;while(solo.advance(0,error)){CHECK(++filled<=1024);}CHECK(filled==512&&solo.events().size()==1024&&solo.position()==0);CHECK(!solo.advance(0,error));CHECK(solo.events().size()==1024);
 std::printf("Dungeon key clocks: %u checks; %u native observations, all seven clocks, continuous easing, source methods/deferred child order, diagnosed signals, lifecycle/atomic/parser negatives; no live map/audio/hardware claim\n",checks,seen);
}
