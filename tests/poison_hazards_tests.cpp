#include "encore/poison_hazards.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
using namespace encore;
namespace {unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Poison %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
uint32_t bits(float value){uint32_t raw;std::memcpy(&raw,&value,4);return raw;}
float number(uint32_t raw){float value;std::memcpy(&value,&raw,4);return value;}
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){CHECK(at<=b.size()&&4<=b.size()-at);for(unsigned n=0;n<4;++n)b[at+n]=uint8_t(v>>(8*n));}
void checksum(std::vector<uint8_t>&b){put(b,16,crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==5);std::vector<uint8_t>raw;CHECK(read_file(argv[1],raw,262144,error));upstream::PoisonHazardsData data;CHECK(data.load(raw.data(),raw.size(),error));CHECK(data.areas().size()==11&&data.holes().size()==37&&data.shapes().size()==22&&data.points().size()==178);CHECK(!data.load(raw.data(),raw.size(),error));
 upstream::FieldStepsData steps;CHECK(steps.load_file(argv[2],error)&&data.matches(steps.view(),error));upstream::FieldStepsData no_steps;CHECK(!data.matches(no_steps.view(),error));
 for(size_t n=0;n<raw.size();++n){upstream::PoisonHazardsData invalid;CHECK(!invalid.load(raw.data(),n,error)&&!invalid.valid());}upstream::PoisonHazardsData invalid;CHECK(!invalid.load(nullptr,raw.size(),error)&&!invalid.load_file(nullptr,error));
 auto reject=[&](size_t at,uint32_t value){auto changed=raw;put(changed,at,value);checksum(changed);upstream::PoisonHazardsData bad;CHECK(!bad.load(changed.data(),changed.size(),error)&&!bad.valid());};
 const auto policy=u(raw.data()+84),holes=u(raw.data()+100),areas=u(raw.data()+116),shapes=u(raw.data()+132),points=u(raw.data()+148);
 reject(8,2);reject(20,2);reject(24,0);reject(24,65536);reject(28,1);reject(32,1);reject(52,5);reject(56,1);reject(60,1);reject(64,2);reject(76,2);reject(68,164);reject(72,65537);reject(84,0);reject(92,63);reject(100,UINT32_MAX);reject(108,47);reject(116,160);reject(124,41);reject(140,31);reject(156,9);
 reject(policy,UINT32_MAX);reject(policy+8,u(raw.data()+policy));reject(policy+24,UINT32_MAX);reject(policy+28,1000001);reject(policy+40,0x7f800000);reject(policy+48,0x7fc00000);reject(policy+56,1);reject(policy+60,1);
 reject(holes,0);reject(holes,UINT32_MAX);reject(holes+48,u(raw.data()+holes));reject(holes+12,0x7fc00000);reject(holes+24,0x7ff00000);reject(holes+24,0);reject(holes+32,0xbff00000);reject(holes+28,1);reject(holes+32,0x7ff80000);reject(holes+36,1);reject(holes+40,0);reject(holes+44,1);
 reject(areas,0);reject(areas+40,u(raw.data()+areas));reject(areas+12,0);reject(areas+16,0x100000);reject(areas+24,4);reject(areas+28,1);reject(areas+32,1);reject(areas+36,1);
 reject(shapes,1);reject(shapes+4,2);reject(shapes+8,1);reject(shapes+12,2);reject(shapes+16,0x7fc00000);reject(shapes+24,u(raw.data()+areas+16)^1);reject(shapes+60,0x100000);reject(points,0x7f800000);
 {auto changed=raw;std::copy(changed.begin()+points,changed.begin()+points+8,changed.begin()+points+8);checksum(changed);upstream::PoisonHazardsData bad;CHECK(!bad.load(changed.data(),changed.size(),error));}
 {auto changed=raw;std::swap_ranges(changed.begin()+points+8,changed.begin()+points+16,changed.begin()+points+24);checksum(changed);upstream::PoisonHazardsData bad;CHECK(!bad.load(changed.data(),changed.size(),error));}
 {auto changed=raw;changed.back()^=1;upstream::PoisonHazardsData bad;CHECK(!bad.load(changed.data(),changed.size(),error));changed=raw;changed.push_back(0);CHECK(!bad.load(changed.data(),changed.size(),error));}
 // Native Timer states and real timeout order are authoritative. Observer
 // actions happen after the current internal processing pass.
 std::ifstream clock_input(argv[3]);CHECK(clock_input.good());unsigned count;CHECK(bool(clock_input>>count)&&count==data.holes().size());upstream::PoisonHoleTimers timers;CHECK(!timers.prepare({},1,error)&&!timers.prepare(data,0,error));CHECK(timers.prepare(data,1,error)&&!timers.prepare(data,2,error));
 auto clock_equal=[&](size_t index){uint32_t time_bits,wait_bits;int paused;CHECK(bool(clock_input>>time_bits>>wait_bits>>paused));upstream::PoisonHoleClock actual;CHECK(timers.inspect(1,data.holes()[index].id,actual,error));CHECK(bits(float(std::max(0.,actual.remaining)))==time_bits);CHECK(bits(actual.wait)==wait_bits&&actual.paused==bool(paused));};
 for(size_t n=0;n<count;++n)clock_equal(n);unsigned frames;CHECK(bool(clock_input>>frames)&&frames==720);uint64_t ticket=0;std::vector<upstream::PoisonSpawnRequest>all;
 for(unsigned frame=0;frame<frames;++frame){unsigned index,delta_bits,tree_paused,actions,timeouts;CHECK(bool(clock_input>>index>>delta_bits>>tree_paused>>actions>>timeouts)&&index==frame);CHECK(timers.advance_idle(1,double(number(delta_bits)),!tree_paused,error));
  for(unsigned n=0;n<actions;++n){unsigned kind,owner,value;CHECK(bool(clock_input>>kind>>owner>>value));switch(kind){case 1:for(const auto&hole:data.holes())CHECK(timers.pause(1,hole.id,value!=0,error));break;case 2:CHECK(owner<data.holes().size()&&timers.pause(1,data.holes()[owner].id,value!=0,error));break;case 3:case 4:CHECK(value<=1);break;default:CHECK(false);}}
  for(size_t n=0;n<count;++n)clock_equal(n);const auto emitted=timers.take_requests();CHECK(emitted.size()==timeouts);
  for(const auto&request:emitted){unsigned source;CHECK(bool(clock_input>>source)&&source<data.holes().size());CHECK(request.epoch==1&&request.ticket==++ticket&&request.hole==data.holes()[source].id);all.push_back(request);}
 }
 unsigned spawned;CHECK(bool(clock_input>>spawned)&&spawned==all.size());for(const auto&request:all){uint32_t x,y;CHECK(bool(clock_input>>x>>y)&&bits(request.position.x)==x&&bits(request.position.y)==y);}std::string trailing;CHECK(!(clock_input>>trailing));
 upstream::PoisonHoleClock unchanged,after;CHECK(timers.inspect(1,data.holes()[0].id,unchanged,error));CHECK(!timers.advance_idle(2,1,true,error)&&!timers.advance_idle(1,-1,true,error)&&!timers.advance_idle(1,std::numeric_limits<double>::infinity(),true,error)&&!timers.advance_idle(1,3601,true,error));CHECK(!timers.pause(2,data.holes()[0].id,true,error)&&!timers.pause(1,0,true,error)&&!timers.inspect(1,0,after,error));CHECK(timers.inspect(1,data.holes()[0].id,after,error)&&unchanged.remaining==after.remaining&&unchanged.wait==after.wait&&unchanged.paused==after.paused);CHECK(!timers.retire(2,error)&&timers.retire(1,error)&&!timers.valid()&&!timers.advance_idle(1,0,true,error)&&!timers.prepare(data,1,error)&&timers.prepare(data,2,error));
 // A very long frame still generates at most one native timeout per Timer.
 CHECK(timers.advance_idle(2,100,true,error));CHECK(timers.requests().size()==data.holes().size());const auto before=timers.requests().size();CHECK(timers.advance_idle(2,0,false,error)&&timers.requests().size()==before);CHECK(timers.advance_idle(2,0,true,error)&&timers.requests().size()==2*before);timers.take_requests();
 // Exhausting the source request queue rejects the whole Timer batch.
 bool rejected=false;for(unsigned n=0;n<200;++n){const auto size=timers.requests().size();upstream::PoisonHoleClock old_clock,new_clock;CHECK(timers.inspect(2,data.holes()[0].id,old_clock,error));if(!timers.advance_idle(2,100,true,error)){CHECK(timers.requests().size()==size&&timers.inspect(2,data.holes()[0].id,new_clock,error)&&old_clock.remaining==new_clock.remaining&&old_clock.wait==new_clock.wait);rejected=true;break;}}CHECK(rejected);
 {auto changed=raw;put(changed,areas+12,42);checksum(changed);upstream::PoisonHazardsData wrong_steps;CHECK(wrong_steps.load(changed.data(),changed.size(),error)&&!wrong_steps.matches(steps.view(),error));}
 // Native original area/geyser callbacks: pause gates ENTER only,while EXIT
 // neither clears remembered attack values nor removes the poisoned request.
 std::ifstream contacts(argv[4]);CHECK(contacts.good());unsigned rows;CHECK(bool(contacts>>rows)&&rows==99);upstream::FieldRunLoop run;const auto v=steps.view();CHECK(run.initialize(v,v.text(v.sound(v.policy().default_sound).name),error));upstream::FieldStepShadowState shadows[2];for(auto&shadow:shadows)CHECK(shadow.initialize(v,error));upstream::PoisonDamageState damage[2];
 for(unsigned n=0;n<rows;++n){unsigned kind,area,body,entered,paused,wanted;CHECK(bool(contacts>>kind>>area>>body>>entered>>paused>>wanted)&&kind<=1&&body<=2&&area<data.areas().size());const auto classification=body==0?upstream::FieldStepBody::Player:body==1?upstream::FieldStepBody::Party:upstream::FieldStepBody::Other;upstream::PoisonDamageState ignored;
  CHECK(upstream::apply_poison_callback(data,kind?upstream::PoisonContactKind::Geyser:upstream::PoisonContactKind::Area,kind?0:data.areas()[area].id,entered!=0,classification,paused!=0,body<2?damage[body]:ignored,run,body<2?&shadows[body]:nullptr,error));CHECK(run.wanted_sound()==wanted);
  for(unsigned b=0;b<2;++b){unsigned continuous,attack,variance,statuses,mode;CHECK(bool(contacts>>continuous>>attack>>variance>>statuses>>mode));CHECK(damage[b].continuous==bool(continuous)&&uint32_t(damage[b].damage)==attack&&uint32_t(damage[b].variance)==variance&&damage[b].status_requests.size()==statuses&&shadows[b].animation()==mode);for(const auto&status:damage[b].status_requests)CHECK(status==data.policy().status);}
 }CHECK(!(contacts>>trailing));
 auto call=[&](const upstream::PoisonHazardsData&content,upstream::PoisonContactKind kind,uint32_t owner,upstream::FieldStepBody body,upstream::FieldStepShadowState*shadow){return upstream::apply_poison_callback(content,kind,owner,true,body,false,damage[0],run,shadow,error);};
 const auto original=damage[0];const auto run_sound=run.wanted_sound();const auto shadow_frame=shadows[0].frame();CHECK(!call({},upstream::PoisonContactKind::Area,data.areas()[0].id,upstream::FieldStepBody::Player,&shadows[0]));CHECK(!call(data,upstream::PoisonContactKind(0),0,upstream::FieldStepBody::Player,&shadows[0]));CHECK(!call(data,upstream::PoisonContactKind::Area,data.areas()[0].id,upstream::FieldStepBody(0),&shadows[0]));CHECK(!call(data,upstream::PoisonContactKind::Area,0,upstream::FieldStepBody::Player,&shadows[0]));CHECK(!call(data,upstream::PoisonContactKind::Geyser,1,upstream::FieldStepBody::Player,&shadows[0]));CHECK(!call(data,upstream::PoisonContactKind::Area,data.areas()[0].id,upstream::FieldStepBody::Player,nullptr));CHECK(damage[0].continuous==original.continuous&&damage[0].damage==original.damage&&damage[0].variance==original.variance&&damage[0].status_requests==original.status_requests&&run.wanted_sound()==run_sound&&shadows[0].frame()==shadow_frame);
 upstream::FieldStepsData duplicate;CHECK(duplicate.load_file(argv[2],error));upstream::FieldStepShadowState wrong;CHECK(wrong.initialize(duplicate.view(),error));CHECK(!call(data,upstream::PoisonContactKind::Area,data.areas()[0].id,upstream::FieldStepBody::Player,&wrong));damage[0].status_requests.assign(256,data.policy().status);CHECK(!call(data,upstream::PoisonContactKind::Geyser,0,upstream::FieldStepBody::Player,nullptr)&&damage[0].status_requests.size()==256);
 std::printf("Poison hazards: %u checks;actual native720 Timer frames/99 callbacks/102 deferred positions,source polygons and negative format/callback paths;not full scene,HP,physics,animation/audio or hardware\n",checks);
}
