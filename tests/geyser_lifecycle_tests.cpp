#include "encore/geyser_lifecycle.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <cstdlib>
using namespace encore::upstream;
namespace {
size_t checks=0;std::string label;
void check(bool value,const char*message){++checks;if(!value){std::cerr<<label<<": "<<message<<"\n";std::exit(1);}}
uint32_t bits(float v){uint32_t b;std::memcpy(&b,&v,4);return b;}
float scalar(uint32_t b){float v;std::memcpy(&v,&b,4);return v;}
template<class T>T read(std::istream&in){T value{};check(bool(in>>value),"fixture truncated");return value;}
void equal(uint64_t actual,uint64_t expected,const char*message){check(actual==expected,message);}
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t value){for(unsigned n=0;n<4;++n)b[at+n]=uint8_t(value>>(8*n));}
void repair(std::vector<uint8_t>&b,size_t base=0,size_t length=0){if(!length)length=b.size()-base;put(b,base+16,encore::crc32(b.data()+base+32,length-32));}
void parser_negative(const std::vector<uint8_t>&bytes,const GeyserLifecycleData&data){
 label="family parser negative";std::string error;
 auto reject=[&](std::vector<uint8_t>bad){repair(bad);GeyserLifecycleData invalid;check(!invalid.load(bad.data(),bad.size(),error)&&!invalid.valid()&&!invalid.poison().valid()&&!invalid.geyser().valid()&&!error.empty(),"unknown/corrupt family fails without partial owners");};
 for(const auto&change:std::vector<std::pair<size_t,uint32_t>>{{8,0},{8,2},{12,uint32_t(bytes.size()+1)},{20,0},{20,2},{24,0},{24,data.poison().policy().scene+1},{28,1},{52,5},{56,1},{60,1},{64,2},{68,129},{72,0},{76,2},{80,1},{84,word(bytes,68)},{88,2},{92,31},{96,4},{100,UINT32_MAX},{104,262145},{108,2},{112,5},{116,word(bytes,100)},{120,65537},{124,2}}){auto bad=bytes;put(bad,change.first,change.second);reject(std::move(bad));}
 const auto pool=word(bytes,68),policy=word(bytes,84),poison=word(bytes,100),poison_size=word(bytes,104),geyser=word(bytes,116),geyser_size=word(bytes,120);
 for(size_t offset:{size_t(0),size_t(32),size_t(policy+24),size_t(policy+28)}){auto bad=bytes;bad[offset]^=1;reject(std::move(bad));}
 for(const auto&change:std::vector<std::pair<size_t,uint32_t>>{{policy,UINT32_MAX},{policy+4,0},{policy+4,513},{policy+8,word(bytes,policy)},{policy+12,word(bytes,policy+4)},{policy+20,0}}){auto bad=bytes;put(bad,change.first,change.second);reject(std::move(bad));}
 for(uint8_t invalid:{uint8_t('/'),uint8_t('\\'),uint8_t(':'),uint8_t(0),uint8_t(31),uint8_t(128)}){auto bad=bytes;bad[pool+word(bytes,policy+16)]=invalid;reject(std::move(bad));}
 // Keep nested resources independently valid, then prove the family refuses a
 // different source identity rather than accepting any well-formed sub-pack.
 {auto bad=bytes;const auto sub_pool=poison+word(bytes,poison+68),sub_policy=poison+word(bytes,poison+84);const auto at=sub_pool+word(bytes,sub_policy)+data.poison().policy().source.find_last_of('/')+1;bad[at]^=1;repair(bad,poison,poison_size);PoisonHazardsData valid;check(valid.load(bad.data()+poison,poison_size,error),"negative nested poison is valid on its own");reject(std::move(bad));}
 {auto bad=bytes;const auto sub_pool=geyser+word(bytes,geyser+68),sub_policy=geyser+word(bytes,geyser+84);const auto at=sub_pool+word(bytes,sub_policy)+data.geyser().policy().source.find_last_of('/')+1;bad[at]^=1;repair(bad,geyser,geyser_size);GeyserTimelineData valid;check(valid.load(bad.data()+geyser,geyser_size,error),"negative nested Geyser is valid on its own");reject(std::move(bad));}
 for(auto sub:{std::make_pair(poison,poison_size),std::make_pair(geyser,geyser_size)})for(size_t field:{size_t(8),size_t(20)}){auto bad=bytes;put(bad,sub.first+field,2);repair(bad,sub.first,sub.second);reject(std::move(bad));}
 {auto bad=bytes;bad.push_back(0);put(bad,12,uint32_t(bad.size()));reject(std::move(bad));}
 {auto bad=bytes;bad.back()^=1;GeyserLifecycleData invalid;check(!invalid.load(bad.data(),bad.size(),error),"checksum corruption");}
 GeyserLifecycleData empty;check(!empty.load(nullptr,bytes.size(),error)&&!empty.load_file(nullptr,error)&&!empty.load_file("",error),"null input/path");auto held=data;check(!held.load(bytes.data(),bytes.size(),error)&&held.same_owner(data),"second load preserves owner");
}
struct Context {bool player=false,tree=false;GeyserContext ui;};
void phase(std::istream&in,const GeyserLifecycleData&data,GeyserLifecycle&life,PoisonHoleTimers&timers,const Context&context){
 equal(context.player,read<unsigned>(in),"player pause");equal(context.tree,read<unsigned>(in),"tree pause");equal(context.ui.battle,read<unsigned>(in),"battle");equal(context.ui.cutscene,read<unsigned>(in),"cutscene");equal(context.ui.game_over,read<unsigned>(in),"game over");std::string error;
 for(const auto&hole:data.poison().holes()){PoisonHoleClock clock;check(timers.inspect(41,hole.id,clock,error),"clock inspect");equal(bits(float(clock.remaining)),read<uint32_t>(in),"Timer remaining");equal(bits(clock.wait),read<uint32_t>(in),"Timer wait");equal(clock.paused,read<unsigned>(in),"Timer paused");}
 const auto count=read<size_t>(in);const auto instances=life.instances();equal(instances.size(),count,"instance count");
 for(size_t n=0;n<count;++n){
  const auto ticket=read<uint64_t>(in),hole=read<uint64_t>(in);const auto x=read<uint32_t>(in),y=read<uint32_t>(in);const auto ready=read<unsigned>(in),time=read<uint32_t>(in),playing=read<unsigned>(in),frame=read<unsigned>(in),monitoring=read<unsigned>(in),disabled=read<unsigned>(in),active=read<unsigned>(in),paused=read<unsigned>(in),gain=read<uint32_t>(in),queued=read<unsigned>(in),on_screen=read<unsigned>(in);
  check(hole<data.poison().holes().size(),"fixture hole index");equal(instances[n],ticket,"ticket order");GeyserInstanceSnapshot snapshot;check(life.inspect(41,ticket,snapshot,error),"instance inspect");equal(snapshot.source.hole,data.poison().holes()[hole].id,"source hole");equal(bits(snapshot.source.position.x),x,"root x once");equal(bits(snapshot.source.position.y),y,"root y once");equal(bool(snapshot.pose),ready,"deferred Ready");equal(on_screen,0,"selected explicit offscreen context");
  if(snapshot.pose){const auto&pose=*snapshot.pose;equal(pose.clip,data.geyser().policy().spewing,"source clip");equal(bits(pose.time),time,"animation time");equal(pose.playing,playing,"playing");equal(pose.frame,frame,"frame");equal(pose.monitoring,monitoring,"monitoring");equal(pose.disabled,disabled,"disabled");equal(pose.active,active,"active");equal(pose.stream_paused,paused,"stream paused");equal(bits(pose.gain),gain,"gain");equal(pose.queued_free,queued,"queued lifetime");}
  else{equal(time,0,"pending has no elapsed animation");equal(playing,0,"pending not playing");equal(queued,0,"pending not queued for deletion");}
 }
 std::vector<GeyserInstanceCommand>commands;check(life.take_commands(41,commands,error),"drain transport");equal(commands.size(),read<size_t>(in),"command count");
 for(const auto&command:commands){equal(command.epoch,41,"command epoch");equal(command.instance,read<uint64_t>(in),"command instance");equal(unsigned(command.command.kind),read<unsigned>(in),"command kind");equal(command.command.sound,read<unsigned>(in),"command sound");equal(bits(command.command.offset),read<uint32_t>(in),"command offset");}
}
void original_profile(std::istream&in,const GeyserLifecycleData&data){
 const auto ticks=read<unsigned>(in),wanted_born=read<unsigned>(in),wanted_retired=read<unsigned>(in),animation_first=read<unsigned>(in);equal(ticks,720,"native coverage");check(animation_first<=1,"known source tree-order profile");std::string error;GeyserLifecycle life;PoisonHoleTimers timers;check(life.prepare(data,41,error),"prepare lifecycle");check(timers.prepare(data.poison(),41,error),"prepare source timers");Context context;bool first_gate=false,tree_gate=false;GeyserNamedVoice voice=GeyserNamedVoice::Absent;uint64_t born=0,retired=0;
 for(unsigned tick=0;tick<ticks;++tick){
  label="original tick "+std::to_string(tick);const auto delta=scalar(read<uint32_t>(in));
  // The controlled profile places Objects before the holes. The actual map's
  // selected tree order puts all hole Timers before Objects descendants.
  if(animation_first)check(life.advance_animations_idle(41,double(delta),!context.tree,error),"source early internal animation phase");
  for(const auto&hole:data.poison().holes()){
   check(timers.advance_hole_idle(41,hole.id,double(delta),!context.tree,error),"source Timer phase");
   if(!timers.requests().empty()){
    born+=timers.requests().size();check(life.receive_spawns(41,timers,error),"construct deferred instances");
    if(!first_gate){first_gate=true;context.player=true;check(life.player_signal(41,timers,true,error),"pause after construction before Ready");}
    else if(tick>=220&&!tree_gate){tree_gate=true;context.tree=true;check(life.tree_notification(41,true,false,error),"tree pause after construction");}
   }
  }
  if(!animation_first)check(life.advance_animations_idle(41,double(delta),!context.tree,error),"source map internal animation after Timers");
  check(life.process_ui(41,!context.tree,context.ui,error),"source script process after internal Timers");phase(in,data,life,timers,context);
  const auto actions=read<unsigned>(in);
  for(unsigned n=0;n<actions;++n){const auto op=read<unsigned>(in),value=read<unsigned>(in);
   if(op==1){check(value<=1,"pause operand");context.player=value;check(life.player_signal(41,timers,value,error),"observed player signal");}
   else if(op==2){check(value<=1,"tree operand");context.tree=value;check(life.tree_notification(41,value,false,error),"observed tree notification");}
   else if(op==3){check(value<=3,"UI operand");context.ui.battle=value==1;context.ui.cutscene=value==2;context.ui.game_over=value==3;}
   else check(false,"unknown fixture action");
  }
  check(life.flush_additions(41,error),"actual deferred Ready phase");std::vector<GeyserDeferredContext>contexts;for(auto id:life.instances())contexts.push_back({id,context.ui});check(life.flush_methods(41,contexts,voice,error),"source deferred methods");phase(in,data,life,timers,context);std::vector<PoisonSpawnRequest>removed;check(life.retire_queued(41,removed,error),"actual post-snapshot delete phase");retired+=removed.size();
  for(const auto&old:removed){GeyserInstanceSnapshot missing;check(!life.inspect(41,old.ticket,missing,error),"deleted ticket cannot resolve");}
 }
 equal(born,wanted_born,"actual native instance coverage");equal(retired,wanted_retired,"actual native deferred deletions");check(life.retire_scene(41,error),"scene cleanup");check(timers.retire(41,error),"Timer cleanup");check(!life.valid(),"retired scene invalid");
}
void original(const char*fixture,const GeyserLifecycleData&data){std::ifstream in(fixture);check(bool(in),"open native fixture");const auto profiles=read<unsigned>(in);equal(profiles,2,"both source tree-order profiles");for(unsigned n=0;n<profiles;++n)original_profile(in,data);std::string extra;check(!(in>>extra),"fixture trailing bytes");}
void runtime_negative(const GeyserLifecycleData&data,const std::vector<uint8_t>&bytes){
 label="lifecycle ownership/rollback";std::string error;GeyserLifecycle life;PoisonHoleTimers timers;check(!life.prepare(data,0,error),"zero epoch");check(life.prepare(data,41,error)&&timers.prepare(data.poison(),41,error),"negative case setup");check(!life.prepare(data,42,error),"cannot replace live owner");
 PoisonHoleTimers absent,stale,other;GeyserLifecycleData separate;check(separate.load(bytes.data(),bytes.size(),error),"independent same bytes");check(stale.prepare(data.poison(),42,error)&&other.prepare(separate.poison(),41,error),"wrong owner timers");for(auto*clock:{&absent,&stale,&other}){check(!life.receive_spawns(41,*clock,error)&&!life.player_signal(41,*clock,true,error)&&life.instances().empty(),"reject wrong Timer binding without mutation");}
 for(double delta:{-1.,-0.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),61.})check(!life.advance_animations_idle(41,delta,true,error)&&!life.advance_idle(41,delta,true,{},error),"bad animation delta even with no actors");
 check(!timers.advance_hole_idle(41,0,0,false,error)&&!timers.advance_hole_idle(42,data.poison().holes()[0].id,0,false,error)&&!timers.advance_hole_idle(41,data.poison().holes()[0].id,3601,true,error),"selected Timer invalid owner/epoch/delta");
 check(timers.advance_idle(41,4,true,error),"construct source wave");const auto count=timers.requests().size();check(count>1&&life.receive_spawns(41,timers,error)&&timers.requests().empty(),"receive source batch");const auto ids=life.instances();equal(ids.size(),count,"all source requests accepted");
 // An unready addition misses both notifications; Ready starts its source clip
 // without sampling those earlier states or advancing the clock.
 check(life.player_signal(41,timers,true,error)&&life.tree_notification(41,true,false,error)&&life.flush_additions(41,error),"signals before deferred birth");
 for(auto id:ids){GeyserInstanceSnapshot s;check(life.inspect(41,id,s,error)&&s.pose&&s.pose->active&&!s.pose->stream_paused&&s.pose->time==0,"Ready misses old pause notifications");}
 GeyserInstanceSnapshot untouched;untouched.source.ticket=999;check(!life.inspect(42,ids[0],untouched,error)&&untouched.source.ticket==999,"stale inspect output unchanged");check(!life.inspect(41,UINT64_MAX,untouched,error)&&untouched.source.ticket==999,"unknown inspect output unchanged");
 PoisonHoleTimers replay;check(replay.prepare(data.poison(),41,error)&&replay.advance_idle(41,4,true,error),"replayed source setup");const auto replay_count=replay.requests().size();check(!life.receive_spawns(41,replay,error)&&life.instances()==ids&&replay.requests().size()==replay_count,"source ticket replay rollback");
 GeyserNamedVoice voice=GeyserNamedVoice::Absent;std::vector<GeyserDeferredContext>contexts;for(auto id:ids){GeyserContext c;c.on_screen=true;contexts.push_back({id,c});}
 check(!life.flush_methods(41,{},voice,error)&&voice==GeyserNamedVoice::Absent,"missing contexts reject");auto bad_contexts=contexts;bad_contexts.back().instance=ids[0];check(!life.flush_methods(41,bad_contexts,voice,error)&&voice==GeyserNamedVoice::Absent,"duplicate/out-of-order contexts reject");auto invalid_voice=GeyserNamedVoice(0);check(!life.flush_methods(41,contexts,invalid_voice,error)&&unsigned(invalid_voice)==0,"unknown voice rejects");
 check(life.advance_animations_idle(41,data.geyser().clip(data.geyser().policy().spewing)->duration,true,error),"queue animation completion and deferred rumble");
 std::vector<PoisonSpawnRequest>removed;check(!life.retire_queued(41,removed,error)&&removed.empty()&&life.instances()==ids,"undelivered commands block delete");check(!life.retire_scene(41,error)&&life.valid(),"undelivered commands block scene teardown");
 std::vector<GeyserInstanceCommand>commands(1);check(!life.take_commands(41,commands,error)&&commands.size()==1,"nonempty output cannot consume commands");commands.clear();check(life.take_commands(41,commands,error)&&!commands.empty(),"drain original Audio2D phase");uint64_t last=0;for(auto&command:commands){check(command.instance>=last&&command.command.kind!=GeyserCommandKind::GlobalRumble,"audio commands retain source actor order before deferred methods");last=command.instance;}
 check(!life.retire_queued(41,removed,error)&&removed.empty()&&life.instances()==ids,"pending deferred method blocks delete after audio drain");check(!life.retire_scene(41,error)&&life.valid(),"pending deferred method blocks scene teardown");check(!life.advance_animations_idle(41,0,true,error),"completed actors require deletion before next idle");
 check(life.flush_methods(41,contexts,voice,error)&&voice==GeyserNamedVoice::Playing,"first visible actor starts shared named voice");commands.clear();check(life.take_commands(41,commands,error),"drain global phase");equal(commands.size(),1,"one shared global voice across all actors");equal(commands[0].instance,ids.front(),"source first deferred caller wins");check(commands[0].command.kind==GeyserCommandKind::GlobalRumble,"deferred command kind");
 check(life.retire_queued(41,removed,error)&&removed.size()==ids.size()&&life.instances().empty(),"delete only after commands/methods delivered");for(auto id:ids)check(!life.inspect(41,id,untouched,error),"deleted instance rejects late reference");check(life.retire_scene(41,error)&&voice==GeyserNamedVoice::Playing,"scene teardown preserves external named voice");check(!life.prepare(data,41,error)&&life.prepare(data,42,error),"scene epoch monotonic");check(!life.receive_spawns(42,timers,error),"old scene Timer rejects after new epoch");check(life.retire_scene(42,error),"new scene cleanup");
 // Exhaust pending admission without touching actual source queues on failure.
 GeyserLifecycle bounded;PoisonHoleTimers busy;check(bounded.prepare(data,51,error)&&busy.prepare(data.poison(),51,error),"bounded setup");bool rejected=false;
 for(unsigned n=0;n<10;++n){check(busy.advance_idle(51,4,true,error),"bounded source advance");auto before=bounded.instances();const auto waiting=busy.requests().size();if(!bounded.receive_spawns(51,busy,error)){check(bounded.instances()==before&&busy.requests().size()==waiting&&waiting>0,"capacity leaves both owners unchanged");rejected=true;break;}}
 check(rejected,"schema capacity exercised");check(bounded.retire_scene(51,error)&&busy.retire(51,error),"bounded cleanup");
}
}
int main(int argc,char**argv){check(argc==3,"family and native fixture arguments required");std::vector<uint8_t>bytes;std::string error;check(encore::read_file(argv[1],bytes,524288,error),"read family");GeyserLifecycleData data;check(data.load(bytes.data(),bytes.size(),error),"checked family load");check(data.valid()&&data.poison().valid()&&data.geyser().valid(),"three checked resources");
 label="parser truncation";for(size_t n=0;n<bytes.size();++n){GeyserLifecycleData invalid;check(!invalid.load(bytes.data(),n,error)&&!invalid.valid(),"all truncations fail closed");}
 parser_negative(bytes,data);original(argv[2],data);runtime_negative(data,bytes);std::cout<<"Geyser lifecycle checks: "<<checks<<" passed\n";return 0;}
