#include "music_region_player.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace encore;
namespace {unsigned checks=0,mutations=0;int alloc_left=-1;size_t allocated=0;Result flush_result=0;
std::array<std::vector<ndspWaveBuf*>,20>queues;std::array<float,20>gains{};
void check(bool x,const char*m){++checks;if(!x){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
size_t ch(int c){check(c>=4&&c<20,"existing music/SFX/jingle/dialogue channels never touched");++mutations;return size_t(c);}
void drain(int c){auto&q=queues[ch(c)];for(auto*b:q)b->status=NDSP_WBUF_DONE;q.clear();}
uint32_t checksum(const int16_t*p,size_t n){return upstream::audio_crc32(reinterpret_cast<const uint8_t*>(p),n*2);}
}
Result ndspInit(){check(false,"adapter must not initialize owner DSP");return 0;}
void ndspExit(){check(false,"adapter must not exit owner DSP");}
void ndspSetMasterVol(float){check(false,"adapter must not change master gain");}
uint32_t ndspGetDroppedFrames(){return 0;}
void*linearAlloc(size_t n){if(alloc_left==0)return nullptr;if(alloc_left>0)--alloc_left;allocated+=n;return std::malloc(n);}
void linearFree(void*p){check(p!=nullptr,"valid release");allocated-=3*2048*4;std::free(p);}
void ndspChnReset(int c){drain(c);}void ndspChnWaveBufClear(int c){drain(c);}
void ndspChnSetMix(int c,float*v){gains[ch(c)]=v[0];check(v[0]==v[1],"equal stereo gain");}
Result DSP_FlushDataCache(const void*p,uint32_t n){check(p&&n>0&&n<=2048*4,"bounded streaming flush");return flush_result;}
void ndspChnWaveBufAdd(int c,ndspWaveBuf*b){auto&q=queues[ch(c)];check(b->data_pcm16&&b->nsamples==2048&&!b->looping,"source streamed frame buffer");b->status=NDSP_WBUF_QUEUED;q.push_back(b);}
void ndspChnSetInterp(int c,int){ch(c);}void ndspChnSetRate(int c,float){ch(c);}void ndspChnSetFormat(int c,int){ch(c);}
// Test-only completion driver retains each original adapter case while
// exercising the production incremental API; this is not a runtime wrapper.
bool prepare_all(ctr::MusicRegionPlayer& player,const char* bank,const char* root,
                 const upstream::MusicRegionData& data,uint32_t capacity,
                 bool available,float master,std::string& error){
 if(!player.begin_prepare(bank,root,data,capacity,available,master,error))return false;
 for(unsigned i=0;i<20000;++i){
  const auto step=player.prepare_step(8192,error);
  if(step==ctr::MusicPreparationStep::Ready)return true;
  if(step==ctr::MusicPreparationStep::Failed)return false;
 }
 check(false,"bounded preparation must terminate for frozen fixture");return false;
}
int main(int argc,char**argv){check(argc==4,"region+audio+root arguments");upstream::MusicRegionData data;std::string e;check(data.load_file(argv[1],e),e.c_str());upstream::AudioBank bank;check(bank.load_file(argv[2],e),e.c_str());
 ctr::MusicRegionPlayer unavailable;check(!prepare_all(unavailable,argv[2],argv[3],data,8,false,bank.master_db(),e)&&mutations==0&&allocated==0,"unavailable preserves existing backend without init retry");
 ctr::MusicRegionPlayer failed;alloc_left=2;check(!prepare_all(failed,argv[2],argv[3],data,8,true,bank.master_db(),e)&&mutations==0&&allocated==0,"partial allocation rollback never touches existing channels");alloc_left=-1;
 ctr::MusicRegionPlayer audio;check(prepare_all(audio,argv[2],argv[3],data,8,true,bank.master_db(),e),e.c_str());check(mutations==0&&allocated==8*24576&&audio.buffer_bytes()==allocated,"prepare only checks streams and reserves192KiB, silent");
 upstream::MusicRegionController c;upstream::MusicRegionContext context;context.flag=[](std::string_view){return false;};check(c.initialize(data,8,e)&&c.attach_scene(1,e)&&c.enter(1,"Music/MusicArea",context,e)&&audio.sync(c,e),e.c_str());check(audio.submitted_voices()==1&&queues[4].size()==3,"region starts streaming");const auto first=checksum(queues[4][0]->data_pcm16,4096);auto generation=c.voices()[0].generation;
 check(c.enter(1,"Music/MusicArea3",context,e)&&c.advance(.5,false,e)&&audio.sync(c,e),"same track lower-gain area");check(audio.submitted_voices()==1&&c.voices()[0].generation==generation&&std::abs(gains[4]-upstream::audio_linear_gain(-4))<1e-5,"fadeto keeps queued voice and cursor");
 check(c.exit(1,"Music/MusicArea",context,e)&&c.exit(1,"Music/MusicArea3",context,e)&&c.idle_frame(1,e)&&c.enter(1,"Music/MusicArea6",context,e)&&audio.sync(c,e),"overlap crossfade retains old track");check(queues[4].size()==3&&queues[5].size()==3,"both music voices concurrently queued");
 check(c.exit(1,"Music/MusicArea6",context,e)&&c.idle_frame(1,e)&&c.enter(1,"Music/MusicArea",context,e)&&audio.sync(c,e),"rapid return duplicate original loop");check(audio.submitted_voices()==3&&queues[6].size()==3,"independent same-track duplicate voice supported");check(checksum(queues[6][0]->data_pcm16,4096)==first,"new duplicate starts at source frame zero");drain(4);check(audio.sync(c,e),"old same-track cursor refilled");check(checksum(queues[4][0]->data_pcm16,4096)!=first,"old voice phase continues independently");
 upstream::MusicRegionController wrong;check(wrong.initialize(data,1,e)&&wrong.attach_scene(2,e),"wrong capacity fixture");auto old=queues;check(!audio.sync(wrong,e)&&queues==old,"bad snapshot capacity cannot stop old audio");
 check(c.advance(2,false,e)&&audio.sync(c,e),"completed fades clear only old region channels");check(queues[4].empty()&&queues[5].empty()&&queues[6].size()==3,"current duplicate survives old fade cleanup");
 // Detached candidates can prepare without taking the live range; accidental
 // activation fails before a single old channel is altered.
 ctr::MusicRegionPlayer candidate;auto old_queues=queues;auto old_mutations=mutations;
 check(prepare_all(candidate,argv[2],argv[3],data,1,true,bank.master_db(),e)&&mutations==old_mutations,"detached replacement preparation leaves live region audio intact");
 check(!candidate.sync(wrong,e)&&e=="Music region channels already owned; existing audio preserved"&&queues==old_queues&&mutations==old_mutations,"second adapter cannot steal the reserved music range");
 candidate.shutdown();check(queues==old_queues&&mutations==old_mutations,"discarded candidate cannot clear live channels");
 flush_result=-1;drain(6);check(!audio.sync(c,e)&&e=="Region DSP cache flush failed"&&queues[6].empty(),"stream failure explicit and limited to affected voice");flush_result=0;check(audio.sync(c,e)&&queues[6].size()==3,"authorized retry refills affected region");
 audio.shutdown();check(allocated==0,"region streaming buffers all released");for(size_t i=4;i<queues.size();++i)check(queues[i].empty(),"region shutdown clear queues before buffers");check(mutations>0,"double exercised adapter");std::cout<<"Music region NDSP-double PASS "<<checks<<" checks; no real audio output claim\n";
}
