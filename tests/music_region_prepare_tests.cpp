#include "music_region_service.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <vector>
using namespace encore;
namespace {
unsigned checks=0,mutations=0,mix_writes=0,owner_calls=0,file_opens=0,region_reads=0;
unsigned short_injections=0,error_injections=0;
size_t linear_bytes=0,read_bytes=0,max_read_request=0;int alloc_remaining=-1;
bool dsp=false,slow_reads=false,inject_short=false,inject_error=false,inject_corruption=false;
unsigned underruns=0;double old_frames_pending=0;uint64_t modeled_read_milliseconds=0;
std::set<FILE*>region_files;std::array<std::vector<ndspWaveBuf*>,20>queues;std::array<float,20>gain{};
void check(bool okay,const char*why){++checks;if(!okay){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
size_t channel(int i){check(i>=0&&i<20,"valid observed channel");++mutations;return size_t(i);}
void drain(int i){for(auto*p:queues[size_t(i)])p->status=NDSP_WBUF_DONE;queues[size_t(i)].clear();}
void modeled_slow_read(){
 if(!slow_reads)return;
 modeled_read_milliseconds+=45;old_frames_pending+=44100*.045;
 while(!queues[0].empty()&&old_frames_pending>=queues[0].front()->nsamples){old_frames_pending-=queues[0].front()->nsamples;queues[0].front()->status=NDSP_WBUF_DONE;queues[0].erase(queues[0].begin());}
 if(queues[0].empty()&&old_frames_pending>0){++underruns;old_frames_pending=0;}
}
}
extern "C" FILE*__real_fopen(const char*,const char*);
extern "C" size_t __real_fread(void*,size_t,size_t,FILE*);
extern "C" int __real_fclose(FILE*);
extern "C" int __real_ferror(FILE*);
extern "C" FILE*__wrap_fopen(const char*p,const char*m){++file_opens;FILE*f=__real_fopen(p,m);if(f&&std::strstr(p,"podunk-")&&std::strstr(p,".pcm"))region_files.insert(f);return f;}
extern "C" int __wrap_fclose(FILE*f){region_files.erase(f);return __real_fclose(f);}
extern "C" int __wrap_ferror(FILE*f){return inject_error&&region_files.count(f)?1:__real_ferror(f);}
namespace {
// glibc fortification may lower the same source fread to __fread_chk. Keep
// both entry points on the same observer and still call their real libc read.
template<class Read>size_t observed_read(void*p,size_t size,size_t count,FILE*f,Read read){
 if(!region_files.count(f))return read(count);
 ++region_reads;max_read_request=std::max(max_read_request,size*count);modeled_slow_read();
 if(inject_error){++error_injections;return 0;}
 if(inject_short)++short_injections;
 const size_t n=read(inject_short?count/2:count);if(inject_corruption&&n)static_cast<uint8_t*>(p)[0]^=1;read_bytes+=size*n;return n;
}
}
extern "C" size_t __wrap_fread(void*p,size_t size,size_t count,FILE*f){
 return observed_read(p,size,count,f,[&](size_t n){return __real_fread(p,size,n,f);});
}
#ifdef ENCORE_WRAP_FREAD_CHK
extern "C" size_t __fread_chk(void*,size_t,size_t,size_t,FILE*);
extern "C" size_t __real___fread_chk(void*,size_t,size_t,size_t,FILE*);
extern "C" size_t __wrap___fread_chk(void*p,size_t bytes,size_t size,size_t count,FILE*f){
 return observed_read(p,size,count,f,[&](size_t n){return __real___fread_chk(p,bytes,size,n,f);});
}
#endif
Result ndspInit(){++owner_calls;dsp=true;return 0;}void ndspExit(){++owner_calls;for(int i=0;i<20;++i)drain(i);dsp=false;}
void ndspSetMasterVol(float){++owner_calls;}uint32_t ndspGetDroppedFrames(){return 0;}
void*linearAlloc(size_t n){if(alloc_remaining==0)return nullptr;if(alloc_remaining>0)--alloc_remaining;linear_bytes+=n;return std::malloc(n);}
void linearFree(void*p){linear_bytes-=24576;std::free(p);}
void ndspChnReset(int i){channel(i);drain(i);}void ndspChnWaveBufClear(int i){channel(i);drain(i);}
void ndspChnSetMix(int i,float*v){channel(i);++mix_writes;gain[size_t(i)]=v[0];check(v[0]==v[1],"stereo equal");}
Result DSP_FlushDataCache(const void*p,uint32_t n){check(p&&n&&n<=8192,"bounded streaming payload");return 0;}
void ndspChnWaveBufAdd(int i,ndspWaveBuf*p){check(dsp,"real owner lifetime");p->status=NDSP_WBUF_QUEUED;queues[channel(i)].push_back(p);}
void ndspChnSetInterp(int i,int){channel(i);}void ndspChnSetRate(int i,float){channel(i);}void ndspChnSetFormat(int i,int){channel(i);}
int main(int argc,char**argv){
 check(argc==6,"opening bank/root and region pack/bank/root");std::string e;ctr::AudioPlayer owner;ctr::MusicRegionService service;
#ifdef ENCORE_WRAP_FREAD_CHK
 // Exercise the checked libc endpoint even when a compiler can prove that a
 // particular production read fits and optimizes it back to ordinary fread.
 FILE*probe=std::tmpfile();check(probe!=nullptr,"checked-read injection probe opened");
 const std::array<unsigned char,8>payload{{1,2,3,4,5,6,7,8}};
 check(std::fwrite(payload.data(),1,payload.size(),probe)==payload.size(),"checked-read probe initialized");std::rewind(probe);region_files.insert(probe);
 std::array<unsigned char,8>observed{};const auto probe_injections=short_injections;inject_short=true;
 const auto probe_read=__fread_chk(observed.data(),observed.size(),1,observed.size(),probe);inject_short=false;
 check(probe_read==4&&short_injections==probe_injections+1&&!std::feof(probe),"checked-read endpoint injects a real non-EOF short read");
 check(std::equal(observed.begin(),observed.begin()+4,payload.begin()),"checked-read endpoint preserves real read bytes");
 check(std::fclose(probe)==0&&region_files.empty(),"checked-read probe closed");
#endif
 check(owner.initialize(argv[1],argv[2],e)&&owner.play(34,ctr::AudioLane::Music,e)&&owner.fade_music(2,e),e.c_str());
 const auto generation=owner.observe_music().generation,base_linear=linear_bytes;const auto start_gain=gain[0];
 auto begin=[&](){auto before=mutations+owner_calls;auto before_region_reads=region_reads;check(service.begin_prepare(argv[3],argv[4],argv[5],8,owner,e),e.c_str());check(service.phase()==ctr::MusicRegionServicePhase::Preparing&&region_reads==before_region_reads&&mutations+owner_calls==before,"begin only reads bounded metadata, no PCM/NDSP");check(service.total_pcm_bytes()==77419288,"actual checked source PCM byte count");};
 begin();check(!service.commit_scene(1,owner,e),"unvalidated candidate cannot commit");
 auto before=mutations+owner_calls,opens=file_opens;check(service.update(100,owner,e)&&mutations+owner_calls==before&&file_opens==opens,"Preparing update never drives IO or game/audio time");
 for(uint32_t budget:{1u,7u,8192u,65536u}){const auto bytes=read_bytes;const auto ndsp=mutations+owner_calls;check(service.prepare_step(budget,e)==ctr::MusicPreparationStep::Progress,"bounded prepare step progresses");check(read_bytes-bytes<=budget&&mutations+owner_calls==ndsp,"step honors total byte budget and does no NDSP");}
 check(service.cancel_preparation(e)&&region_files.empty()&&linear_bytes==base_linear&&owner.observe_music().generation==generation&&owner.observe_music().playing,"cancel closes all candidate files and preserves old voice");
 begin();before=mutations+owner_calls;check(service.prepare_step(0,e)==ctr::MusicPreparationStep::Failed&&service.phase()==ctr::MusicRegionServicePhase::Dormant&&mutations+owner_calls==before,"zero budget fails explicitly and rolls back candidate");
 begin();before=mutations+owner_calls;const auto short_before=short_injections;inject_short=true;const auto short_result=service.prepare_step(8192,e);inject_short=false;
 check(short_injections==short_before+1,"short-read fault actually reached the PCM read");
 check(short_result==ctr::MusicPreparationStep::Failed&&region_files.empty()&&mutations+owner_calls==before,"unexpected short read without EOF rejected with rollback");
 begin();before=mutations+owner_calls;const auto error_before=error_injections;inject_error=true;const auto error_result=service.prepare_step(8192,e);inject_error=false;
 check(error_injections==error_before+1,"read-error fault actually reached the PCM read");
 check(error_result==ctr::MusicPreparationStep::Failed&&region_files.empty()&&mutations+owner_calls==before,"read error rejected with rollback");
 begin();before=mutations+owner_calls;check(service.prepare_step(65537,e)==ctr::MusicPreparationStep::Failed&&service.phase()==ctr::MusicRegionServicePhase::Dormant&&mutations+owner_calls==before,"over-limit byte budget rejected without touching old queues");
 begin();before=mutations+owner_calls;inject_corruption=true;auto corrupt=ctr::MusicPreparationStep::Progress;unsigned corrupt_steps=0;
 while(corrupt==ctr::MusicPreparationStep::Progress&&corrupt_steps++<3000)corrupt=service.prepare_step(8192,e);
 inject_corruption=false;check(corrupt==ctr::MusicPreparationStep::Failed&&e=="Region PCM length/checksum/read validation failed"&&region_files.empty()&&mutations+owner_calls==before,"incremental CRC detects read-byte corruption and rolls back only the candidate");
 // Virtual45ms per region read models a slow storage source while the old
 //44.1kHz queue consumes. This is deliberately not a real hardware benchmark.
 begin();slow_reads=true;max_read_request=0;read_bytes=0;const auto mix_before=mix_writes;
 unsigned steps=0;ctr::MusicPreparationStep result=ctr::MusicPreparationStep::Progress;
 while(result==ctr::MusicPreparationStep::Progress&&steps++<20000){const auto bytes=read_bytes;result=ctr::pump_region_music_preparation(service,owner,65536,e);check(result!=ctr::MusicPreparationStep::Failed,e.c_str());check(read_bytes-bytes<=8192,"cooperative step has only one bounded8KiB region read");}
 slow_reads=false;
 check(result==ctr::MusicPreparationStep::Ready&&service.phase()==ctr::MusicRegionServicePhase::Prepared&&service.prepared_pcm_bytes()==service.total_pcm_bytes()&&read_bytes==77419288,"full real PCM validation reaches Ready with exact length/CRC");
 check(underruns==0&&modeled_read_milliseconds>400000,"old queue stayed fed under explicit slow-read model");
 check(max_read_request<=8192&&mix_writes==mix_before&&gain[0]==start_gain&&owner.observe_music().tweening&&owner.observe_music().generation==generation,"stream pumping changes no gain, fade progress or identity");
 check(service.buffer_bytes()==196608&&linear_bytes==base_linear+196608,"voice buffers allocated after validation");
 check(service.cancel_preparation(e)&&region_files.empty()&&linear_bytes==base_linear&&!queues[0].empty(),"cancel Ready before commit preserves currently audible owner queues");
 // Allocation failure occurs only after complete validation. At most one voice
 // buffer is allocated per call; a failed second allocation releases the first.
 begin();result=ctr::MusicPreparationStep::Progress;alloc_remaining=1;unsigned iterations=0;before=mutations+owner_calls;
 while(result==ctr::MusicPreparationStep::Progress&&iterations++<20000)result=service.prepare_step(8192,e);
 alloc_remaining=-1;
 check(result==ctr::MusicPreparationStep::Failed&&e.find("allocation failed")!=e.npos&&service.phase()==ctr::MusicRegionServicePhase::Dormant&&region_files.empty()&&linear_bytes==base_linear&&mutations+owner_calls==before,"partial allocation failure releases candidate and does no old NDSP work");
 // The public pump also reports real natural completion without game ticks.
 owner.reset_scene();check(owner.play(1001,ctr::AudioLane::Music,e),"one-shot fixture for natural completion");const auto one_shot=owner.observe_music().generation;unsigned drains=0;while(owner.observe_music().playing&&drains++<200){drain(0);check(owner.pump_streams(e),"old one-shot refill only");}
 check(!owner.observe_music().playing&&owner.observe_music().generation==one_shot,"refill-only observes natural finish without minting identity");
 service.shutdown();owner.shutdown();check(linear_bytes==0&&region_files.empty(),"all resources released before owner exit");
 std::cout<<"Incremental music preparation PASS "<<checks<<" checks; "<<steps<<" steps, modeled read ms="<<modeled_read_milliseconds<<", modeled underruns="<<underruns<<", maximum read request="<<max_read_request<<"; no hardware timing claim\n";
}
