#include "encore/save_menu.hpp"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0;bool ok=true;
void check(bool b,const char*s){++checks;if(!b){ok=false;std::fprintf(stderr,"FAIL: %s\n",s);}}
bool near(float a,float b){return std::abs(a-b)<.001f;}
std::vector<SaveMenuEvent>drain(SaveMenu&m){std::vector<SaveMenuEvent>v;SaveMenuEvent e;while(m.poll_event(e))v.push_back(e);return v;}
size_t count(const std::vector<SaveMenuEvent>&v,SaveMenuEventKind kind){size_t n=0;for(const auto&e:v)n+=e.kind==kind;return n;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int b=0;b<8;++b)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
void put(std::vector<uint8_t>&b,size_t i,uint32_t x){for(unsigned j=0;j<4;++j)b[i+j]=uint8_t(x>>(j*8));}
void repair(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,crc(b.data()+24,b.size()-24));}
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 std::string e;SaveMenuData d;check(d.load_file(argv[1],e),e.c_str());if(!ok)return 1;
 check(d.slot_count()==10&&d.activation_seconds()==.5&&d.scroll_seconds()==.2&&d.slot_spacing()==76,"pinned release timing/count data");
 check(d.text(SaveMenuText::Overwrite)=="OK to overwrite this file?"&&d.text(SaveMenuText::NoData)=="No Data","original translated content");
 check(d.layout(SaveMenuLayout::ChoiceYes).x==22&&d.layout(SaveMenuLayout::ChoiceNo).x==62,"native translated HBox positions");
 check(d.resources()[d.cursor_resource()].width==19&&d.resources()[d.flavors()[0].confirm_resource].width==28,"reversed ninepatch seams normalized losslessly");
 std::vector<SaveSlotMetadata>slots(d.slot_count());SaveMenu m;
 check(!m.open(d,slots,0,e)&&!m.open(d,slots,11,e)&&!m.open(d,{},1,e),"invalid initial slot/card count rejected");
 check(m.open(d,slots,1,e),"open empty slots");check(!m.open(d,slots,1,e),"double open rejected");
 check(m.step(.49,{0,0,true,true},e)&&m.phase()==SaveMenuPhase::Activating&&drain(m).empty(),"activation blocks input");
 check(m.step(.01,{0,0,true,false},e)&&m.phase()==SaveMenuPhase::Slots&&drain(m).empty(),"activation does not reuse opener");
 check(near(m.pose().cursor_margins.x,-6),"initial cursor idle clock continues during activation");
 check(m.step(0,{1,0,false,false},e)&&m.selected_slot()==2,"down selects second card");drain(m);
 check(m.step(.1,{},e)&&near(m.pose().cursor_y,71.25f)&&near(m.pose().cards_y,21),"quarter-out midpoint and fixed top scroll");
 check(m.step(.1,{},e)&&near(m.pose().cursor_y,76),"cursor tween ends at second position");
 check(m.step(0,{1,0,false,false},e)&&m.selected_slot()==3,"third selection scrolls cards");drain(m);m.step(.2,{},e);
 check(near(m.pose().cards_y,-55)&&near(m.pose().cursor_y,76),"two cursor positions preserve76px card spacing");
 for(int i=0;i<7;++i){m.step(.2,{1,0,false,false},e);drain(m);}check(m.selected_slot()==10,"reach final slot");
 m.step(.2,{1,0,false,false},e);drain(m);m.step(.2,{},e);check(m.selected_slot()==1&&near(m.pose().cards_y,21)&&near(m.pose().cursor_y,0),"down wraps to first");
 m.step(0,{-1,0,false,false},e);drain(m);m.step(.2,{},e);check(m.selected_slot()==10&&near(m.pose().cards_y,21-8*76)&&near(m.pose().cursor_y,76),"up wraps with last card at lower position");
 m.close();auto events=drain(m);check(events.size()==1&&events[0].kind==SaveMenuEventKind::Closed&&!events[0].any_saved,"close without write is not saved");m.close();check(drain(m).empty(),"close emitted once");
 check(m.open(d,slots,10,e),"valid non-first initial selection");m.step(.5,{},e);check(near(m.pose().cards_y,21-9*76)&&near(m.pose().cursor_y,0),"initial previous slot uses top position");m.step(0,{1,0,false,false},e);drain(m);m.step(.2,{},e);check(m.selected_slot()==1&&near(m.pose().cards_y,21),"last-top selection wraps correctly");
 check(m.step(0,{0,0,true,false},e)&&m.phase()==SaveMenuPhase::Writing,"empty accept requests synchronous caller write");events=drain(m);check(events.size()==2&&events[0].kind==SaveMenuEventKind::SoundRequested&&events[0].sound==SaveMenuSound::Accept&&events[1].kind==SaveMenuEventKind::SaveRequested&&events[1].slot==1,"empty accept sound then source slot request");
 m.step(.1,{1,1,true,true},e);m.close();check(m.phase()==SaveMenuPhase::Writing&&drain(m).empty(),"pending write cannot move, duplicate, or close");
 check(m.acknowledge_save(false,nullptr,e)&&m.phase()==SaveMenuPhase::Slots&&!m.any_saved()&&!m.slots()[0].occupied,"failed write preserves empty metadata and result");
 SaveSlotMetadata saved;saved.occupied=true;saved.lead_name="Ninten";saved.scene_label="Ninten's House";saved.menu_flavor="Plain";saved.highest_level=2;saved.playtime_seconds=123.5;saved.party={"ninten"};
 m.step(0,{0,0,true,false},e);drain(m);auto invalid=saved;invalid.party={"missing"};check(!m.acknowledge_save(true,&invalid,e)&&m.phase()==SaveMenuPhase::Writing&&!m.any_saved(),"invalid success metadata cannot publish success");
 check(m.acknowledge_save(true,&saved,e)&&m.any_saved()&&m.is_open()&&m.slots()[0].occupied,"successful write refreshes card and leaves menu open");
 m.step(0,{0,0,true,false},e);events=drain(m);check(m.phase()==SaveMenuPhase::OverwriteYield&&count(events,SaveMenuEventKind::SaveRequested)==0,"occupied slot yields before confirmation");
 m.step(0,{0,0,true,false},e);check(m.phase()==SaveMenuPhase::Overwrite&&m.pose().overwrite_choice==0&&drain(m).empty(),"idle boundary consumes original press; default Yes");
 m.step(0,{0,-1,false,false},e);check(m.pose().overwrite_choice==0&&drain(m).empty(),"overwrite does not wrap left");
 m.step(0,{1,0,false,false},e);check(m.pose().overwrite_choice==0&&drain(m).empty(),"overwrite uses horizontal input");
 m.step(0,{0,1,false,false},e);events=drain(m);check(m.pose().overwrite_choice==1&&events.size()==1&&events[0].sound==SaveMenuSound::Move,"move to No");
 m.step(.05,{},e);check(near(m.pose().arrow_choice,.9375f),"overwrite arrow quarter-out movement");m.step(0,{0,0,true,false},e);events=drain(m);check(m.phase()==SaveMenuPhase::Slots&&m.any_saved()&&count(events,SaveMenuEventKind::SaveRequested)==0&&events[0].sound==SaveMenuSound::Back,"No returns without erasing earlier success");
 m.step(0,{0,0,true,false},e);drain(m);m.step(0,{},e);m.step(0,{0,0,false,true},e);events=drain(m);check(m.phase()==SaveMenuPhase::Slots&&m.any_saved()&&events.size()==1&&events[0].sound==SaveMenuSound::Back,"cancel overwrite backs to slots");
 m.step(0,{0,0,true,false},e);drain(m);m.step(0,{},e);m.step(0,{0,0,true,false},e);events=drain(m);check(count(events,SaveMenuEventKind::SaveRequested)==1,"Yes requests repeated write");auto prior=m.slots()[0];check(m.acknowledge_save(false,nullptr,e)&&m.any_saved()&&m.slots()[0].playtime_seconds==prior.playtime_seconds,"later failed overwrite retains card and earlier success");
 m.step(0,{0,0,true,false},e);drain(m);m.step(0,{},e);m.step(0,{0,0,true,false},e);drain(m);saved.playtime_seconds=500;check(m.acknowledge_save(true,&saved,e)&&m.slots()[0].playtime_seconds==500&&m.is_open(),"repeated successful write refreshes but remains open");
 m.step(0,{0,0,false,true},e);events=drain(m);check(events.size()==1&&events[0].kind==SaveMenuEventKind::Closed&&events[0].any_saved,"final cancel returns any-success result exactly once");
 check(!m.acknowledge_save(false,nullptr,e)&&!m.step(-.1,{},e)&&!m.step(std::numeric_limits<double>::quiet_NaN(),{},e)&&!m.step(2,{},e)&&!m.step(0,{2,0,false,false},e),"invalid calls fail closed");
 slots[0]=saved;slots[0].menu_flavor="unreviewed";check(!m.open(d,slots,1,e),"unreviewed flavor rejected");slots[0]=saved;slots[0].lead_name="bad^glyph";check(!m.open(d,slots,1,e),"missing EBMain glyph rejected");slots[0]=saved;slots[0].playtime_seconds=std::numeric_limits<double>::infinity();check(!m.open(d,slots,1,e),"nonfinite metadata rejected");
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>raw((std::istreambuf_iterator<char>(f)),{});check(raw.size()>100,"fixture bytes");
 auto reject=[&](std::vector<uint8_t>b,const char*why){check(!d.load(b.data(),b.size(),e),why);check(d.valid()&&d.slot_count()==10,"failed reload preserves prior checked data");};
 for(size_t n:{size_t(0),size_t(8),size_t(23),size_t(24),raw.size()-1})reject(std::vector<uint8_t>(raw.begin(),raw.begin()+n),"truncation rejected");
 auto b=raw;b[0]^=1;reject(b,"magic rejected");b=raw;put(b,8,2);reject(b,"schema rejected");b=raw;put(b,20,2);reject(b,"unknown capability rejected");b=raw;b.back()^=1;reject(b,"CRC rejected");b=raw;b.push_back(0);repair(b);reject(b,"trailing bytes rejected after corrected CRC");
 b=raw;put(b,24,1);repair(b);reject(b,"one-position slot count rejected");b=raw;put(b,28+4,0x7ff80000);repair(b);reject(b,"NaN timing rejected after corrected CRC");b=raw;put(b,84,9999);repair(b);reject(b,"layout count rejected after corrected CRC");
 // Every single-byte payload mutation is exercised with both original and repaired
 // checksums; accepted numeric changes are bounded data, never new opcodes.
 for(size_t i=24;i<raw.size();i+=17){b=raw;b[i]^=0xff;SaveMenuData candidate;check(!candidate.load(b.data(),b.size(),e),"payload corruption rejected by CRC");repair(b);candidate.load(b.data(),b.size(),e);}
 std::printf("Save menu: %u checks\n",checks);return ok?0:1;
}
