// Actual source PCM and production queues; deliberate host-only NDSP double.
#include "dungeon_mechanism_audio.hpp"
#include "encore/content.hpp"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
using namespace encore;
namespace {
unsigned checks=0;std::string error;bool fail_alloc=false;Result flush_result=0;size_t allocated=0;std::map<void*,size_t>allocations;
std::array<std::vector<ndspWaveBuf*>,24>queues;std::array<float,24>gains{};std::array<unsigned,24>resets{};
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Mechanism NDSP %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
size_t channel(int c){CHECK(c>=0&&c<24);return size_t(c);}
void drain(int c){for(auto*w:queues[channel(c)])w->status=NDSP_WBUF_DONE;queues[channel(c)].clear();}
void complete(int c){auto&q=queues[channel(c)];CHECK(!q.empty());q.front()->status=NDSP_WBUF_DONE;q.erase(q.begin());}
void write(const std::filesystem::path&p,const std::vector<uint8_t>&b){std::filesystem::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));CHECK(bool(f));}
}
Result ndspInit(){return 0;}void ndspExit(){for(int i=0;i<24;++i)drain(i);}void ndspSetMasterVol(float){}
void ndspChnReset(int c){++resets[channel(c)];ndsp_test_reset_sequence(c);drain(c);}void ndspChnWaveBufClear(int c){drain(c);}void ndspChnSetPaused(int c,bool){channel(c);}
void*linearAlloc(size_t n){if(fail_alloc)return nullptr;void*p=std::malloc(n);if(p){allocations[p]=n;allocated+=n;}return p;}
void linearFree(void*p){auto it=allocations.find(p);CHECK(it!=allocations.end());const auto first=reinterpret_cast<uintptr_t>(p),end=first+it->second;for(const auto&q:queues)for(auto*w:q){const auto sample=reinterpret_cast<uintptr_t>(w->data_pcm16);CHECK(sample<first||sample>=end);}allocated-=it->second;allocations.erase(it);std::free(p);}
Result DSP_FlushDataCache(const void*p,uint32_t n){CHECK(p&&n&&n<=512*1024);return flush_result;}
void ndspChnSetMix(int c,float*mix){CHECK(mix[0]==mix[1]);gains[channel(c)]=mix[0];}
void ndspChnSetRate(int c,float rate){channel(c);CHECK(std::isfinite(rate)&&rate>0);}void ndspChnSetFormat(int c,int){channel(c);}void ndspChnSetInterp(int c,int){channel(c);}
void ndspChnWaveBufAdd(int c,ndspWaveBuf*w){CHECK(w&&w->data_pcm16&&w->nsamples&&w->nsamples<=2048&&!w->looping);ndsp_test_assign_sequence(c,w);w->status=NDSP_WBUF_QUEUED;queues[channel(c)].push_back(w);}
uint32_t ndspGetDroppedFrames(){return 0;}uint32_t ndspChnGetSamplePos(int c){channel(c);return 0;}uint16_t ndspChnGetWaveBufSeq(int c){const auto&q=queues[channel(c)];return q.empty()?0:q.front()->sequence_id;}
int main(int argc,char**argv){
 CHECK(argc==6);upstream::DungeonMechanismPresentationData data;CHECK(data.load_file(argv[3],error)&&data.media_bound());const std::filesystem::path source=argv[4],out=argv[5];ctr::AudioPlayer player;CHECK(player.initialize(argv[1],argv[2],error));
 std::vector<std::vector<uint8_t>>pcms;size_t total=0;for(uint32_t i=0;i<data.audio_bank().count();++i){const auto asset=data.audio_bank().asset(i);std::vector<uint8_t>pcm;CHECK(read_file((source/asset.pcm_path).string().c_str(),pcm,512*1024,error));write(out/asset.pcm_path,pcm);total+=pcm.size();pcms.push_back(std::move(pcm));}
 CHECK(player.play(21,ctr::AudioLane::Music,error));CHECK(player.prepare_player_voice(error));const auto music=player.observe_music();const auto music_queue=queues[0];auto&pool=ctr::region_channel_pool::owners();const auto before_pool=pool;const auto before_bytes=allocated;
 ctr::DungeonMechanismAudioService service;ctr::AudioPlayer uninitialized;CHECK(!service.prepare(data,uninitialized,out.string().c_str(),error)&&!service.prepared());CHECK(!service.prepared()&&!service.activate(42,error)&&!service.matches(data,error));CHECK(!service.prepare(data,player,nullptr,error));
 fail_alloc=true;CHECK(!service.prepare(data,player,out.string().c_str(),error)&&!service.prepared()&&allocated==before_bytes&&pool==before_pool);fail_alloc=false;
 flush_result=-7;CHECK(!service.prepare(data,player,out.string().c_str(),error)&&!service.prepared()&&allocated==before_bytes&&pool==before_pool&&queues[0]==music_queue);flush_result=0;
 const auto first=data.audio_bank().asset(0);write(out/first.pcm_path,std::vector<uint8_t>(first.pcm_bytes,0));CHECK(!service.prepare(data,player,out.string().c_str(),error)&&!service.prepared()&&allocated==before_bytes);write(out/first.pcm_path,pcms[0]);
 CHECK(service.prepare(data,player,out.string().c_str(),error)&&service.matches(data,error)&&allocated==before_bytes+total);CHECK(!service.prepare(data,player,out.string().c_str(),error));CHECK(!service.activate(0,error)&&service.activate(42,error)&&!service.activate(43,error));
 for(uint32_t i=0;i<data.audio_bank().count();++i)CHECK(std::filesystem::remove(out/data.audio_bank().asset(i).pcm_path));
 size_t one=SIZE_MAX,two=SIZE_MAX,manager=SIZE_MAX;for(size_t i=0;i<data.sounds().size();++i){const auto&s=data.sounds()[i];if(s.kind==upstream::DungeonMechanismSoundKind::ManagerChannel)manager=i;else if(one==SIZE_MAX)one=i;else if(s.stable_id==data.sounds()[one].stable_id&&two==SIZE_MAX)two=i;}CHECK(one!=SIZE_MAX&&two!=SIZE_MAX&&manager!=SIZE_MAX);
 auto command=[&](size_t slot,bool playing,uint64_t epoch=42){const auto&s=data.sounds()[slot];return upstream::DungeonMechanismSoundCommand{epoch,uint32_t(slot),s.owner,s.kind==upstream::DungeonMechanismSoundKind::Node?s.owner:data.sounds()[one].owner,s.kind,playing};};
 CHECK(!service.reserve({},error));static int foreign;for(auto&owner:pool)if(!owner)owner=&foreign;const auto full=pool;CHECK(!service.reserve({command(one,true),command(two,true)},error)&&pool==full&&service.play_requests()==0&&queues[0]==music_queue);for(auto&owner:pool)if(owner==&foreign)owner=nullptr;CHECK(pool==before_pool);
 CHECK(service.reserve({command(one,true),command(two,true)},error)&&service.active_nodes()==0);CHECK(!service.pump_streams(error)&&!service.reserve({command(one,true)},error));service.cancel();CHECK(pool==before_pool&&service.play_requests()==0&&queues[0]==music_queue);
 CHECK(service.reserve({command(one,true),command(two,true)},error));service.commit();CHECK(service.active_nodes()==2&&service.play_requests()==2&&queues[0]==music_queue);
 int a=-1,b=-1;for(unsigned i=0;i<pool.size();++i)if(pool[i]&&pool[i]!=before_pool[i]){if(a<0)a=int(i+4);else b=int(i+4);}CHECK(a>=4&&b>a&&!queues[a].empty()&&!queues[b].empty());CHECK(queues[a][0]!=queues[b][0]&&queues[a][0]->data_pcm16==queues[b][0]->data_pcm16);const auto other=queues[b];complete(a);CHECK(service.pump_streams(error)&&queues[b]==other);
 const auto resets_b=resets[b];CHECK(service.reserve({command(one,true)},error));service.commit();CHECK(service.active_nodes()==2&&service.play_requests()==3&&resets[b]==resets_b&&queues[b]==other&&queues[0]==music_queue);
 CHECK(player.set_user_volume(0,0,-6,error)&&service.update(error));upstream::AudioAsset sound;CHECK(data.audio_bank().find(data.sounds()[one].stable_id,sound));CHECK(std::abs(gains[a]-upstream::audio_linear_gain(sound.gain_db+data.sounds()[one].gain_db-6))<.000001f);
 const auto saved_pool=pool;const auto saved_queues=queues;auto bad=command(two,true);bad.epoch=43;CHECK(!service.reserve({command(one,true),bad},error)&&pool==saved_pool&&queues==saved_queues&&service.play_requests()==3);bad=command(two,true);bad.slot=64;CHECK(!service.reserve({command(one,true),bad},error)&&pool==saved_pool&&queues==saved_queues);bad=command(two,true);bad.owner=0;CHECK(!service.reserve({command(one,true),bad},error)&&pool==saved_pool&&queues==saved_queues);bad=command(two,true);bad.kind=static_cast<upstream::DungeonMechanismSoundKind>(99);CHECK(!service.reserve({bad},error));CHECK(!service.reserve(std::vector<upstream::DungeonMechanismSoundCommand>(4097,command(one,true)),error));CHECK(pool==saved_pool&&queues==saved_queues&&service.play_requests()==3);bad=command(manager,true);bad.request_owner=0;CHECK(!service.reserve({bad},error));CHECK(!service.reserve({command(manager,false)},error));
 CHECK(service.reserve({command(one,false),command(one,true)},error));service.commit();CHECK(service.active_nodes()==2&&service.play_requests()==4&&queues[b]==other);
 CHECK(service.reserve({command(manager,true)},error));service.commit();CHECK(service.active_manager_channels()==1&&service.active_nodes()==2);int global=-1;for(unsigned i=0;i<pool.size();++i)if(pool[i]&&pool[i]!=before_pool[i]&&int(i+4)!=a&&int(i+4)!=b)global=int(i+4);CHECK(global>=4&&!queues[global].empty());const auto global_queue=queues[global];
 CHECK(service.retire_scene(42,error)&&service.epoch()==0&&service.active_nodes()==0&&service.active_manager_channels()==1&&queues[global]==global_queue&&queues[0]==music_queue);CHECK(!service.reserve({command(one,true)},error)&&!service.activate(42,error));CHECK(service.activate(43,error)&&queues[global]==global_queue);CHECK(service.reserve({command(one,true,43)},error)&&!service.retire_scene(43,error));service.cancel();CHECK(service.retire_scene(43,error)&&queues[global]==global_queue);
 for(unsigned t=0;t<128&&service.active_manager_channels();++t){if(!queues[global].empty())complete(global);CHECK(service.pump_streams(error));}CHECK(service.active_manager_channels()==0&&pool==before_pool&&queues[0]==music_queue);CHECK(player.observe_music().generation==music.generation&&player.observe_music().asset_id==music.asset_id);service.shutdown();CHECK(allocated==before_bytes&&pool==before_pool&&queues[0]==music_queue);
 for(uint32_t i=0;i<data.audio_bank().count();++i)write(out/data.audio_bank().asset(i).pcm_path,pcms[i]);CHECK(service.prepare(data,player,out.string().c_str(),error)&&service.activate(100,error)&&service.reserve({command(one,true,100)},error));service.shutdown();CHECK(allocated==before_bytes&&pool==before_pool);
 CHECK(service.prepare(data,player,out.string().c_str(),error)&&service.activate(101,error)&&service.reserve({command(manager,true,101)},error));service.commit();player.shutdown();CHECK(!service.update(error)&&!service.reserve({command(one,true,101)},error));service.shutdown();CHECK(allocated==0);for(auto owner:pool)CHECK(!owner);
 std::printf("Mechanism NDSP: %u checks; actual4PCM caches,independent tagged voices,restarts/stop-start/cancel/CRC/allocation failures,global channel outlives local scene,zero playback IO and unchanged BGM; explicit host SDK double, no audible/emulator/hardware claim\n",checks);
}
