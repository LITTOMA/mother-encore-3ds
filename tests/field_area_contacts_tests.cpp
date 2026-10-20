#include "encore/field_area_contacts.hpp"
#include "encore/field_swing.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Area timing %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
}
int main(int argc,char**argv){
 CHECK(argc==3);FieldSwingData swing;CHECK(swing.load_file(argv[1],error));
 FieldAreaContacts contacts;FieldAreaEvents events;uint8_t enabled=1,overlap=1;
 events.count=17;CHECK(!contacts.begin(&enabled,1,events,error)&&events.count==17);CHECK(!contacts.end(&enabled,&overlap,1,error));
 for(const auto&ids:std::vector<std::vector<uint32_t>>{{},{0},{3,3},std::vector<uint32_t>(129,1)})CHECK(!contacts.initialize(ids,error)&&!contacts.ready());
 CHECK(contacts.initialize({12},error)&&contacts.count()==1&&!contacts.inside(0)&&!contacts.inside(1));CHECK(!contacts.initialize({13},error));
 CHECK(!contacts.begin(nullptr,1,events,error)&&events.count==17);CHECK(!contacts.begin(&enabled,0,events,error));uint8_t bad=2;CHECK(!contacts.begin(&bad,1,events,error)&&!contacts.sampling());
 FILE*f=std::fopen(argv[2],"rb");CHECK(f);unsigned tick,source,target,inside,event,seen=0;float x,y;int columns;
 while((columns=std::fscanf(f,"%u\t%u\t%u\t%f\t%f\t%u\t%u",&tick,&source,&target,&x,&y,&inside,&event))==7){
  CHECK(tick==seen&&source<=1&&target<=1&&inside<=1&&event<=2);enabled=uint8_t(source&&target);
  CHECK(contacts.begin(&enabled,1,events,error)&&contacts.sampling());CHECK(contacts.inside(0)==bool(inside));
  CHECK(events.count==unsigned(event!=0));if(event)CHECK(events.values[0].owner==12&&events.values[0].index==0&&events.values[0].entered==(event==1));
  const auto before=events.count;CHECK(!contacts.begin(&enabled,1,events,error)&&events.count==before);
  auto pose=swing.initial();pose.pivot=-90;pose.hitbox=bool(source);bool hit=false;CHECK(swing_rectangle_overlap(swing,pose,{x,y},{0,-10},{8,6},hit,error));overlap=uint8_t(hit);
  CHECK(!contacts.end(&bad,&overlap,1,error)&&contacts.sampling());CHECK(!contacts.end(&enabled,&bad,1,error)&&contacts.sampling());
  CHECK(!contacts.end(nullptr,&overlap,1,error)&&contacts.sampling());CHECK(!contacts.end(&enabled,&overlap,0,error)&&contacts.sampling());
  CHECK(contacts.end(&enabled,&overlap,1,error)&&!contacts.sampling());CHECK(!contacts.end(&enabled,&overlap,1,error));++seen;
 }
 CHECK(columns==EOF&&std::feof(f)&&std::fclose(f)==0&&seen==41);
 // Full checked capacity, distinct stable owners, deterministic dispatch order.
 FieldAreaContacts full;std::vector<uint32_t>ids;for(unsigned i=0;i<128;++i)ids.push_back(i+1);CHECK(full.initialize(ids,error));std::array<uint8_t,128>bits{};bits.fill(1);
 CHECK(full.begin(bits.data(),bits.size(),events,error)&&events.count==0&&full.end(bits.data(),bits.data(),bits.size(),error));
 CHECK(full.begin(bits.data(),bits.size(),events,error)&&events.count==128);for(unsigned i=0;i<128;++i)CHECK(events.values[i].owner==i+1&&events.values[i].index==i&&events.values[i].entered);
 CHECK(full.end(bits.data(),bits.data(),bits.size(),error));bits.fill(0);CHECK(full.begin(bits.data(),bits.size(),events,error)&&events.count==128);for(unsigned i=0;i<128;++i)CHECK(!events.values[i].entered);CHECK(full.end(bits.data(),bits.data(),bits.size(),error));
 std::printf("Area timing: %u checks;41 original-engine physics observations, canceled transient enter, enter/exit/reentry, source/target disable, owner/phase/capacity rejection; no scene integration claim\n",checks);
}
