#include "encore/field_objects.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Object lifecycle %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t x){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(x>>(i*8));}
void fix(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==4);FieldObjectsData data;RoomData room;CHECK(data.load_file(argv[1],error)&&room.load_file(argv[2],error));
 FieldObjects objects;SourceRandom random(9);const auto seed_state=random.state();
 CHECK(!objects.enter_ready(1,false,random,error)&&random.state()==seed_state);
 CHECK(!objects.idle_frame(error)&&!objects.advance_idle(0,error)&&!objects.flush_queued(error));
 CHECK(!objects.queue_free("Objects/Items/Item",error)&&objects.state(1)==nullptr);
 CHECK(objects.prepare(data,room.view(),error)&&random.state()==seed_state);
 CHECK(!objects.can_queue_free("Objects/Items/Item",error)&&!objects.enter_ready(0,false,random,error)&&random.state()==seed_state);
 CHECK(objects.state(1)&&!objects.state(1)->ready&&!objects.state(1)->visible&&objects.state(1)->alive);
 std::ifstream file(argv[3]);CHECK(bool(file));std::string line;CHECK(bool(std::getline(file,line)));unsigned rows=0;uint32_t tick=0;
 while(std::getline(file,line)){
  std::istringstream in(line);std::string phase;uint32_t number,flag,alive,visible,queued,next;int frame;
  CHECK(bool(in>>phase>>number>>flag>>alive>>visible>>queued>>frame>>next));++rows;
  if(phase=="ready"){
   SourceRandom sample(number);CHECK(objects.prepare(data,room.view(),error));CHECK(objects.enter_ready(1,flag!=0,sample,error));CHECK(sample.randi()==next);
  }else if(phase=="initial"||phase=="suppressed-ready"){
   random.seed(9);CHECK(objects.prepare(data,room.view(),error));CHECK(objects.enter_ready(1,phase=="suppressed-ready",random,error));tick=0;
  }else if(phase=="clock"){
   CHECK(number==tick+1);if(tick)CHECK(objects.advance_idle(1.0/60,error));CHECK(objects.idle_frame(error));tick=number;
   // The source fixture sets got_shovel at tick3. No ongoing flag poll exists:
   // only an explicit queue_free later removes this live object.
  }else if(phase=="queue"){
   CHECK(objects.can_queue_free("Objects/Items/Item",error)&&objects.queue_free("Objects/Items/Item",error));
   CHECK(objects.queue_free("Objects/Items/Item",error));
  }else if(phase=="deleted"||phase=="suppressed-deleted")CHECK(objects.flush_queued(error));
  else CHECK(false);
  const auto*s=objects.state(1);CHECK(s&&s->ready&&s->alive==bool(alive)&&s->visible==bool(visible)&&s->queued==bool(queued));
  if(alive)CHECK(s->frame==uint32_t(frame));
 }
 CHECK(rows==499);CHECK(!objects.can_queue_free("Objects/Items/Item",error)&&!objects.queue_free("Objects/Items/Item",error));
 CHECK(objects.prepare(data,room.view(),error));random.seed(9);CHECK(objects.enter_ready(1,false,random,error));const auto after_ready=random.state();
 CHECK(!objects.enter_ready(1,false,random,error)&&random.state()==after_ready);
 CHECK(!objects.enter_ready(2,false,random,error)&&random.state()==after_ready);
 CHECK(objects.advance_idle(0,error)&&!objects.state(1)->visible&&objects.state(1)->show_pending);
 for(double dt:{-1.,1.01,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
  const auto before=*objects.state(1);CHECK(!objects.advance_idle(dt,error));const auto after=*objects.state(1);CHECK(before.frame==after.frame&&before.timeout==after.timeout&&before.visible==after.visible&&before.show_pending==after.show_pending);
 }
 CHECK(!objects.queue_free("",error)&&!objects.queue_free("../Objects/Items/Item",error)&&!objects.queue_free("Objects/Items/item",error));
 CHECK(objects.advance_idle(.75,error)&&!objects.state(1)->visible&&objects.state(1)->frame!=39); // hidden Sprite still processes
 CHECK(objects.idle_frame(error)&&objects.state(1)->visible);
 FieldObjectsData invalid;RoomData absent;CHECK(!objects.prepare(invalid,room.view(),error)&&objects.state(1)->visible);CHECK(!objects.prepare(data,absent.view(),error)&&objects.state(1)->visible);
 std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,1024*1024,error));const size_t at=word(bytes,100);FieldObjectsData fixed;
 // Legal non-random/paused/non-loop data use the same checked executor.
 auto b=bytes;put(b,at+8,255^uint32_t(FieldObjectFlag::RandomFrame)^uint32_t(FieldObjectFlag::Playing)^uint32_t(FieldObjectFlag::Loop));put(b,at+76,0);put(b,at+80,0);fix(b);
 CHECK(fixed.load(b.data(),b.size(),error)&&objects.prepare(fixed,room.view(),error));random.seed(1);const auto untouched=random.state();CHECK(objects.enter_ready(1,false,random,error)&&random.state()==untouched&&objects.state(1)->frame==22);CHECK(objects.advance_idle(1,error)&&objects.state(1)->frame==22);
 b=bytes;put(b,at+8,255^uint32_t(FieldObjectFlag::RandomFrame)^uint32_t(FieldObjectFlag::Loop));put(b,at+76,0);put(b,at+80,0);put(b,at+24,39);fix(b);
 CHECK(fixed.load(b.data(),b.size(),error)&&objects.prepare(fixed,room.view(),error)&&objects.enter_ready(1,false,random,error));CHECK(objects.advance_idle(1,error)&&objects.state(1)->frame==39);
 // Flag and scene domains are checked before replacing an existing owner.
 b=bytes;put(b,24,data.scene_id()+1);fix(b);CHECK(fixed.load(b.data(),b.size(),error)&&!objects.prepare(fixed,room.view(),error)&&objects.state(1)->frame==39);
 b=bytes;const auto pool=word(bytes,68);b[pool+word(bytes,at+40)+9]='x';fix(b);CHECK(fixed.load(b.data(),b.size(),error)&&!objects.prepare(fixed,room.view(),error)&&objects.state(1)->frame==39);
 std::printf("Field object lifecycle: %u checks; 499 actual-engine observations, 32 seeds with/without ready suppression, deferred show/delete and float32 idle clock; no collection/frontend claim\n",checks);
}
