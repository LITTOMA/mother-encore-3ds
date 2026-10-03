#include "music_region_service.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace encore;
namespace {
unsigned checks=0,mutations=0,owner_calls=0,file_opens=0;
size_t linear_bytes=0;bool dsp=false;
std::array<std::vector<ndspWaveBuf*>,20>queues;
void check(bool okay,const char*why){++checks;if(!okay){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
size_t channel(int i){check(i>=0&&i<20,"valid observed channel");++mutations;return size_t(i);}
void drain(int i){for(auto*p:queues[size_t(i)])p->status=NDSP_WBUF_DONE;queues[size_t(i)].clear();}
}
extern "C" FILE*__real_fopen(const char*,const char*);
extern "C" FILE*__wrap_fopen(const char*p,const char*m){++file_opens;return __real_fopen(p,m);}
Result ndspInit(){++owner_calls;dsp=true;return 0;}
void ndspExit(){++owner_calls;for(int i=0;i<20;++i)drain(i);dsp=false;}
void ndspSetMasterVol(float){++owner_calls;}
uint32_t ndspGetDroppedFrames(){return 0;}
void*linearAlloc(size_t n){linear_bytes+=n;return std::malloc(n);}
void linearFree(void*p){linear_bytes-=24576;std::free(p);}
void ndspChnReset(int i){channel(i);drain(i);}void ndspChnWaveBufClear(int i){channel(i);drain(i);}
void ndspChnSetMix(int i,float*v){channel(i);check(v[0]==v[1],"stereo mix unchanged");}
Result DSP_FlushDataCache(const void*p,uint32_t n){check(p&&n&&n<=8192,"checked streaming payload");return 0;}
void ndspChnWaveBufAdd(int i,ndspWaveBuf*p){check(dsp,"NDSP owner still alive");p->status=NDSP_WBUF_QUEUED;queues[channel(i)].push_back(p);}
void ndspChnSetInterp(int i,int){channel(i);}void ndspChnSetRate(int i,float){channel(i);}void ndspChnSetFormat(int i,int){channel(i);}
bool complete_prepare(ctr::MusicRegionService&service,const char*regions,const char*bank,const char*root,uint32_t capacity,const ctr::AudioPlayer&owner,std::string&e){
 if(!service.begin_prepare(regions,bank,root,capacity,owner,e))return false;
 auto step=ctr::MusicPreparationStep::Progress;
 while(step==ctr::MusicPreparationStep::Progress)step=service.prepare_step(8192,e);
 return step==ctr::MusicPreparationStep::Ready;
}
int main(int argc,char**argv){
 check(argc==6,"opening bank/root, region pack/bank/root arguments");
 ctr::AudioPlayer owner;ctr::MusicRegionService service;std::string e;
 const auto dormant_opens=file_opens,dormant_ndsp=mutations+owner_calls;
 for(int i=0;i<100;++i)check(service.update(.016,owner,e),"Dormant tick succeeds with no owner");
 check(file_opens==dormant_opens&&mutations+owner_calls==dormant_ndsp&&linear_bytes==0,"Dormant strictly zero file opens, buffers and NDSP calls");
 check(!complete_prepare(service,"not-a-path","not-a-bank","not-a-root",8,owner,e)&&file_opens==0,"unavailable owner rejected before resource IO");
 check(owner.initialize(argv[1],argv[2],e),e.c_str());
 auto snapshot=owner.observe_music();check(snapshot.available&&!snapshot.playing&&snapshot.generation==0,"fresh observation reflects real owner");
 check(owner.play(34,ctr::AudioLane::Music,e),e.c_str());snapshot=owner.observe_music();const auto first=snapshot.generation;
 check(first==1&&snapshot.playing&&snapshot.asset_id==34&&!snapshot.tweening,"successful music start creates identity");
 const auto observed_mutations=mutations,observed_opens=file_opens;
 for(int i=0;i<100;++i)check(owner.observe_music().generation==first,"observation stable");
 check(mutations==observed_mutations&&file_opens==observed_opens,"observation read-only");
 check(owner.play(22,ctr::AudioLane::Effect,e)&&owner.play(1001,ctr::AudioLane::Jingle,e),"unrelated lanes play");
 check(owner.observe_music().generation==first,"effect and jingle do not change external music identity");
 check(!owner.play(999999,ctr::AudioLane::Music,e)&&owner.observe_music().generation==first,"failed play cannot mint identity");
 check(owner.fade_music(2,e)&&owner.observe_music().tweening,"real fade observed");
 check(owner.update(2,e)&&!owner.observe_music().playing&&!owner.observe_music().tweening&&owner.observe_music().generation==first,"fade stop reflected without reusing identity");
 owner.reset_scene();check(owner.observe_music().generation==first&&!owner.observe_music().playing,"reset stops while preserving identity sequence");
 check(owner.play(1001,ctr::AudioLane::Music,e),"one-shot used only as observation completion fixture");const auto one_shot=owner.observe_music().generation;
 unsigned frames=0;while(owner.observe_music().playing&&frames++<200){drain(0);check(owner.update(0,e),"natural finish drain");}
 check(!owner.observe_music().playing&&owner.observe_music().generation==one_shot&&one_shot>first,"natural finish observed with same ended identity");
 check(owner.play(34,ctr::AudioLane::Music,e)&&owner.play(32,ctr::AudioLane::DialogueMusic,e),"existing dialogue music fixture");
 check(owner.observe_music().dialogue_music_playing,"unmapped dialogue music reported truthfully");
 const auto before_prepare=mutations+owner_calls;const auto existing_queues=queues;
 check(complete_prepare(service,argv[3],argv[4],argv[5],8,owner,e),e.c_str());
 check(service.phase()==ctr::MusicRegionServicePhase::Prepared&&service.buffer_bytes()==196608&&mutations+owner_calls==before_prepare&&queues==existing_queues,"prepare silent while existing music/effect channels preserved");
 const auto prepared_opens=file_opens;check(service.update(1,owner,e)&&file_opens==prepared_opens&&mutations+owner_calls==before_prepare,"Prepared tick does not play, read or call NDSP");
 check(!service.commit_scene(1,owner,e)&&service.phase()==ctr::MusicRegionServicePhase::Prepared&&mutations+owner_calls==before_prepare,"unmapped DialogueMusic commit gate preserves prepared and active state");
 owner.reset_scene();check(owner.play(34,ctr::AudioLane::Music,e),"source House owner starts after reset");const auto house_generation=owner.observe_music().generation;
 const auto before_commit=mutations+owner_calls;
 check(service.commit_scene(1,owner,e)&&service.phase()==ctr::MusicRegionServicePhase::Active&&service.live_voice_count()==0&&mutations+owner_calls==before_commit,"real commit alone does not manufacture Area entry or audio");
 check(service.update(0,owner,e)&&mutations+owner_calls==before_commit,"active scene with no real Area events remains silent");
 upstream::MusicRegionContext ctx;ctx.flag=[](std::string_view){return false;};
 check(service.area_enter(1,"Music/MusicArea",ctx,owner,e)&&mutations+owner_calls==before_commit,"real Area callback changes only queued core state");
 const auto house_queue=queues[0];check(service.update(0,owner,e)&&service.live_voice_count()==1&&service.submitted_voices()==1,"first actual Area event reaches the production adapter");
 check(queues[0]==house_queue&&queues[4].size()==3&&owner.observe_music().generation==house_generation,"new region channel does not change House voice or identity");
 check(service.update(.1,owner,e)&&service.area_enter(1,"Music/MusicArea3",ctx,owner,e)&&service.update(.5,owner,e)&&service.submitted_voices()==1,"same-song callback above silent threshold reuses production voice");
 check(!service.cancel_preparation(e)&&!service.finish_scene(1,e),"committed scene requires its source exits");
 const auto busy_mutations=mutations+owner_calls,busy_opens=file_opens;check(!complete_prepare(service,argv[3],argv[4],argv[5],8,owner,e)&&mutations+owner_calls==busy_mutations&&file_opens==busy_opens,"new preparation cannot replace live service");
 check(service.tree_exit(1,"Music/MusicArea",e)&&service.tree_exit(1,"Music/MusicArea3",e)&&service.finish_scene(1,e),"actual tree exit order enters Draining");
 check(service.update(0,owner,e)&&service.phase()==ctr::MusicRegionServicePhase::Draining&&!queues[4].empty(),"Draining retains queued fading voice instead of immediate shutdown");
 check(owner.play(34,ctr::AudioLane::Music,e)&&owner.observe_music().generation>house_generation,"new House playback has fresh monotonic identity");
 check(service.commit_scene(2,owner,e)&&service.area_enter(2,"Music/MusicArea",ctx,owner,e)&&service.update(0,owner,e),"quick scene return keeps source old fade and new external order");
 check(service.live_voice_count()==2&&queues[4].size()==3&&queues[5].size()==3,"old region phase and new same-song voice coexist across scene commit");
 check(service.area_enter(1,"Music/Unknown",ctx,owner,e),"old epoch event ignored before source lookup");
 check(service.tree_exit(2,"Music/MusicArea",e)&&service.finish_scene(2,e)&&service.update(.75,owner,e)&&service.phase()==ctr::MusicRegionServicePhase::Draining,"half fade keeps Draining");
 check(service.update(.75,owner,e)&&service.phase()==ctr::MusicRegionServicePhase::Prepared&&service.live_voice_count()==0&&queues[4].empty()&&queues[5].empty()&&!queues[0].empty(),"fade completion returns resident service to Prepared preserving House");
 check(service.cancel_preparation(e)&&service.phase()==ctr::MusicRegionServicePhase::Dormant,"prepared cache can be discarded safely");
 const auto previous=owner.observe_music().generation;owner.shutdown();check(!owner.observe_music().available&&!owner.observe_music().playing&&owner.observe_music().generation==previous,"shutdown observation truthful without identity reuse");
 check(owner.initialize(argv[1],argv[2],e)&&owner.play(34,ctr::AudioLane::Music,e)&&owner.observe_music().generation>previous,"reinitialize does not reuse identity");owner.shutdown();
 check(linear_bytes==0,"all owner and service buffers released");
 std::cout<<"Music production service/observation PASS "<<checks<<" checks; real API under NDSP double, no audible output claim\n";
}
