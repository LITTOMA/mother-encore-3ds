#include "encore/geyser_mixer.hpp"
#include "encore/geyser_graphics.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error,label;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"Instance presentation "<<__LINE__<<" "<<label<<": "<<#x<<" ("<<error<<")\n";std::exit(1);}}while(0)
template<class T>T read(std::istream&in){T value{};in>>value;CHECK(bool(in));return value;}
float real(uint32_t bits){float value;std::memcpy(&value,&bits,4);return value;}
uint32_t bits(float value){uint32_t result;std::memcpy(&result,&value,4);return result;}
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t value){for(unsigned n=0;n<4;++n)b[at+n]=uint8_t(value>>(8*n));}
struct Expected {uint32_t sound=0,cursor=0;bool playing=false;};
struct Replay {
 const GeyserLifecycleData&family;const GeyserMediaData&media;const GeyserGraphicsData&graphics;const std::vector<GeyserPcmView>&pcm;
 GeyserLifecycle life;PoisonHoleTimers timers;GeyserMixer mixer;GeyserContext context;bool player=false,tree=false;uint64_t plays=0;std::map<uint64_t,Expected>expected;
 explicit Replay(const GeyserLifecycleData&f,const GeyserMediaData&m,const GeyserGraphicsData&g,const std::vector<GeyserPcmView>&p):family(f),media(m),graphics(g),pcm(p){CHECK(life.prepare(f,71,error)&&timers.prepare(f.poison(),71,error)&&mixer.prepare(m,p,error)&&mixer.activate(71,error));}
 void phase(std::istream&in){
  CHECK(player==bool(read<unsigned>(in))&&tree==bool(read<unsigned>(in)));CHECK(context.battle==bool(read<unsigned>(in))&&context.cutscene==bool(read<unsigned>(in))&&context.game_over==bool(read<unsigned>(in)));
  for(size_t n=0;n<family.poison().holes().size();++n){read<uint32_t>(in);read<uint32_t>(in);read<unsigned>(in);}
  const auto count=read<size_t>(in);CHECK(count==life.instances().size());std::map<uint64_t,GeyserInstanceSnapshot>actors;
  for(size_t n=0;n<count;++n){
   const auto id=read<uint64_t>(in),hole=read<uint64_t>(in);const auto x=read<uint32_t>(in),y=read<uint32_t>(in);const auto ready=read<unsigned>(in),time=read<uint32_t>(in),playing=read<unsigned>(in),frame=read<unsigned>(in),monitoring=read<unsigned>(in),disabled=read<unsigned>(in),active=read<unsigned>(in),paused=read<unsigned>(in),gain=read<uint32_t>(in),queued=read<unsigned>(in),visible=read<unsigned>(in);
   GeyserInstanceSnapshot snapshot;CHECK(life.inspect(71,id,snapshot,error));CHECK(hole<family.poison().holes().size()&&snapshot.source.hole==family.poison().holes()[hole].id&&bits(snapshot.source.position.x)==x&&bits(snapshot.source.position.y)==y&&bool(snapshot.pose)==bool(ready));
   if(ready){const auto&p=*snapshot.pose;CHECK(bits(p.time)==time&&p.playing==bool(playing)&&p.frame==frame&&p.monitoring==bool(monitoring)&&p.disabled==bool(disabled)&&p.active==bool(active)&&p.stream_paused==bool(paused)&&bits(p.gain)==gain&&p.queued_free==bool(queued));expected.try_emplace(id);
    GeyserSpriteDraw draw;CHECK(graphics.compose_lifecycle(life,71,id,{0,0},bool(visible),draw,error)&&draw.source.index==frame&&draw.visible==bool(visible));
    GeyserTimeline independent;GeyserSpriteDraw baseline;CHECK(independent.prepare(graphics.timeline(),71,id,error)&&independent.manual_advance(71,id,p.time,error));CHECK(graphics.compose(independent,71,id,snapshot.source.position,{0,0},bool(visible),baseline,error));CHECK(draw.position.x==baseline.position.x&&draw.position.y==baseline.position.y&&draw.source.index==baseline.source.index);
   }else{GeyserSpriteDraw untouched;untouched.position={17,19};CHECK(!graphics.compose_lifecycle(life,71,id,{0,0},true,untouched,error)&&untouched.position.x==17&&untouched.position.y==19);}
   actors.emplace(id,std::move(snapshot));
  }
  const auto command_count=read<size_t>(in);CHECK(command_count==life.commands().size());
  for(size_t n=0;n<command_count;++n){const auto id=read<uint64_t>(in);const auto kind=read<unsigned>(in),sound=read<uint32_t>(in),offset=read<uint32_t>(in);const auto&actual=life.commands()[n];CHECK(actual.epoch==71&&actual.instance==id&&unsigned(actual.command.kind)==kind&&actual.command.sound==sound&&bits(actual.command.offset)==offset);auto&state=expected.at(id);
   if(kind==1){state.sound=sound;state.cursor=0;state.playing=false;}
   else if(kind==2){AudioAsset asset;CHECK(media.bank().find(sound,asset));state.sound=sound;state.cursor=uint32_t(double(real(offset))*asset.sample_rate);state.playing=true;++plays;}
   else if(kind==3)state.playing=false;
   else CHECK(false); // Selected frozen profile is explicitly offscreen.
  }
  CHECK(mixer.sync_lifecycle(71,life,{{0,0},320,1},error)&&life.commands().empty()&&mixer.play_requests()==plays);
  for(const auto&entry:actors){GeyserPlayback actual;const auto&id=entry.first;const auto&snapshot=entry.second;
   if(!snapshot.pose){CHECK(!mixer.inspect(71,id,actual,error));continue;}
   const auto&state=expected.at(id);CHECK(mixer.inspect(71,id,actual,error)&&actual.present&&actual.paused==snapshot.pose->stream_paused&&actual.sound==state.sound&&actual.cursor==state.cursor&&actual.playing==state.playing);
  }
 }
 void run(std::istream&in){
  const auto count=read<unsigned>(in),born=read<unsigned>(in),retired=read<unsigned>(in),early=read<unsigned>(in);CHECK(count==720&&early<=1);uint64_t allocated=0,deleted=0;bool first=false,second=false;
  for(unsigned tick=0;tick<count;++tick){label="profile "+std::to_string(early)+" tick "+std::to_string(tick);const auto delta=real(read<uint32_t>(in));if(early)CHECK(life.advance_animations_idle(71,delta,!tree,error));
   for(const auto&hole:family.poison().holes()){CHECK(timers.advance_hole_idle(71,hole.id,delta,!tree,error));if(!timers.requests().empty()){allocated+=timers.requests().size();CHECK(life.receive_spawns(71,timers,error));if(!first){first=true;player=true;CHECK(life.player_signal(71,timers,true,error));}else if(tick>=220&&!second){second=true;tree=true;CHECK(life.tree_notification(71,true,false,error));}}}
   if(!early)CHECK(life.advance_animations_idle(71,delta,!tree,error));CHECK(life.process_ui(71,!tree,context,error));phase(in);const auto actions=read<unsigned>(in);
   for(unsigned n=0;n<actions;++n){const auto op=read<unsigned>(in),value=read<unsigned>(in);if(op==1){player=value;CHECK(life.player_signal(71,timers,player,error));}else if(op==2){tree=value;CHECK(life.tree_notification(71,tree,false,error));}else if(op==3){context.battle=value==1;context.cutscene=value==2;context.game_over=value==3;}else CHECK(false);}
   CHECK(life.flush_additions(71,error));std::vector<GeyserDeferredContext>contexts;for(auto id:life.instances())contexts.push_back({id,context});auto voice=mixer.global_voice();CHECK(life.flush_methods(71,contexts,voice,error));phase(in);std::vector<PoisonSpawnRequest>removed;CHECK(life.retire_queued(71,removed,error));deleted+=removed.size();CHECK(mixer.sync_lifecycle(71,life,{{0,0},320,1},error));for(const auto&old:removed){expected.erase(old.ticket);GeyserPlayback gone;CHECK(!mixer.inspect(71,old.ticket,gone,error));}
  }
  CHECK(allocated==born&&deleted==retired&&life.retire_scene(71,error)&&timers.retire(71,error)&&mixer.retire_scene(71,error));
 }
};
void negative(const GeyserLifecycleData&family,const GeyserLifecycleData&other,const GeyserLifecycleData&altered,const GeyserMediaData&media,const GeyserGraphicsData&graphics,const std::vector<GeyserPcmView>&pcm){
 label="negative ownership/rollback";GeyserMixer mixer;GeyserLifecycle life;PoisonHoleTimers timers;CHECK(mixer.prepare(media,pcm,error)&&mixer.activate(91,error)&&life.prepare(family,91,error)&&timers.prepare(family.poison(),91,error));GeyserLifecycle absent;CHECK(!mixer.sync_lifecycle(91,absent,{{0,0},320,1},error)&&!mixer.sync_lifecycle(92,life,{{0,0},320,1},error));
 CHECK(timers.advance_hole_idle(91,family.poison().holes().front().id,family.poison().holes().front().delay+family.poison().holes().front().interval+1,true,error)&&life.receive_spawns(91,timers,error));CHECK(!life.instances().empty());const auto id=life.instances().front();GeyserSpriteDraw draw;draw.position={11,13};CHECK(!graphics.compose_lifecycle(life,91,id,{0,0},true,draw,error)&&draw.position.x==11&&draw.position.y==13);CHECK(life.flush_additions(91,error)&&life.advance_animations_idle(91,.1,true,error)&&!life.commands().empty());const auto queued=life.commands().size();
 for(auto listener:{GeyserListener{{0,0},0,1},GeyserListener{{NAN,0},320,1},GeyserListener{{0,0},320,INFINITY}})CHECK(!mixer.sync_lifecycle(91,life,listener,error)&&life.commands().size()==queued&&mixer.play_requests()==0);
 auto&transport=const_cast<std::vector<GeyserInstanceCommand>&>(life.commands());const auto last=transport.back();for(unsigned bad:{0u,255u}){transport.back().command.kind=GeyserCommandKind(bad);CHECK(!mixer.sync_lifecycle(91,life,{{0,0},320,1},error)&&mixer.play_requests()==0&&life.commands().size()==queued);transport.back()=last;}
 transport.back().instance=UINT64_MAX;CHECK(!mixer.sync_lifecycle(91,life,{{0,0},320,1},error)&&mixer.play_requests()==0);transport.back()=last;transport.back().epoch=92;CHECK(!mixer.sync_lifecycle(91,life,{{0,0},320,1},error)&&mixer.play_requests()==0);transport.back()=last;
 {auto saved=transport;transport.resize(32769);CHECK(!mixer.sync_lifecycle(91,life,{{0,0},320,1},error)&&mixer.play_requests()==0);transport=std::move(saved);}
 std::array<int16_t,1024>out{};uint32_t frames=99;CHECK(mixer.reserve_render(out.data(),512,frames,error));CHECK(!mixer.sync_lifecycle(91,life,{{0,0},320,1},error)&&life.commands().size()==queued);mixer.cancel_render();CHECK(mixer.sync_lifecycle(91,life,{{0,0},320,1},error)&&life.commands().empty()&&mixer.play_requests()==1);CHECK(!mixer.bind(91,id+1,error)&&!mixer.retire_actor(91,id,error));
 GeyserTimeline legacy;CHECK(legacy.prepare(media.timeline(),91,id,error));CHECK(!mixer.sync(91,id,legacy,{},{{0,0},320,1},error));GeyserMixer mixed;CHECK(mixed.prepare(media,pcm,error)&&mixed.activate(91,error)&&mixed.bind(91,1,error)&&mixed.retire_actor(91,1,error));CHECK(!mixed.sync_lifecycle(91,life,{{0,0},320,1},error));
 GeyserLifecycle foreign,changed;PoisonHoleTimers changed_timers;CHECK(foreign.prepare(other,91,error)&&!mixer.sync_lifecycle(91,foreign,{{0,0},320,1},error));CHECK(changed.prepare(altered,91,error)&&changed_timers.prepare(altered.poison(),91,error));const auto&hole=altered.poison().holes().front();CHECK(changed_timers.advance_hole_idle(91,hole.id,hole.delay+hole.interval+1,true,error)&&changed.receive_spawns(91,changed_timers,error)&&changed.flush_additions(91,error));GeyserMixer clean;CHECK(clean.prepare(media,pcm,error)&&clean.activate(91,error)&&!clean.sync_lifecycle(91,changed,{{0,0},320,1},error));CHECK(!graphics.compose_lifecycle(changed,91,1,{},true,draw,error));
 CHECK(!graphics.compose_lifecycle(life,92,id,{},true,draw,error)&&!graphics.compose_lifecycle(life,91,UINT64_MAX,{},true,draw,error)&&!graphics.compose_lifecycle(life,91,id,{NAN,0},true,draw,error));
 CHECK(life.player_signal(91,timers,true,error)&&mixer.sync_lifecycle(91,life,{{0,0},320,1},error));GeyserPlayback state;CHECK(mixer.inspect(91,id,state,error)&&state.paused);CHECK(life.tree_notification(91,false,false,error)&&mixer.sync_lifecycle(91,life,{{0,0},320,1},error)&&mixer.inspect(91,id,state,error)&&!state.paused);
 CHECK(life.player_signal(91,timers,false,error)&&life.advance_animations_idle(91,3,true,error));std::vector<GeyserDeferredContext>contexts{{id,{true,false,false,false}}};auto voice=mixer.global_voice();CHECK(life.flush_methods(91,contexts,voice,error)&&voice==GeyserNamedVoice::Playing);CHECK(mixer.sync_lifecycle(91,life,{{0,0},320,1},error)&&mixer.global_voice()==GeyserNamedVoice::Playing);std::vector<PoisonSpawnRequest>removed;CHECK(life.retire_queued(91,removed,error)&&removed.size()==1&&mixer.sync_lifecycle(91,life,{{0,0},320,1},error)&&!mixer.inspect(91,id,state,error));CHECK(life.retire_scene(91,error)&&mixer.retire_scene(91,error)&&mixer.global_voice()==GeyserNamedVoice::Playing);CHECK(!graphics.compose_lifecycle(life,91,id,{},true,draw,error)&&!mixer.sync_lifecycle(91,life,{{0,0},320,1},error));
 CHECK(mixer.activate(92,error)&&life.prepare(family,92,error)&&mixer.sync_lifecycle(92,life,{{0,0},320,1},error)&&life.retire_scene(92,error)&&mixer.retire_scene(92,error));
}
}
int main(int argc,char**argv){CHECK(argc==6);GeyserLifecycleData family,other,altered;GeyserMediaData media;GeyserGraphicsData graphics;CHECK(family.load_file(argv[1],error)&&other.load_file(argv[1],error)&&media.load_file(argv[2],error)&&graphics.load_file(argv[4],error));CHECK(media.timeline().same_content(family.geyser())&&graphics.matches(family.geyser())&&!media.timeline().same_owner(family.geyser()));
 std::vector<uint8_t>raw;CHECK(encore::read_file(argv[1],raw,524288,error));const auto nested=word(raw,116),size=word(raw,120),policy=word(raw,nested+84);put(raw,nested+policy+80,bits(real(word(raw,nested+policy+80))+1));put(raw,nested+16,encore::crc32(raw.data()+nested+32,size-32));put(raw,16,encore::crc32(raw.data()+32,raw.size()-32));CHECK(altered.load(raw.data(),raw.size(),error)&&!altered.geyser().same_content(family.geyser()));
 std::vector<std::vector<int16_t>>samples(media.bank().count());std::vector<GeyserPcmView>pcm;
 for(uint32_t n=0;n<media.bank().count();++n){const auto asset=media.bank().asset(n);std::vector<uint8_t>raw;CHECK(encore::read_file((std::string(argv[3])+"/"+std::string(asset.pcm_path)).c_str(),raw,1024*1024,error)&&raw.size()==asset.pcm_bytes);auto&out=samples[n];out.resize(raw.size()/2);for(size_t i=0;i<out.size();++i){const uint16_t value=uint16_t(raw[2*i])|uint16_t(raw[2*i+1])<<8;std::memcpy(&out[i],&value,2);}pcm.push_back({asset.stable_id,out.data(),out.size()});}
 std::ifstream native(argv[5]);CHECK(bool(native)&&read<unsigned>(native)==2);for(unsigned n=0;n<2;++n){Replay replay(family,media,graphics,pcm);replay.run(native);}std::string extra;CHECK(!(native>>extra));negative(family,other,altered,media,graphics,pcm);std::cout<<"Geyser instance presentation: "<<checks<<" checks passed;two frozen720-frame source profiles,real cached PCM,CPU projection;not GPU/NDSP/audible/full room/hardware\n";
}
