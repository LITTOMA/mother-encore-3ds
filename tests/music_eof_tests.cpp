#include "music_bank_transports.hpp"
#include "audio_player.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace encore;
namespace {
unsigned checks=0,mutations=0,owner_calls=0,file_opens=0;
size_t linear_bytes=0;bool dsp=false;
std::array<uint64_t,20>frames_added{};std::array<uint32_t,20>resets{};bool partial_wave=false;
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
void ndspChnReset(int i){channel(i);++resets[size_t(i)];drain(i);}void ndspChnWaveBufClear(int i){channel(i);drain(i);}
void ndspChnSetMix(int i,float*v){channel(i);check(v[0]==v[1],"stereo mix unchanged");}
Result DSP_FlushDataCache(const void*p,uint32_t n){check(p&&n&&n<=8192,"checked streaming payload");return 0;}
void ndspChnWaveBufAdd(int i,ndspWaveBuf*p){check(dsp,"NDSP owner still alive");check(p->nsamples>0&&p->nsamples<=2048,"bounded partial/full music buffer");partial_wave|=p->nsamples<2048;frames_added[size_t(i)]+=p->nsamples;p->status=NDSP_WBUF_QUEUED;queues[channel(i)].push_back(p);}
void ndspChnSetPaused(int lane,bool paused){paused_channels[channel(lane)]=paused;++mutations;}
void ndspChnSetInterp(int i,int){channel(i);}void ndspChnSetRate(int i,float){channel(i);}void ndspChnSetFormat(int i,int){channel(i);}
bool prepare(ctr::MusicBankTransports&service,uint64_t id,const char*metadata,const char*bank,const char*root,float master,std::string&e){
 if(!service.begin_prepare(id,metadata,bank,root,4,true,master,e))return false;
 for(unsigned i=0;i<20000;++i){const auto step=service.prepare_step(id,8192,e);if(step==ctr::MusicPreparationStep::Ready)return true;if(step==ctr::MusicPreparationStep::Failed)return false;}
 check(false,"bounded real PCM preparation terminates");return false;
}
bool prepare_library(ctr::MusicBankTransports&service,uint64_t id,const upstream::MusicLibraryBindings&library,uint32_t track,const char*root,float master,std::string&e){
 if(!service.begin_library(id,library,track,root,4,true,master,e))return false;
 for(unsigned i=0;i<20000;++i){const auto step=service.prepare_step(id,8192,e);if(step==ctr::MusicPreparationStep::Ready)return true;if(step==ctr::MusicPreparationStep::Failed)return false;}
 check(false,"bounded checked library preparation terminates");return false;
}
ctr::MusicBankFinished finish(ctr::MusicBankTransports&service,int lane,std::string&e){
 for(unsigned i=0;i<10000;++i){drain(lane);check(service.pump_streams(e),e.c_str());auto events=service.take_finished();if(!events.empty()){check(events.size()==1,"one source completed");return events[0];}}
 check(false,"source frames must complete within declared bound");return {};
}
int main(int argc,char**argv){
 check(argc==12,"opening bank/root, source metadata/banks and checked library descriptor/root");std::string e;
 upstream::MusicRegionData library;check(library.load_file(argv[3],e)&&library.library(),e.c_str());
 ctr::AudioPlayer owner;check(owner.initialize(argv[1],argv[2],e),e.c_str());
 ctr::MusicBankTransports service;upstream::MusicBankRoutes routes;upstream::MusicPlayers graph;
 check(service.initialize(4,0,e)&&graph.initialize({library.silence_db(),library.empty_player_gain_db(),library.fade_to_seconds(),16},e),"source library policy initialized");
 upstream::AudioBank original;upstream::MusicLibraryBindings bindings;check(original.load_file(argv[1],e)&&bindings.load_file(argv[10],e)&&bindings.admit(argv[11],original,e),"checked source library file mappings admitted");
 upstream::MusicRegionData im,hm,vm;check(im.load_file(argv[4],e)&&hm.load_file(argv[6],e)&&vm.load_file(argv[8],e),"three source track identities from their checked metadata");
 const auto opens_before_bad=file_opens;upstream::MusicLibraryBindings absent;
 check(!service.begin_library(1,absent,im.tracks()[0].id,argv[2],4,true,owner.observe_music().master_db,e)&&!service.begin_library(1,bindings,0,argv[2],4,true,owner.observe_music().master_db,e)&&!service.begin_library(1,bindings,im.tracks()[0].id,argv[2],0,true,owner.observe_music().master_db,e)&&file_opens==opens_before_bad&&service.provider_count()==0,"unadmitted/unknown/bad-capacity library requests reject before file or DSP mutation");
 const auto calls=mutations+owner_calls;
 check(prepare_library(service,1,bindings,im.tracks()[0].id,argv[2],owner.observe_music().master_db,e)&&prepare_library(service,2,bindings,hm.tracks()[0].id,argv[2],owner.observe_music().master_db,e)&&prepare_library(service,3,bindings,vm.tracks()[0].id,argv[2],owner.observe_music().master_db,e),e.c_str());
 check(mutations+owner_calls==calls,"three source preparations are zero NDSP");
 const auto intro=*service.data(1),house=*service.data(2),victory=*service.data(3);const auto ia=service.bank(1)->asset(0),ha=service.bank(2)->asset(0),va=service.bank(3)->asset(0);
 // Copy the URIs whose bank owners will be explicitly retired below.
 const std::string intro_uri=intro.tracks()[0].source_path,house_uri=house.tracks()[0].source_path,victory_uri=victory.tracks()[0].source_path;
 const uint32_t intro_frames=ia.frames,house_frames=ha.frames,victory_frames=va.frames;
 check(!ia.loops()&&ha.loops()&&!va.loops(),"original builtin loop policies");
 check(owner.play(ha.stable_id,ctr::AudioLane::Music,e),"independent existing owner fixture");auto root_queue=queues[0];
 check(routes.initialize({4,16,owner.observe_music().master_db,library.silence_db()},e)&&service.publish(1,routes,e)&&service.publish(2,routes,e)&&service.publish(3,routes,e),"three prepared source providers published");
 check(graph.play_track(0,intro_uri,house_uri,e)&&routes.reconcile(graph,{{0,1,1}},e)&&service.sync_routes(routes,e)&&queues[4].size()==3,"non-loop intro starts");
 const auto event=finish(service,4,e);check(event.provider==1&&event.player==0&&event.generation==1,"physical EOF keeps source identity");
 check(frames_added[4]==intro_frames&&queues[4].empty()&&partial_wave,"EOF follows exactly the source frames including its partial tail");
 auto old_resets=resets;auto old_frames=frames_added;
 for(unsigned i=0;i<10;++i)check(service.sync_routes(routes,e)&&service.pump_streams(e)&&service.take_finished().empty(),"unconsumed completion is latched");
 check(resets==old_resets&&frames_added==old_frames&&queues[0]==root_queue,"repeated sync never restarts completed generation or touches existing music");
 check(graph.signal_finished(event.player,event.generation,e)&&graph.dispatch_finished(e)&&graph.flush_deletions(e)&&graph.find(0)->generation==2&&graph.find(0)->stream==house_uri,"source deferred intro-loop callback");
 check(routes.reconcile(graph,{{0,2,2}},e)&&service.sync_routes(routes,e)&&queues[4].size()==3,"new generation changes prepared provider");
 const auto house_start=frames_added[4];
 for(unsigned i=0;i<house_frames/(3*2048)+4;++i){drain(4);check(service.pump_streams(e)&&service.take_finished().empty(),"builtin source loop emits no finished callback");}
 check(frames_added[4]-house_start>house_frames&&graph.find(0)->generation==2,"builtin loop wraps without replaying manager call");
 // Original pause_all affects existing nodes. A later same-bank node remains
 // unpaused; routing must preserve both independent stream positions/queues.
 check(graph.pause_all(e)&&routes.reconcile(graph,{},e)&&service.sync_routes(routes,e)&&paused_channels[4]&&!paused_channels[0],"source pause reaches only retained regional voice");
 auto paused_queue=queues[4];auto paused_frames=frames_added[4];auto paused_resets=resets[4];
 for(unsigned i=0;i<5;++i)check(service.pump_streams(e)&&service.take_finished().empty(),"paused source produces neither refill nor EOF");
 check(queues[4]==paused_queue&&frames_added[4]==paused_frames&&resets[4]==paused_resets,"pause retains queued samples and exact cursor");
 uint64_t peer;check(graph.add_player(peer,e)&&graph.play_loop(peer,house_uri,e)&&routes.reconcile(graph,{{peer,1,2}},e)&&service.sync_routes(routes,e),"new source node on same checked provider");
 check(paused_channels[4]&&!paused_channels[5]&&queues[5].size()==3,"existing and newly created same-bank players have independent pause flags");
 check(service.set_paused(true,e)&&paused_channels[4]&&paused_channels[5],"physical lifecycle override pauses both voices");
 check(service.set_paused(false,e)&&paused_channels[4]&&!paused_channels[5],"clearing physical override preserves source pause on older node");
 check(service.set_user_music_db(-12,e)&&service.sync_routes(routes,e)&&paused_channels[4]&&queues[4]==paused_queue,"settings mix changes preserve source pause and queue");
 check(graph.resume_all(e)&&routes.reconcile(graph,{},e)&&service.sync_routes(routes,e)&&!paused_channels[4]&&!paused_channels[5],"source resume changes existing voice flags");
 check(queues[4]==paused_queue&&frames_added[4]==paused_frames&&resets[4]==paused_resets,"resume never restarts old stream or advances queued cursor");
 check(graph.queue_remove(peer,e)&&graph.flush_deletions(e)&&routes.reconcile(graph,{},e)&&service.sync_routes(routes,e)&&!paused_channels[5],"deleting peer releases only its channel");
 const auto victory_baseline=frames_added[5];
 uint64_t second;check(graph.add_player(second,e)&&second==peer+1&&graph.play_track(second,victory_uri,victory_uri,e)&&routes.reconcile(graph,{{second,1,3}},e)&&service.sync_routes(routes,e),"callback-loop victory source starts");
 const auto initial_chunk=frames_added[5]-victory_baseline;check(initial_chunk>0,"victory initial buffers");
 const auto first_wave=upstream::audio_crc32(reinterpret_cast<const uint8_t*>(queues[5][0]->data_pcm16),queues[5][0]->nsamples*va.channels*2);
 const auto v1=finish(service,5,e);check(v1.player==second&&v1.generation==1&&frames_added[5]-victory_baseline==victory_frames,"callback body ends after one full source, not a builtin loop");
 check(service.sync_routes(routes,e)&&service.take_finished().empty()&&frames_added[5]-victory_baseline==victory_frames,"body also stays latched before callback");
 check(graph.signal_finished(second,1,e)&&graph.dispatch_finished(e)&&routes.reconcile(graph,{{second,2,3}},e)&&service.sync_routes(routes,e),"same URI callback publishes another playback generation");
 check(upstream::audio_crc32(reinterpret_cast<const uint8_t*>(queues[5][0]->data_pcm16),queues[5][0]->nsamples*va.channels*2)==first_wave,"callback restarts at source frame zero");
 const auto v2=finish(service,5,e);check(v2.player==second&&v2.generation==2&&frames_added[5]-victory_baseline==uint64_t(victory_frames)*2,"exact second callback cycle");
 check(graph.stop(second,e)&&graph.signal_finished(second,2,e)&&graph.dispatch_finished(e)&&graph.deferred_finished_count()==0&&routes.reconcile(graph,{},e)&&service.sync_routes(routes,e),"stop before signal disconnects source loop");
 uint64_t once;check(graph.add_player(once,e)&&graph.play_track(once,intro_uri,{},e)&&routes.reconcile(graph,{{once,1,1}},e)&&service.sync_routes(routes,e),"one-shot source on retained provider");
 const auto only=finish(service,5,e);check(only.player==once&&only.generation==1,"one-shot EOF");
 check(graph.signal_finished(once,1,e)&&graph.find(once)->pending_delete&&graph.dispatch_finished(e)&&graph.flush_deletions(e)&&!graph.find(once)&&routes.reconcile(graph,{},e)&&service.sync_routes(routes,e),"one-shot source removes its node after signal boundary");
 check(service.retire(1,routes,e)&&service.retire(3,routes,e)&&service.provider_count()==1,"finished source banks retire without affecting current loop");
 // Reverse provider grouping: child3 uses provider5; child4 uses provider4.
 // Completion delivery follows source child insertion order, not bank order.
 check(prepare(service,4,argv[4],argv[5],argv[2],owner.observe_music().master_db,e)&&prepare(service,5,argv[4],argv[5],argv[2],owner.observe_music().master_db,e)&&service.publish(4,routes,e)&&service.publish(5,routes,e),"two source providers for completion order");
 uint64_t third,fourth;check(graph.add_player(third,e)&&graph.play_track(third,intro_uri,{},e)&&graph.add_player(fourth,e)&&graph.play_track(fourth,intro_uri,{},e)&&routes.reconcile(graph,{{third,1,5},{fourth,1,4}},e)&&service.sync_routes(routes,e),"source child order differs from provider iteration");
 for(unsigned i=0;i<intro_frames/(3*2048)+4;++i){drain(5);drain(6);check(service.pump_streams(e),e.c_str());}
 const auto paired=service.take_finished();check(paired.size()==2&&paired[0].player==third&&paired[0].provider==5&&paired[1].player==fourth&&paired[1].provider==4,"completed source nodes delivered in insertion order");
 check(service.take_finished().empty()&&queues[0]==root_queue,"completion tokens consumed once, existing owner intact");
 service.shutdown();owner.shutdown();check(linear_bytes==0,"all buffers released after EOF and loops");

 std::cout<<"Music natural EOF NDSP-double PASS "<<checks<<" checks; exact source frames/partial tails, one delivery, latched completed generations, intro/builtin/callback loops, one-shot cleanup and source order; no real device timing/audible-output claim\n";
}
