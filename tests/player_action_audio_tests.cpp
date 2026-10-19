#include "player_action_audio.hpp"
#include "encore/resource_catalog.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <map>
#include <vector>
using namespace encore;
namespace {unsigned checks=0,calls=0;bool live=false,fail_alloc=false;Result flush_result=0;size_t allocated=0;std::map<void*,size_t>allocations;std::string error;
std::array<std::vector<ndspWaveBuf*>,24>queues;std::array<float,24>gains{};std::array<bool,24>paused{};
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Player NDSP %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
size_t channel(int i){CHECK(i>=0&&i<24);++calls;return size_t(i);}
void drain(int i){for(auto*b:queues[size_t(i)])b->status=NDSP_WBUF_DONE;queues[size_t(i)].clear();}
}
Result ndspInit(){live=true;return 0;}void ndspExit(){for(int i=0;i<24;++i)drain(i);live=false;}
void ndspSetMasterVol(float){++calls;}uint32_t ndspGetDroppedFrames(){return 0;}
void*linearAlloc(size_t n){if(fail_alloc)return nullptr;auto*p=std::malloc(n);if(p){allocations[p]=n;allocated+=n;}return p;}
void linearFree(void*p){CHECK(allocations.count(p));allocated-=allocations[p];allocations.erase(p);std::free(p);}
void ndspChnReset(int i){ndsp_test_reset_sequence(i);channel(i);drain(i);}void ndspChnWaveBufClear(int i){channel(i);drain(i);}
void ndspChnSetMix(int i,float*v){gains[channel(i)]=v[0];CHECK(v[0]==v[1]);}void ndspChnSetPaused(int i,bool v){paused[channel(i)]=v;}
void ndspChnSetInterp(int i,int){channel(i);}void ndspChnSetRate(int i,float){channel(i);}void ndspChnSetFormat(int i,int){channel(i);}
Result DSP_FlushDataCache(const void*p,uint32_t n){CHECK(p&&n&&n<=8192);return flush_result;}
void ndspChnWaveBufAdd(int i,ndspWaveBuf*b){ndsp_test_assign_sequence(i,b);CHECK(live&&b->data_pcm16&&b->nsamples&&b->nsamples<=2048);queues[channel(i)].push_back(b);b->status=NDSP_WBUF_QUEUED;}
int main(int argc,char**argv){
 CHECK(argc==4);const std::string root=argv[1];upstream::ResourceCatalog catalog;CHECK(catalog.load_file((root+"/system/native.encresources").c_str(),error));upstream::FieldSwingResources resources;CHECK(resources.load(catalog,root.c_str(),error));upstream::RoomData room;CHECK(room.load_file((root+"/world/house/opening.encroom").c_str(),error));
 ctr::AudioPlayer player;CHECK(!player.prepare_player_voice(error));CHECK(player.initialize(argv[2],argv[3],error));std::vector<uint32_t>loops;for(uint32_t i=0;i<player.bank().count();++i)if(player.bank().asset(i).loops())loops.push_back(player.bank().asset(i).stable_id);CHECK(loops.size()>=2);CHECK(player.play(loops[0],ctr::AudioLane::Music,error)&&player.play(loops[1],ctr::AudioLane::Run,error));const auto music=queues[0],run=queues[4];const auto music_generation=player.observe_music().generation;const auto music_gain=gains[0],run_gain=gains[4];
 ctr::PlayerActionAudioService service;upstream::FieldSwingResources empty;CHECK(!service.prepare(empty,player,error)&&!service.prepared());
 {upstream::ResourceCatalog old;upstream::FieldSwingResources old_resources;CHECK(old.load_file((root+"/system/legacy-action.encresources").c_str(),error)&&old_resources.load(old,root.c_str(),error));CHECK(old_resources.action().capability()==1&&!service.prepare(old_resources,player,error)&&!service.prepared());}
 int foreign=0;auto&pool=ctr::region_channel_pool::owners();for(auto&owner:pool)if(!owner)owner=&foreign;const auto full=queues;const auto before=calls;CHECK(!service.prepare(resources,player,error)&&!service.prepared()&&queues==full&&calls==before);for(auto&owner:pool)if(owner==&foreign)owner=nullptr;
 fail_alloc=true;CHECK(!service.prepare(resources,player,error)&&!service.prepared()&&queues==full);fail_alloc=false;CHECK(service.prepare(resources,player,error)&&service.prepared());CHECK(!service.prepare(resources,player,error));CHECK(pool[0]==&player&&pool[1]&&pool[1]!=&player&&queues[0]==music&&queues[4]==run);const auto retained=allocated;CHECK(player.prepare_player_voice(error)&&allocated==retained);
 upstream::OpeningWorld world;CHECK(world.initialize(room.view())&&world.bind_player_action(resources,error));bool accepted=false;CHECK(world.player_action_input(resources.action().strings()[2],true,true,false,false,accepted,error)&&accepted);
 fail_alloc=true;CHECK(service.sync(world,player,error)&&service.starts()==1&&queues[5].size()==3);fail_alloc=false;CHECK(queues[0]==music&&queues[4]==run&&gains[0]==music_gain&&gains[4]==run_gain&&player.observe_music().generation==music_generation);CHECK(player.lane_asset_id(ctr::AudioLane::Player)==service.sound_id());
 const auto no_event=calls;CHECK(service.sync(world,player,error)&&service.starts()==1&&calls==no_event);
 // Bat ends before its source sound: a second ordinary press restarts only
 // that persistent source voice, without allocation or a BGM/run mutation.
 for(unsigned tick=0;tick<60&&world.player_action().attacking();++tick)CHECK(world.advance({})&&world.idle_frame(1.0/60)&&world.end_scene_frame());
 CHECK(!world.player_action().attacking()&&player.playing(ctr::AudioLane::Player));const auto first_sample=queues[5][0]->data_pcm16[0];
 CHECK(world.player_action_input(resources.action().strings()[2],true,true,false,false,accepted,error)&&accepted);fail_alloc=true;CHECK(service.sync(world,player,error)&&service.starts()==2&&allocated==retained);fail_alloc=false;CHECK(queues[5].size()==3&&queues[5][0]->data_pcm16[0]==first_sample&&queues[0]==music&&queues[4]==run);
 CHECK(player.set_user_volume(0,0,-6,error));CHECK(gains[0]==music_gain&&std::abs(gains[5]-std::pow(10.0f,-6.0f/20))<.000001f&&std::abs(gains[4]-run_gain*std::pow(10.0f,-6.0f/20))<.000001f);CHECK(queues[0]==music&&queues[4]==run&&player.set_user_volume(0,0,0,error));
 CHECK(world.pause_for_house()&&service.sync(world,player,error));CHECK(service.paused()&&!player.playing(ctr::AudioLane::Player)&&queues[5].empty()&&queues[0]==music&&queues[4]==run);
 CHECK(world.unpause_from_house()&&world.player_action_input(resources.action().strings()[2],true,true,false,false,accepted,error)&&accepted);CHECK(service.sync(world,player,error)&&service.starts()==3&&!service.paused()&&queues[5].size()==3);
 CHECK(world.pause_for_house()&&world.unpause_from_house()&&world.player_action_input(resources.action().strings()[2],true,true,false,false,accepted,error)&&accepted);flush_result=-1;CHECK(!service.sync(world,player,error)&&service.consumed_events()==2&&service.starts()==3&&service.pending_events().size()==3);CHECK(queues[0]==music&&queues[4]==run);flush_result=0;CHECK(service.sync(world,player,error)&&service.starts()==4&&service.pending_events().empty());
 // Reject the entire bad batch before even a preceding valid source pause.
 auto&bad=const_cast<std::vector<upstream::PlayerActionEvent>&>(service.pending_events());bad.push_back(upstream::PlayerActionEvent::Pause);bad.push_back(upstream::PlayerActionEvent(99));const auto intact=queues;const auto intact_calls=calls;CHECK(!service.sync(world,player,error)&&queues==intact&&calls==intact_calls&&!service.paused());bad.clear();bad.push_back(upstream::PlayerActionEvent::Pause);bad.push_back(upstream::PlayerActionEvent::Start);CHECK(!service.sync(world,player,error)&&queues==intact&&calls==intact_calls);bad.clear();
 ctr::AudioPlayer wrong;CHECK(!service.sync(world,wrong,error)&&queues==intact);upstream::OpeningWorld different;CHECK(different.initialize(room.view())&&different.bind_player_action(resources,error));CHECK(!service.sync(different,player,error)&&queues==intact);
 CHECK(world.pause_for_house()&&service.sync(world,player,error));upstream::OpeningWorld next;CHECK(next.initialize(room.view())&&next.pause_for_house());upstream::ScenePlayerTransfer transfer;CHECK(next.prepare_scene_player(world,world.player().position,{},transfer,error)&&next.commit_scene_player(world,std::move(transfer),error));CHECK(!world.healthy()&&next.player_action_resources()==&resources);CHECK(next.unpause_from_house()&&next.player_action_input(resources.action().strings()[2],true,true,false,false,accepted,error)&&accepted);CHECK(service.sync(next,player,error)&&service.starts()==5&&queues[0]==music&&queues[4]==run);
 bool nonzero=false;for(unsigned step=0;step<100&&player.playing(ctr::AudioLane::Player);++step){for(auto*b:queues[5])for(uint32_t i=0;i<b->nsamples;++i)nonzero=nonzero||b->data_pcm16[i]!=0;drain(5);CHECK(player.update(0,error));}CHECK(nonzero&&!player.playing(ctr::AudioLane::Player)&&queues[0]==music&&queues[4]==run&&player.observe_music().generation==music_generation);
 player.reset_scene();const auto after_scene=calls;fail_alloc=true;CHECK(service.sync(next,player,error)&&service.starts()==5&&allocated==retained&&calls>after_scene);fail_alloc=false;CHECK(pool[0]&&pool[0]!=&player&&!player.playing(ctr::AudioLane::Player));
 CHECK(service.reset_player(player,error)&&service.starts()==0);const auto generation=player.configuration_generation();CHECK(player.initialize(argv[2],argv[3],error)&&player.configuration_generation()>generation);CHECK(!service.sync(next,player,error));player.shutdown();CHECK(!allocated&&allocations.empty()&&!live);for(auto owner:pool)CHECK(!owner);
 std::printf("Player action NDSP: %u checks; actual source PCM, preloaded private channel, BGM/run preservation, stop/resume/retrigger, retry/negative batches and persistent scene handoff; NDSP double, no audible/hardware claim\n",checks);
}
