#include "encore/continue_menu.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0;bool ok=true;
void check(bool b,const char*s){++checks;if(!b){ok=false;std::fprintf(stderr,"FAIL: %s\n",s);}}
std::vector<ContinueEvent>drain(ContinueMenu&m){std::vector<ContinueEvent>v;ContinueEvent e;while(m.poll_event(e))v.push_back(e);return v;}
size_t count(const std::vector<ContinueEvent>&v,ContinueEventKind k){size_t n=0;for(const auto&e:v)n+=e.kind==k;return n;}
bool has_sound(const std::vector<ContinueEvent>&v,const std::string&s){for(const auto&e:v)if(e.kind==ContinueEventKind::SoundRequested&&e.path==s)return true;return false;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
void put(std::vector<uint8_t>&b,size_t i,uint32_t x){for(unsigned j=0;j<4;++j)b[i+j]=uint8_t(x>>(8*j));}
void repair(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,crc(b.data()+24,b.size()-24));}
void tick(ContinueMenu&m,double dt,std::string&e){check(m.step(dt,{},e),e.c_str());}
void enter_slots(ContinueMenu&m,std::string&e){check(m.step(0,{0,0,true,false},e),"select title Load");drain(m);tick(m,.54,e);tick(m,0,e);tick(m,.5,e);tick(m,.2,e);check(m.phase()==ContinuePhase::Slots,"source activation reaches slot list");drain(m);}
}
int main(int argc,char**argv){
 if(argc!=4)return 2;
 std::string e;ContinueMenuData d;SaveMenuData sd;check(d.load_file(argv[1],e),e.c_str());check(sd.load_file(argv[2],e),e.c_str());if(!ok)return 1;
 check(d.text(0)=="Play"&&d.text(1)=="Copy"&&d.text(2)=="Delete"&&d.text(3)=="Settings","source translated action labels");
 check(d.layout(ContinueLayout::Body).x==8&&d.layout(ContinueLayout::Body).w==-8,"nonembedded source margins");
 check(d.viewports()[0].actions[3].x==211&&d.viewports()[1].actions[3].x==291,"Godot original HBox at both viewport widths");
 check(d.title_background_color()==0xff0b0007,"expanded title uses tinted source exterior color");
 const auto title_shift=d.layout(ContinueLayout::TitleViewportOffsets);check(title_shift.x==0&&title_shift.y==0&&title_shift.w==40&&title_shift.h==30,"nonfocused circle uses the same external title center offsets at both viewports");
 for(size_t i=0;i<d.title_layers().size();++i){const auto&a=d.title_layers()[i];check(a.viewport_offsets[0].x==0&&a.viewport_offsets[0].y==0,"reference title layer remains at source coordinates");check(a.viewport_offsets[1].x==40&&a.viewport_offsets[1].y==(i?30:60),"expanded title centered and backdrop bottom-centered without scaling");}
 for(const auto&a:d.title_options()){check(a.viewport_offsets[0].x==0&&a.viewport_offsets[0].y==0,"reference title option remains at source coordinates");check(a.viewport_offsets[1].x==40&&a.viewport_offsets[1].y==30,"expanded title option follows centered artwork");}
 check(d.animation(ContinueAnimation::FadeIn).length==1&&d.animation(ContinueAnimation::FadeIn).key_end==.4,"fade opaque before animation_done");
 check(d.door_in_speed()==1.5&&d.music_fade_seconds()==.5&&d.door_music_fade_seconds()==.8,"source transition and music rates");
 std::ifstream oracle(argv[3]);uint32_t animation=0;double time=0,value=0;unsigned samples=0;while(oracle>>animation>>time>>value){++samples;check(animation<uint32_t(ContinueAnimation::Count)&&std::abs(d.animation(ContinueAnimation(animation)).value(time)-value)<.000002,"Godot source fade interpolation oracle");}check(samples==24,"complete source fade oracle");
 std::vector<SaveSlotMetadata>slots(sd.slot_count());SaveSlotMetadata saved;saved.occupied=true;saved.lead_name="Ninten";saved.scene_label="Ninten's House";saved.menu_flavor="Plain";saved.highest_level=2;saved.playtime_seconds=123.5;saved.party={"ninten"};slots[2]=saved;ContinueMenu m;
 check(!m.open(d,sd,{},0,e),"slot registry must be complete");check(m.open(d,sd,slots,0,e),"open original final title");check(m.pose().title_option==0,"zero remembered slot selects NewGame despite occupied slots");auto events=drain(m);check(events.size()==1&&events[0].kind==ContinueEventKind::TitleMusicRequested&&events[0].path=="Audio/Music/Mother Earth.mp3","source title music event");check(!m.open(d,sd,slots,3,e),"double open rejected");
 check(m.step(0,{0,0,true,false},e),"NewGame boundary accepts");events=drain(m);check(count(events,ContinueEventKind::LoadRequested)==0&&events.back().kind==ContinueEventKind::BoundaryRequested&&events.back().boundary==ContinueBoundary::NewGame,"NewGame never silently loads occupied/newest slot");
 m.step(0,{-1,0,false,false},e);drain(m);check(m.pose().title_option==3,"title up wraps Exit");m.step(0,{-1,0,false,false},e);drain(m);m.step(0,{0,0,true,false},e);events=drain(m);check(events.back().boundary==ContinueBoundary::Settings&&m.phase()==ContinuePhase::Title,"Settings explicit bounded boundary");
 m.close();check(m.open(d,sd,slots,3,e),"remembered selection opens");drain(m);check(m.pose().title_option==1,"nonzero remembered selects Load");enter_slots(m,e);check(m.selected_slot()==3&&m.slot_model().pose().cards_y==21-2*76,"remembered slot opens exact top card");
 m.step(0,{0,0,true,false},e);events=drain(m);check(m.phase()==ContinuePhase::Actions&&m.pose().action==0&&count(events,ContinueEventKind::LoadRequested)==0,"occupied nonembedded confirm selects Play row without loading");
 check(m.pose().arrow_frame==1,"source card arrow begins from hidden AnimatedSprite phase");
 check(m.pose().slots.cursor_margins.y==17&&m.pose().slots.cursor_margins.h==94,"source selected border fixed geometry");
 m.step(0,{0,-1,false,false},e);drain(m);check(m.pose().action==3,"action left wraps Options and skips spacers");m.step(0,{0,0,true,false},e);events=drain(m);check(events.back().kind==ContinueEventKind::BoundaryRequested&&events.back().boundary==ContinueBoundary::Options&&m.phase()==ContinuePhase::Actions,"Options stays explicit boundary");
 m.step(.05,{0,1,false,false},e);drain(m);check(m.pose().action==0,"action right wraps Play");m.step(0,{0,1,false,false},e);drain(m);check(m.pose().action==0,"source cursor Timer prevents immediate repeat");m.step(.05,{0,1,false,false},e);drain(m);m.step(0,{0,0,true,false},e);events=drain(m);check(events.back().boundary==ContinueBoundary::Copy&&count(events,ContinueEventKind::LoadRequested)==0,"Copy never repurposed as Play");
 m.step(.05,{0,1,false,false},e);drain(m);m.step(0,{0,0,true,false},e);events=drain(m);check(events.back().boundary==ContinueBoundary::Delete,"Delete explicit boundary");
 m.step(0,{0,0,false,true},e);events=drain(m);check(m.phase()==ContinuePhase::Slots&&has_sound(events,d.sound(ContinueSound::Back)),"card cancel returns slots with source sound");
 m.step(0,{-1,0,false,false},e);drain(m);check(m.selected_slot()==2,"move to empty exact slot");m.step(0,{0,0,true,false},e);events=drain(m);check(m.phase()==ContinuePhase::Slots&&has_sound(events,d.sound(ContinueSound::Restricted))&&count(events,ContinueEventKind::LoadRequested)==0,"empty slot restricted, never fallback");
 m.step(0,{0,0,false,true},e);drain(m);tick(m,.67,e);tick(m,0,e);tick(m,.67,e);drain(m);check(m.phase()==ContinuePhase::Title&&m.pose().title_option==1,"list cancel returns source title preference");enter_slots(m,e);check(m.selected_slot()==3,"cancel does not persist browsed empty slot");
 m.step(0,{0,0,true,false},e);drain(m);m.step(0,{0,0,true,false},e);events=drain(m);check(m.phase()==ContinuePhase::LoadFade&&count(events,ContinueEventKind::LoadFadeRequested)==1&&count(events,ContinueEventKind::LoadRequested)==0,"Play requests fade and music stop before load");
 tick(m,.4,e);check(m.pose().fade.alpha==1&&drain(m).empty(),"opaque at .4 but no load before fade_in_done");m.step(.59,{1,1,true,true},e);check(m.phase()==ContinuePhase::LoadFade&&drain(m).empty(),"fade locks selection and cannot duplicate Play");tick(m,.01,e);events=drain(m);check(m.phase()==ContinuePhase::LoadPending&&events.size()==1&&events[0].kind==ContinueEventKind::LoadRequested&&events[0].slot==3,"full fade emits one typed exact LoadRequested");
 m.step(1,{1,1,true,true},e);check(m.phase()==ContinuePhase::LoadPending&&drain(m).empty(),"pending caller restore blocks all inputs");check(m.acknowledge_load(false,e)&&m.phase()==ContinuePhase::Actions&&!m.pose().fade.visible,"failed read/application returns usable Play row");
 m.step(0,{0,0,true,false},e);drain(m);tick(m,1,e);drain(m);check(m.acknowledge_load(true,e)&&m.pose().world_visible&&m.pose().fade.alpha==1,"successful fresh restore enters black source idle boundary");tick(m,0,e);check(m.pose().fade.focused&&m.pose().fade.circle,"source Circle Focus reveal selected");tick(m,.4,e);check(m.is_open()&&m.pose().world_visible,"world reveal remains visible until animation completion");tick(m,.4,e);check(!m.is_open(),"source Circle Out completion releases frontend");check(!m.acknowledge_load(true,e),"stray acknowledgment rejected");
 check(!m.step(-1,{},e)&&!m.step(std::numeric_limits<double>::quiet_NaN(),{},e)&&!m.step(2,{},e)&&!m.step(0,{2,0,false,false},e),"invalid input/time fails closed");
 check(m.open(d,sd,slots,9999,e),"source remembered index clamps high");drain(m);enter_slots(m,e);check(m.selected_slot()==sd.slot_count(),"high remembered clamps last, never newest");m.close();
 check(m.open(d,sd,slots,3,e),"adapted tap fixture opens");drain(m);enter_slots(m,e);m.step(0,{0,0,true,false},e);drain(m);check(m.phase()==ContinuePhase::Actions,"adapted tap starts action row");
 check(m.step(0,{0,1,false,false},e,true)&&m.pose().action==1,"one adapted press selects one action");drain(m);check(m.step(.01,{0,1,false,false},e,true)&&m.pose().action==2,"separate quick press is not discarded by held-repeat timer");drain(m);m.close();
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>raw((std::istreambuf_iterator<char>(f)),{});auto reject=[&](std::vector<uint8_t>b,const char*why){check(!d.load(b.data(),b.size(),e),why);check(d.valid()&&d.text(0)=="Play","rejected reload preserves prior checked data");};
 for(size_t n:{size_t(0),size_t(8),size_t(23),size_t(24),raw.size()-1})reject(std::vector<uint8_t>(raw.begin(),raw.begin()+n),"truncated pack rejected");
 auto b=raw;b[0]^=1;reject(b,"magic rejected");b=raw;put(b,8,1);reject(b,"old version rejected");b=raw;put(b,8,3);reject(b,"unknown version rejected");b=raw;put(b,20,2);reject(b,"capability rejected");b=raw;b.back()^=1;reject(b,"CRC rejected");b=raw;b.push_back(0);repair(b);reject(b,"trailing payload rejected with repaired CRC");b=raw;put(b,36,0x7ff80000);repair(b);reject(b,"nonfinite transition rejected with repaired CRC");b=raw;put(b,28,0x000b0007);repair(b);reject(b,"transparent title exterior rejected");
 // Find the encoded first layer from its decoded fields, then test each new
 // offset constraint with a correct CRC so parser validation is exercised.
 std::vector<uint8_t>pattern(36);const auto&first=d.title_layers().front();put(pattern,0,first.resource);const float fields[]={first.rect.x,first.rect.y,first.rect.w,first.rect.h,first.viewport_offsets[0].x,first.viewport_offsets[0].y,first.viewport_offsets[1].x,first.viewport_offsets[1].y};for(size_t i=0;i<8;++i){uint32_t bits;std::memcpy(&bits,&fields[i],4);put(pattern,4+i*4,bits);}auto at=std::search(raw.begin(),raw.end(),pattern.begin(),pattern.end());check(at!=raw.end(),"encoded title offset record found");if(at!=raw.end()){const size_t start=size_t(at-raw.begin());auto bad_offset=[&](size_t field,float value,const char*why){b=raw;uint32_t bits;std::memcpy(&bits,&value,4);put(b,start+4+field*4,bits);repair(b);reject(b,why);};bad_offset(4,1,"reference title translation rejected");bad_offset(6,40.5f,"fractional title translation rejected");bad_offset(7,61,"out-of-viewport title translation rejected");bad_offset(6,std::numeric_limits<float>::quiet_NaN(),"nonfinite title translation rejected");}
 // Mutating every byte offset with repaired CRC also exercises bounds on names,
 // counts and indices under sanitizers; only schema-valid numeric data may pass.
 for(size_t i=24;i<raw.size();++i){b=raw;b[i]^=0xff;ContinueMenuData candidate;check(!candidate.load(b.data(),b.size(),e),"payload corruption rejected");repair(b);candidate.load(b.data(),b.size(),e);}
 std::printf("Continue menu: %u checks\n",checks);return ok?0:1;
}
