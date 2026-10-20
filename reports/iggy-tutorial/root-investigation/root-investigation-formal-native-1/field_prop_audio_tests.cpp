// Actual source PCM and production service; explicit NDSP/API double.
#include "field_prop_audio.hpp"
#include "encore/content.hpp"
#include <filesystem>
#include <fstream>
#include <map>
#include <cmath>
#include <cstdio>
using namespace encore;
namespace {
unsigned checks=0;std::string error;bool fail_alloc=false;Result flush_result=0;size_t allocated=0;std::map<void*,size_t>allocations;
std::array<std::vector<ndspWaveBuf*>,24>queues;std::array<float,24>gains{};std::array<unsigned,24>resets{};
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Prop audio %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
size_t channel(int c){CHECK(c>=0&&c<24);return size_t(c);}
void complete(int c){auto&q=queues[channel(c)];CHECK(!q.empty());q.front()->status=NDSP_WBUF_DONE;q.erase(q.begin());}
void drain(int c){auto&q=queues[channel(c)];for(auto*w:q)w->status=NDSP_WBUF_DONE;q.clear();}
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
 CHECK(argc==6);const std::string bank_path=argv[1],base_root=argv[2],packet=argv[3],pcm_root=argv[4];const std::filesystem::path out=argv[5];upstream::FieldPropsData data;CHECK(data.load_file(packet.c_str(),error)&&data.contact_bound());ctr::AudioPlayer player;CHECK(player.initialize(bank_path.c_str(),base_root.c_str(),error));upstream::AudioAsset sound;CHECK(data.matches_audio(player.bank(),sound,error));std::vector<uint8_t>pcm;CHECK(read_file((std::filesystem::path(pcm_root)/sound.pcm_path).string().c_str(),pcm,512*1024,error));const auto target=out/sound.pcm_path;write(target,pcm);
 CHECK(player.play(21,ctr::AudioLane::Music,error));CHECK(player.prepare_player_voice(error));const auto music=player.observe_music();const auto music_queue=queues[0];const auto allocated_before=allocated;auto&pool=ctr::region_channel_pool::owners();const auto prior_pool=pool;
 ctr::FieldPropAudioService service;CHECK(!service.matches(data,error));CHECK(!service.prepare(data,player,nullptr,error)&&!service.prepared());
 fail_alloc=true;CHECK(!service.prepare(data,player,out.string().c_str(),error)&&!service.prepared()&&allocated==allocated_before);fail_alloc=false;
 flush_result=-7;CHECK(!service.prepare(data,player,out.string().c_str(),error)&&!service.prepared()&&allocated==allocated_before&&pool==prior_pool&&queues[0]==music_queue);flush_result=0;
 write(target,std::vector<uint8_t>(pcm.size(),0));CHECK(!service.prepare(data,player,out.string().c_str(),error)&&!service.prepared()&&allocated==allocated_before);write(target,pcm);
 CHECK(service.prepare(data,player,out.string().c_str(),error)&&service.matches(data,error));CHECK(!service.prepare(data,player,out.string().c_str(),error));CHECK(allocated==allocated_before+pcm.size());
 // The checked cache, not a filesystem reopen, serves every hit and refill.
 CHECK(std::filesystem::remove(target));CHECK(service.update(error)&&service.active_nodes()==0&&service.starts()==0);
 CHECK(!service.prepare_hits(data,{},error)&&!service.prepare_hits(data,{0,0},error)&&!service.prepare_hits(data,{128},error));
 static int foreign;for(auto&owner:pool)if(!owner)owner=&foreign;const auto full=pool;CHECK(!service.prepare_hits(data,{0,1},error)&&pool==full&&service.starts()==0&&queues[0]==music_queue);for(auto&owner:pool)if(owner==&foreign)owner=nullptr;CHECK(pool==prior_pool);
 CHECK(service.prepare_hits(data,{0,1},error)&&service.active_nodes()==0);CHECK(!service.prepare_hits(data,{2},error));service.commit_hit(0);service.commit_hit(1);CHECK(service.starts()==2&&service.active_nodes()==2&&queues[0]==music_queue);int a=-1,b=-1;for(unsigned i=0;i<pool.size();++i)if(pool[i]&&pool[i]!=prior_pool[i]){if(a<0)a=int(i+ctr::region_channel_pool::first_channel);else b=int(i+ctr::region_channel_pool::first_channel);}CHECK(a>=4&&b>a&&queues[a].size()==3&&queues[b].size()==3);CHECK(queues[a][0]!=queues[b][0]&&queues[a][0]->data_pcm16==queues[b][0]->data_pcm16);const auto untouched=queues[b];complete(a);CHECK(service.pump_streams(error)&&queues[b]==untouched&&queues[a][0]->data_pcm16!=queues[b][0]->data_pcm16);
 const auto other_resets=resets[b];CHECK(service.prepare_hits(data,{0},error));service.commit_hit(0);CHECK(service.starts()==3&&service.active_nodes()==2&&resets[b]==other_resets&&queues[b]==untouched&&queues[0]==music_queue);CHECK(player.set_user_volume(0,0,-6,error)&&service.update(error));CHECK(std::fabs(gains[a]-upstream::audio_linear_gain(sound.gain_db+data.policy().contact.gain_db-6))<.000001f);CHECK(player.observe_music().generation==music.generation&&player.observe_music().asset_id==music.asset_id&&queues[0]==music_queue);
 service.stop_deleted(0);CHECK(service.active_nodes()==1&&queues[a].empty()&&queues[b]==untouched);for(unsigned t=0;t<128&&service.active_nodes();++t){for(int c=4;c<20;++c)if(!queues[c].empty()&&pool[size_t(c-4)]!=prior_pool[size_t(c-4)])complete(c);CHECK(service.pump_streams(error));}CHECK(service.active_nodes()==0&&pool==prior_pool&&queues[0]==music_queue);service.shutdown();CHECK(!service.prepared()&&allocated==allocated_before&&pool==prior_pool);
 // An abandoned all-or-nothing reservation retires only its own tags.
 write(target,pcm);CHECK(service.prepare(data,player,out.string().c_str(),error)&&service.prepare_hits(data,{0,1},error));service.shutdown();CHECK(allocated==allocated_before&&pool==prior_pool&&queues[0]==music_queue);
 // Reconfiguration invalidates an old node owner before any further hit.
 CHECK(service.prepare(data,player,out.string().c_str(),error));player.shutdown();CHECK(!service.prepare_hits(data,{0},error));service.shutdown();CHECK(allocated==0);for(auto owner:pool)CHECK(!owner);
 std::printf("Field prop source audio: %u checks; actual source PCM/cache CRC, independent node cursors/restart/deletion, bounded tagged channels, no hit/refill file IO, transactional failures and unchanged BGM; explicit NDSP double, no audibility/emulator/hardware claim\n",checks);
}
