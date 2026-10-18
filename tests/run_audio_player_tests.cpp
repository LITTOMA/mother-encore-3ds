#include "audio_player.hpp"
#include "music_region_player.hpp"
#include <array>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <vector>
using namespace encore;
namespace {
unsigned checks=0,mutations=0;bool live=false,fail_allocation=false;size_t bytes=0;std::map<void*,size_t>allocations;std::string error;
std::array<std::vector<ndspWaveBuf*>,24>queues;std::array<float,24>gains{};
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Run NDSP %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
size_t channel(int i){CHECK(i>=0&&i<24);++mutations;return size_t(i);}
void drain(int i){for(auto*b:queues[size_t(i)])b->status=NDSP_WBUF_DONE;queues[size_t(i)].clear();}
}
Result ndspInit(){live=true;return 0;}void ndspExit(){for(int i=0;i<24;++i)drain(i);live=false;}
void ndspSetMasterVol(float){++mutations;}uint32_t ndspGetDroppedFrames(){return 0;}
void*linearAlloc(size_t n){if(fail_allocation)return nullptr;auto*p=std::malloc(n);if(p){bytes+=n;allocations[p]=n;}return p;}
void linearFree(void*p){CHECK(allocations.count(p));bytes-=allocations[p];allocations.erase(p);std::free(p);}
void ndspChnReset(int i){channel(i);drain(i);}void ndspChnWaveBufClear(int i){channel(i);drain(i);}
void ndspChnSetMix(int i,float*g){gains[channel(i)]=g[0];CHECK(g[0]==g[1]);}
void ndspChnSetPaused(int i,bool){channel(i);}void ndspChnSetInterp(int i,int){channel(i);}void ndspChnSetRate(int i,float){channel(i);}void ndspChnSetFormat(int i,int){channel(i);}
Result DSP_FlushDataCache(const void*p,uint32_t n){CHECK(p&&n&&n<=8192);return 0;}
void ndspChnWaveBufAdd(int i,ndspWaveBuf*b){CHECK(live&&b->data_pcm16&&b->nsamples&&b->nsamples<=2048);queues[channel(i)].push_back(b);b->status=NDSP_WBUF_QUEUED;}
int main(int argc,char**argv){
 CHECK(argc==6);ctr::AudioPlayer owner;upstream::AudioBank bank;CHECK(bank.load_file(argv[1],error));CHECK(owner.initialize(argv[1],argv[2],error));std::vector<uint32_t>loops;uint32_t one_shot=0;for(uint32_t i=0;i<bank.count();++i){auto a=bank.asset(i);if(a.loops())loops.push_back(a.stable_id);else if(!one_shot)one_shot=a.stable_id;}CHECK(loops.size()>=3&&one_shot);
 CHECK(owner.prepare(loops[0],error)&&owner.prepare(loops[1],error)&&owner.prepare(loops[2],error)&&owner.play(loops[0],ctr::AudioLane::Music,error));const auto ordinary=queues;auto music_generation=owner.observe_music().generation;
 CHECK(!owner.play(one_shot,ctr::AudioLane::Run,error)&&queues==ordinary);const auto before_failure=mutations;fail_allocation=true;CHECK(!owner.play(loops[1],ctr::AudioLane::Run,error)&&queues==ordinary&&mutations==before_failure);fail_allocation=false;
 for(auto p:ctr::region_channel_pool::owners())CHECK(!p);
 upstream::MusicRegionData data;CHECK(data.load_file(argv[3],error));ctr::MusicRegionPlayer regions;CHECK(regions.begin_prepare(argv[4],argv[5],data,16,true,bank.master_db(),error));auto phase=ctr::MusicPreparationStep::Progress;while(phase==ctr::MusicPreparationStep::Progress)phase=regions.prepare_step(8192,error);CHECK(phase==ctr::MusicPreparationStep::Ready);
 upstream::MusicRegionController controller;upstream::MusicRegionContext context;context.flag=[](std::string_view){return false;};CHECK(controller.initialize(data,16,error)&&controller.attach_scene(1,error)&&controller.observe_external_player({1,true,true},error)&&controller.enter(1,"Music/MusicArea",context,error)&&regions.sync(controller,error));CHECK(queues[4].size()==3);const auto region_queue=queues[4];
 CHECK(owner.play(loops[1],ctr::AudioLane::Run,error)&&queues[5].size()==3&&queues[4]==region_queue&&queues[0]==ordinary[0]);CHECK(ctr::region_channel_pool::owners()[0]==&regions&&ctr::region_channel_pool::owners()[1]==&owner);
 owner.reset_scene();CHECK(queues[4]==region_queue&&queues[5].empty()&&!ctr::region_channel_pool::owners()[1]&&!owner.playing(ctr::AudioLane::Run));CHECK(owner.play(loops[0],ctr::AudioLane::Music,error)&&owner.play(loops[1],ctr::AudioLane::Run,error));music_generation=owner.observe_music().generation;
 CHECK(owner.play(loops[2],ctr::AudioLane::Run,error)&&queues[5].size()==3&&queues[4]==region_queue&&queues[0]==ordinary[0]);CHECK(owner.lane_asset_id(ctr::AudioLane::Run)==loops[2]&&owner.observe_music().generation==music_generation);
 CHECK(owner.set_user_volume(0,-3,-6,error)&&gains[5]<gains[0]);CHECK(owner.pause_lane(ctr::AudioLane::Run,true,error)&&owner.pause_lane(ctr::AudioLane::Run,false,error));CHECK(owner.stop_lane(ctr::AudioLane::Run,error)&&queues[5].empty()&&queues[4]==region_queue&&!ctr::region_channel_pool::owners()[1]);
 CHECK(owner.play(loops[1],ctr::AudioLane::Run,error)&&queues[5].size()==3);CHECK(controller.tree_exit(1,"Music/MusicArea",error));
 for(uint64_t n=2;n<=15;++n){CHECK(controller.observe_external_player({n,true,true},error)&&controller.enter(1,"Music/MusicArea",context,error)&&regions.sync(controller,error)&&controller.tree_exit(1,"Music/MusicArea",error));}
 CHECK(controller.observe_external_player({16,true,true},error)&&controller.enter(1,"Music/MusicArea",context,error));const auto full_queues=queues;const auto full_calls=mutations;CHECK(!regions.sync(controller,error)&&queues==full_queues&&mutations==full_calls&&owner.playing(ctr::AudioLane::Run));
 CHECK(owner.stop_lane(ctr::AudioLane::Run,error)&&regions.sync(controller,error));const auto full_music=queues;const auto no_slot_calls=mutations;CHECK(!owner.play(loops[1],ctr::AudioLane::Run,error)&&queues==full_music&&mutations==no_slot_calls&&owner.observe_music().generation==music_generation);
 regions.shutdown();CHECK(owner.play(loops[1],ctr::AudioLane::Run,error)&&queues[4].size()==3);owner.reset_scene();CHECK(!owner.playing(ctr::AudioLane::Run));for(auto p:ctr::region_channel_pool::owners())CHECK(!p);CHECK(owner.play(loops[2],ctr::AudioLane::Run,error)&&queues[4].size()==3);owner.shutdown();CHECK(!live&&!bytes&&allocations.empty());for(auto p:ctr::region_channel_pool::owners())CHECK(!p);for(const auto&q:queues)CHECK(q.empty());
 std::printf("Run/region NDSP ownership: %u checks; independent loop, music preservation, replacement, capacity failure, reuse, reset and shutdown; API double only\n",checks);
}
