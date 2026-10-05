#include "encore/items_data.hpp"
#include "encore/content.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"ItemsData check failed at %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);}
static void put(std::vector<uint8_t>&b,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(n>>(8*i));}
static void fix(std::vector<uint8_t>&b){put(b,16,0);put(b,16,encore::crc32(b.data(),b.size()));}
static uint32_t section(const std::vector<uint8_t>&b,ItemSection s){return get(b,64+(uint32_t(s)-1)*16+4);}
int main(int argc,char**argv){
 const char*path=argc>1?argv[1]:"romfs/data/opening.encitems";std::string error;std::vector<uint8_t>b;CHECK(encore::read_file(path,b,1024*1024,error));
 ItemData data;CHECK(data.load_file(path,error));auto view=data.view();CHECK(view.valid());CHECK(view.metadata().capacity==16);CHECK(view.metadata().owner==1);CHECK(view.count(ItemSection::Definitions)==2);CHECK(view.count(ItemSection::Instances)==1);
 auto def=view.definition(0);CHECK(def.flags==uint32_t(ItemDefinitionFlag::Equipment));CHECK(def.equipment_slot==3&&def.can_use==1);CHECK(view.string(def.name)=="Baseball Cap");CHECK(!view.string(def.description).empty());CHECK(view.string(def.description).find('\n')!=std::string_view::npos);
 auto initial=view.initial_instance(0);CHECK(initial.definition==0&&initial.equipped==1&&initial.doses==1);CHECK(view.parameter(ItemParameter::GridShape).x==2&&view.parameter(ItemParameter::GridShape).y==5);CHECK(view.clip_for(ItemClipRole::Open)!=item_no_index&&view.clip_for(ItemClipRole::Close)!=item_no_index&&view.clip_for(ItemClipRole::CursorIdle)!=item_no_index);CHECK(view.layout_for(ItemLayoutRole::Panel)!=item_no_index);CHECK(view.sound_for(ItemSoundEvent::Move).audio_id>0);
 for(uint32_t i=0;i<view.count(ItemSection::Layouts);++i){auto l=view.layout(i);CHECK(l.parent==item_no_index||l.parent<i);}
 for(uint32_t i=0;i<view.count(ItemSection::Clips);++i){auto c=view.clip(i);CHECK(c.duration>0&&c.track_count>0);for(uint32_t j=0;j<c.track_count;++j){auto t=view.track(c.first_track+j);CHECK(t.target<view.count(ItemSection::Layouts)&&t.key_count>0);CHECK(view.key(t.first_key).time<=c.duration);}}
 for(size_t n=0;n<b.size();++n){CHECK(!data.load(b.data(),n,error));}
 CHECK(!data.load(nullptr,b.size(),error));CHECK(view.definition(0).id==def.id&&data.view().definition(0).id==def.id);
 auto reject=[&](size_t offset,uint32_t value){auto bad=b;put(bad,offset,value);fix(bad);if(data.load(bad.data(),bad.size(),error)){std::fprintf(stderr,"Accepted corrupt Items offset %zu value %u\n",offset,value);CHECK(false);}CHECK(data.view().definition(0).id==def.id);};
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{0,0},{8,3},{12,0},{20,12},{24,3},{28,2},{32,0},{52,1},{64,0},{68,0},{72,UINT32_MAX},{76,1},{84,240},{88,2}})reject(edit.first,edit.second);
 const auto m=section(b,ItemSection::Metadata),d=section(b,ItemSection::Definitions),in=section(b,ItemSection::Instances),r=section(b,ItemSection::Resources),l=section(b,ItemSection::Layouts),p=section(b,ItemSection::Parameters),c=section(b,ItemSection::Clips),t=section(b,ItemSection::Tracks),k=section(b,ItemSection::Keys),s=section(b,ItemSection::Sounds),pool=section(b,ItemSection::Strings);
 // Rich descriptions are an explicit unsupported-rendering/action capability,
 // preserving raw controls rather than presenting a fabricated plain string.
 CHECK(get(b,8)==2&&get(b,24)==2&&get(b,28)==1);auto rich=view.definition(1);
 CHECK(rich.flags==uint32_t(ItemDefinitionFlag::RichDescription)&&rich.can_use==0&&rich.equipment_slot==item_no_index);
 CHECK(view.string(rich.source)=="AsthmaSpray");CHECK(view.string(rich.description).find("[Ninten]")!=std::string_view::npos);CHECK(view.string(rich.description).find("[Asthma]")!=std::string_view::npos);CHECK(view.string(rich.description).find("%s")!=std::string_view::npos);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{d+48+40,4},{d+48+40,3},{d+48+44,1},{d+48+20,0}})reject(edit.first,edit.second);
 for(auto versions:std::vector<std::pair<uint32_t,uint32_t>>{{1,1},{1,2},{2,1},{2,3},{3,2}}){auto bad=b;put(bad,8,versions.first);put(bad,24,versions.second);fix(bad);CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.view().definition(1).id==rich.id);}
 // A generic plain-only schema1 resource remains accepted. Source admission
 // independently controls real game content; this fixture tests format support.
 auto legacy=b;put(legacy,8,1);put(legacy,24,1);put(legacy,d+48+40,0);fix(legacy);ItemData old;CHECK(old.load(legacy.data(),legacy.size(),error));CHECK(old.view().definition(0).id==def.id);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{
  {m,0},{m,65},{m+4,0},{m+8,1},{m+12,1},
  {d,0},{d+4,2},{d+8,0},{d+12,0},{d+16,999},{d+20,4},{d+24,65536},{d+40,2},{d+44,2},
  {in,0},{in+4,999},{in+8,2},{in+12,0},
  {r,0},{r+4,2},{r+8,4},{r+12,0},{r+16,0},{r+20,0},{r+24,0},
  {l,0},{l+4,99},{l+8,0},{l+12,99},{l+16,999},{l+20,999},{l+24,16},{l+28,0x40000000},{l+36,0x7fc00000},{l+44,0xbf800000},{l+52,0x40000000},{l+68,8193},
  {p,99},{p+4,0x7fc00000},{p+4,0},
  {c,0},{c+4,99},{c+8,999},{c+12,0},{c+16,0},{c+20,2},
  {t,999},{t+4,99},{t+8,2},{t+12,999},{t+16,0},
  {k,0x7fc00000},{k,0x3f800000},{k+4,0x7fc00000},{k+8,0x7fc00000},
  {s,0},{s+4,2},{s+8,0},{s+12,1},
  {c+24+4,get(b,c+4)},{c+24+8,get(b,c+8)},{p+20,get(b,p)},
  {r+60,get(b,r)},{r+60+4,get(b,r+4)},{l+84,get(b,l)},{s+16,get(b,s)},{l+84+8,1}})reject(edit.first,edit.second);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{p+12,0x3f800000},{p+2*20+12,0},{p+4*20+12,0},{p+5*20+4,0},{p+5*20+8,0xbf800000},{p+5*20+12,0},{p+5*20+16,0x3f800000},{p+6*20+4,0},{p+6*20+8,0xbf800000},{p+6*20+12,0x3f800000},{p+7*20+4,0x40000000},{p+10*20+12,0},{p+10*20+12,0x40400000},{p+10*20+16,0x3f800000},{l+24,4}})reject(edit.first,edit.second);
 for(uint32_t i=0;i<view.count(ItemSection::Layouts);++i){auto layout=view.layout(i);if(layout.role==uint32_t(ItemLayoutRole::Grid)||layout.role==uint32_t(ItemLayoutRole::InfoPanel))reject(l+i*84+4,uint32_t(ItemLayoutRole::Container));}
 auto missing_event=b;missing_event.resize(missing_event.size()-16);put(missing_event,12,uint32_t(missing_event.size()));put(missing_event,64+10*16+8,4);put(missing_event,64+10*16+12,64);fix(missing_event);CHECK(!data.load(missing_event.data(),missing_event.size(),error));
 for(uint8_t invalid:{uint8_t(0xff),uint8_t(0xc0),uint8_t(0xed)}){auto bad=b;bad[pool+1]=invalid;fix(bad);CHECK(!data.load(bad.data(),bad.size(),error));}
 auto zeros=b;std::fill(zeros.begin()+r+28,zeros.begin()+r+60,0);fix(zeros);CHECK(!data.load(zeros.data(),zeros.size(),error));
 auto badcrc=b;badcrc.back()^=1;CHECK(!data.load(badcrc.data(),badcrc.size(),error));auto trailing=b;trailing.push_back(0);put(trailing,12,uint32_t(trailing.size()));fix(trailing);CHECK(!data.load(trailing.data(),trailing.size(),error));
 // The public view fails safely for absent sections, out-of-range records and
 // offsets into an existing string. Successful validation owns a copy of input.
 CHECK(!ItemView{}.valid());CHECK(ItemView{}.count(ItemSection::Resources)==0);CHECK(ItemView{}.string(0).empty());CHECK(ItemView{}.clip_for(ItemClipRole::Open)==item_no_index);CHECK(ItemView{}.layout_for(ItemLayoutRole::Panel)==item_no_index);CHECK(ItemView{}.sound_for(ItemSoundEvent::Open).audio_id==0);
 CHECK(view.count(static_cast<ItemSection>(0))==0&&view.count(static_cast<ItemSection>(65535))==0);CHECK(view.string(UINT32_MAX).empty());CHECK(view.string(2).empty());CHECK(view.definition(UINT32_MAX).id==0);CHECK(view.initial_instance(UINT32_MAX).id==0);CHECK(view.resource(UINT32_MAX).id==0);CHECK(view.layout(UINT32_MAX).id==0);CHECK(view.clip(UINT32_MAX).id==0);CHECK(view.track(UINT32_MAX).key_count==0);CHECK(view.key(UINT32_MAX).time==0);CHECK(view.sound(UINT32_MAX).audio_id==0);CHECK(view.parameter(static_cast<ItemParameter>(999)).x==0);
 auto changed=b;float x=view.layout(0).rect.x+1;uint32_t bits;std::memcpy(&bits,&x,4);put(changed,l+36,bits);fix(changed);CHECK(data.load(changed.data(),changed.size(),error));CHECK(data.view().layout(0).rect.x==x);CHECK(!data.load(badcrc.data(),badcrc.size(),error));CHECK(data.view().layout(0).rect.x==x);CHECK(data.load(b.data(),b.size(),error));std::fill(b.begin(),b.end(),0);CHECK(data.view().definition(0).id==def.id);CHECK(data.view().layout(0).rect.x==x-1);
 std::printf("ItemsData: %u checks; all truncations, CRC-correct malformed fields, safe views and transactional data-only reload\n",checks);
}
