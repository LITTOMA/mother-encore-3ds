#include "music_bank_transports.hpp"
#include "audio_player.hpp"
#include "encore/music_scene_graph.hpp"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace encore;
namespace {
unsigned checks=0,mutations=0,owner_calls=0,file_opens=0;
size_t linear_bytes=0;bool dsp=false;
std::array<std::vector<ndspWaveBuf*>,20>queues;
std::array<bool,20>paused_channels{};
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
void ndspChnSetPaused(int lane,bool paused){paused_channels[channel(lane)]=paused;++mutations;}
void ndspChnSetInterp(int i,int){channel(i);}void ndspChnSetRate(int i,float){channel(i);}void ndspChnSetFormat(int i,int){channel(i);}
bool complete(ctr::MusicBankTransports&service,uint64_t id,std::string&e){
 for(unsigned i=0;i<20000;++i){auto step=service.prepare_step(id,8192,e);if(step==ctr::MusicPreparationStep::Ready)return true;if(step==ctr::MusicPreparationStep::Failed)return false;}
 check(false,"bounded transport preparation terminates");return false;
}
int main(int argc,char**argv){
 check(argc==6,"opening bank/root and region pack/bank/root");std::string e;
 ctr::MusicBankTransports service;upstream::MusicBankRoutes routes;
 const auto dormant_files=file_opens,dormant_calls=mutations+owner_calls;
 check(!service.initialize(0,0,e)&&!service.initialize(65,0,e)&&!service.initialize(8,NAN,e),"invalid transport policy");
 check(!service.begin_prepare(1,"unused","unused","unused",4,true,-6,e)&&!service.publish(1,routes,e)&&!service.cancel(1,e)&&!service.pump_streams(e)&&!service.set_paused(true,e)&&!service.set_user_music_db(0,e),"uninitialized transport rejects without implicit IO");
 check(file_opens==dormant_files&&mutations+owner_calls==dormant_calls,"uninitialized calls are inert");
 ctr::AudioPlayer owner;check(owner.initialize(argv[1],argv[2],e)&&owner.play(34,ctr::AudioLane::Music,e),e.c_str());auto root_queue=queues[0];
 check(service.initialize(4,0,e)&&!service.initialize(4,0,e),"transport set initialized once");
 auto calls=mutations+owner_calls,files=file_opens;
 check(!service.begin_prepare(1,"unused","unused","unused",4,false,-6,e)&&file_opens==files&&mutations+owner_calls==calls,"missing actual NDSP owner rejects before IO");
 check(service.begin_prepare(1,argv[3],argv[4],argv[5],4,true,owner.observe_music().master_db,e)&&service.preparing()==1,"detached metadata candidate");
 check(!service.data(1)&&!service.bank(1)&&!service.publish(1,routes,e)&&!service.begin_prepare(2,argv[3],argv[4],argv[5],4,true,owner.observe_music().master_db,e),"unvalidated and concurrent candidates rejected");
 check(complete(service,1,e)&&service.data(1)&&service.bank(1)&&service.preparing()==0&&queues[0]==root_queue&&mutations+owner_calls==calls,"complete preparation remains zero NDSP");
 const auto data=*service.data(1);const auto bank=*service.bank(1);
 check(routes.initialize({4,16,bank.master_db(),bank.silence_db()},e)&&service.publish(1,routes,e),"prepared provider published explicitly");
 upstream::MusicSceneGraph graph;upstream::MusicRegionContext context;context.flag=[](std::string_view){return false;};
 check(graph.initialize({data.silence_db(),0,data.fade_to_seconds(),16},e)&&graph.attach_scene(1,data,e)&&graph.enter(1,data.regions()[0].source_path,context,e)&&routes.reconcile(graph.players(),{{0,1,1}},e),"source graph first region");
 check(service.check_routes(routes,e)&&mutations+owner_calls==calls&&service.sync_routes(routes,e)&&queues[4].size()==3&&queues[0]==root_queue,"coordinated first submission preserves existing owner");
 auto first_queue=queues[4];calls=mutations+owner_calls;
 check(!service.cancel(1,e)&&!service.publish(1,routes,e)&&service.prepare_step(1,8192,e)==ctr::MusicPreparationStep::Failed&&service.provider_count()==1&&queues[4]==first_queue&&mutations+owner_calls==calls,"published provider cannot be cancelled or prepared again");
 check(service.begin_prepare(2,argv[3],argv[4],argv[5],4,true,bank.master_db(),e)&&service.prepare_step(2,0,e)==ctr::MusicPreparationStep::Failed&&service.provider_count()==1&&mutations+owner_calls==calls,"bad budget cancels only detached candidate");
 check(!service.begin_prepare(2,argv[3],argv[4],argv[5],4,true,bank.master_db(),e),"failed identity cannot be reused");
 check(service.begin_prepare(3,argv[3],argv[4],argv[5],4,true,bank.master_db(),e),"next candidate while source streams");
 drain(4);check(service.pump_streams(e)&&queues[4].size()==3&&queues[0]==root_queue,"old provider refilled while next validates");
 calls=mutations+owner_calls;first_queue=queues[4];check(complete(service,3,e)&&service.publish(3,routes,e)&&mutations+owner_calls==calls&&queues[4]==first_queue,"next validated provider publishes silently");
 for(auto it=graph.data().regions().rbegin();it!=graph.data().regions().rend();++it)check(graph.tree_exit(1,it->source_path,e),"source tree exit order");
 check(graph.finish_scene(1,e)&&graph.attach_scene(2,*service.data(3),e),"scene data rebound over same music players");
 const upstream::MusicRegionBinding*other=nullptr;for(const auto&r:graph.data().regions())if(r.track_id!=data.regions()[0].track_id&&r.appear_flag.empty()&&r.disappear_flag.empty())other=&r;
 check(other&&graph.enter(2,other->source_path,context,e)&&routes.reconcile(graph.players(),{{1,1,3}},e),"different source track uses new prepared provider");
 check(service.check_routes(routes,e)&&service.sync_routes(routes,e)&&queues[4]==first_queue&&queues[5].size()==3&&queues[0]==root_queue,"both global voices survive coordinated sync");
 auto old_queues=queues;calls=mutations+owner_calls;
 auto unknown=routes;check(unknown.add_provider(4,data,bank,e),"unprepared metadata-only negative provider");
 check(!service.check_routes(unknown,e)&&!service.sync_routes(unknown,e)&&!service.retire(1,routes,e)&&queues==old_queues&&mutations+owner_calls==calls,"all-provider preflight and live-retirement failure preserve queues");
 check(graph.advance(1,false,e)&&graph.advance(.5,false,e)&&graph.players().cleanup_waiting()&&graph.players().find(0),"source waits for the other region's longer fadein");
 double remaining=0;for(const auto&node:graph.players().nodes())if(node.tween.active)remaining=std::max(remaining,node.tween.duration-node.tween.elapsed);
 check(remaining>0,"source-selected long fadein remains active");while(remaining>0){const double delta=std::min(1.,remaining);check(graph.advance(delta,false,e),"source clock input");remaining-=delta;}
 check(graph.flush_deletions(e)&&routes.reconcile(graph.players(),{},e),"all source tweens completed before cleanup");
 check(service.sync_routes(routes,e)&&queues[4].empty()&&queues[5]==old_queues[5]&&service.retire(1,routes,e)&&service.provider_count()==1&&!routes.provider(1),"only unused old bank retires");
 check(!service.begin_prepare(3,argv[3],argv[4],argv[5],4,true,bank.master_db(),e),"retained provider identities stay monotonic");
 // The caller snapshots metadata it needs beyond a provider retirement.
 const float master=service.bank(3)->master_db();
 check(service.begin_prepare(4,argv[3],argv[4],argv[5],4,true,master,e)&&service.set_paused(true,e)&&paused_channels[5]&&!paused_channels[0],"pause owned channels while detached preparation proceeds");
 old_queues=queues;calls=mutations+owner_calls;check(complete(service,4,e)&&mutations+owner_calls==calls&&service.publish(4,routes,e)&&service.retire(4,routes,e)&&queues==old_queues,"unstarted paused provider validates/retires without NDSP");
 check(service.set_user_music_db(-12,e)&&service.sync_routes(routes,e)&&queues==old_queues&&paused_channels[5],"menu volume preserves pause and queues");
 calls=mutations+owner_calls;check(service.pump_streams(e)&&mutations+owner_calls==calls,"paused coordinator pump is silent");check(!service.set_user_music_db(NAN,e)&&service.set_paused(false,e),"volume validation and resume");
 upstream::MusicPlayers new_game;upstream::MusicBankRoutes replacement;
 check(new_game.initialize({data.silence_db(),0,data.fade_to_seconds(),16},e)&&new_game.play_loop(0,graph.data().tracks()[0].source_path,e)&&replacement.initialize({4,16,master,graph.data().silence_db()},e)&&replacement.add_provider(3,*service.data(3),*service.bank(3),e)&&replacement.reconcile(new_game,{{0,1,3}},e),"independent new-game numeric identity fixture");
 old_queues=queues;calls=mutations+owner_calls;check(!service.sync_routes(replacement,e)&&queues==old_queues&&mutations+owner_calls==calls,"wrong global player owner rejected before any provider moves");
 service.shutdown();check(service.provider_count()==0&&queues[5].empty()&&queues[0]==root_queue,"global shutdown preserves existing NDSP owner");owner.shutdown();check(linear_bytes==0,"all real buffers released");
 std::cout<<"Global music bank transports PASS "<<checks<<" checks; prepared publication, ordered release/sync, tails, failure/cancel, pause/menu and retirement under NDSP double; no live scene/audio claim\n";
}
